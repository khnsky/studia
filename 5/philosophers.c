#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ipc.h>
#include <sys/sem.h>

#include <err.h>

#include <pthread.h>

#include <unistd.h>

static char const* const PHILOSOPHERS[] = {
    "Plato",
    "Confucius",
    "Socrates",
    "Voltaire",
    "Descartes",
};

static int FORKS[] = {
    0, 1, 2, 3, 4,
};

#define LEN(x) (sizeof(x) / sizeof(x[0]))
#ifndef MEALS
#   define MEALS 5
#endif

static int SEMID;

static inline void grab_forks(int const left) {
    int const right = left + 1 % LEN(PHILOSOPHERS);

    printf("%s is trying to grab forks: %d and %d\n",
           PHILOSOPHERS[left], left, right);

    /*
     * in order to avoid deadlock even philosophers pick right fork first,
     * while odd start with left fork
     */
    if (left & 1)
        semop(
            SEMID,
            (struct sembuf[]) {
                { .sem_num = left,  .sem_op = -1, .sem_flg = 0 },
                { .sem_num = right, .sem_op = -1, .sem_flg = 0 },
            },
            2
        );
    else
        semop(
            SEMID,
            (struct sembuf[]) {
                { .sem_num = right, .sem_op = -1, .sem_flg = 0 },
                { .sem_num = left,  .sem_op = -1, .sem_flg = 0 },
            },
            2
        );

    printf("%s grabbed forks: %d, %d\n",
           PHILOSOPHERS[left], left, right);
}

static inline void put_away_forks(int const left) {
    int const right = left + 1 % LEN(PHILOSOPHERS);

    printf("%s is putting away forks: %d and %d\n",
           PHILOSOPHERS[left], left, right);
    semop(
        SEMID,
        (struct sembuf[2]) {
            { .sem_num = left,  .sem_op = 1, .sem_flg = 0 },
            { .sem_num = right, .sem_op = 1, .sem_flg = 0 },
        },
        2
    );
}

void think(int philosopher) {
    printf("%s is thinking\n", PHILOSOPHERS[philosopher]);
    sleep(1);
}

void eat(int philosopher, int meal) {
    printf("%s is eating for %d time\n", PHILOSOPHERS[philosopher], meal);
    sleep(1);
}

void* philosopher(void* id) {
    for (int i = 0; i < MEALS; ++i) {
        think(*(int*)id);
        grab_forks(*(int*)id);
        eat(*(int*)id, i + 1);
        put_away_forks(*(int*)id);
    }
    return NULL;
}

int main(void) {
    /* create LEN(PHILOSOPHERS) semaphores with rights 0666 */
    SEMID = semget(IPC_PRIVATE, LEN(PHILOSOPHERS), 0666);

    /* check if created successfully */
    if (SEMID < 0)
        err(EXIT_FAILURE, "failed to create semaphores");


    /* initialize all semaphores to 1 - all forks are free initially */
    for (int i = 0; i < (int)LEN(PHILOSOPHERS); ++i)
        if (semctl(SEMID, i, SETVAL, 1) < 0)
            err(EXIT_FAILURE, "failed to initialize semaphore %d", i);

    /* create LEN(PHILOSOPHERS) threads and run philosopher on them */
    pthread_t threads[LEN(PHILOSOPHERS)];
    for (int i = 0; i < (int)LEN(threads); ++i)
        pthread_create(&threads[i], NULL, philosopher, &FORKS[i]);

    /* join created threads */
    for (int i = 0; i < (int)LEN(threads); ++i)
        pthread_join(threads[i], NULL);

    /* remove the allocated semaphores */
    if (semctl(SEMID, LEN(PHILOSOPHERS), IPC_RMID) < 0)
        err(EXIT_FAILURE, "failed to deallocate semaphores");
}
