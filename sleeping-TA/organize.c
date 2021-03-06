/*
 * Matthew Leeds, CS 426, 2015-10-02, OS Concepts Ch.5 Project 1
 * This program uses process synchronization mechanisms available
 * in pthreads to coordinate students getting help from a TA who
 * sleeps when they're not busy.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <semaphore.h>

// how many times students alternate between programming and seeking help
#define NUM_ITERATIONS 10

pthread_mutex_t help_mutex; // locked when the TA is helping someone
pthread_mutex_t count_mutex; // protects number_waiting changes
sem_t TA_sem; // 0 when the TA is sleeping
sem_t student_sem; // -1 * the nmuber of students waiting
int number_waiting; // the number of students waiting
int go_home; // the main thread sets this to 1 to send the TA home

// this will be passed to each new student thread
typedef struct {
  long nsecSleep;
  int numberOfIterations;
} sleepAndCount;

// to be run by their respective threads
void* simulate_student(void* param);
void* simulate_TA(void* param);

int main(int argc, char** argv) {
  // input validation
  if (argc != 2) {
    printf("Usage: ./organize <number of students>\n");
    return 1;
  }
  int numberOfStudents = atoi(argv[1]);
  if (numberOfStudents <= 0) {
    printf("Error: Number of students must be a positive integer.\n");
    return 2;
  }
  
  // initialize things
  sem_init(&TA_sem, 0, 0);
  sem_init(&student_sem, 0, 0);
  number_waiting = 0;
  go_home = 0;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_mutex_init(&help_mutex, NULL);
  pthread_mutex_init(&count_mutex, NULL);

  // make a thread for the TA
  pthread_t tid_TA;
  pthread_create(&tid_TA, &attr, simulate_TA, NULL);

  // make a thread for each student
  pthread_t* tid_students[numberOfStudents];
  sleepAndCount* params[numberOfStudents];
  int i;
  for (i = 0; i < numberOfStudents; ++i) {
    srand(time(NULL) + i);
    tid_students[i] = malloc(sizeof(pthread_t));
    params[i] = malloc(sizeof(sleepAndCount));
    params[i]->nsecSleep = (long) rand() % 1000000000;
    params[i]->numberOfIterations = NUM_ITERATIONS;
    pthread_create(tid_students[i], &attr, simulate_student, params[i]);
  }
  
  // join the threads and free memory
  for (i = 0; i < numberOfStudents; ++i) {
    pthread_join(*tid_students[i], NULL);
    free(tid_students[i]);
    free(params[i]);
  }
  go_home = 1; // tell the TA to go home
  sem_post(&TA_sem); // wake up the TA if necessary
  pthread_join(tid_TA, NULL);
  pthread_attr_destroy(&attr);
  pthread_mutex_destroy(&help_mutex);
  pthread_mutex_destroy(&count_mutex);
  sem_destroy(&TA_sem);
  sem_destroy(&student_sem);

  return 0;
}

void* simulate_TA(void* param) {
  srand(time(NULL) / 2);
  struct timespec time;
  time.tv_sec = 0;
  time.tv_nsec = (long) rand() % 1000000000;
  while (1) {
    printf("tid = 0x%08x, TA sleeping, number_waiting = %d\n",
           (unsigned)pthread_self(), number_waiting);
    sem_wait(&TA_sem); // sleep
    if (go_home) break; // main thread woke us up
    printf("tid = 0x%08x, TA awake, number_waiting = %d\n",
           (unsigned)pthread_self(), number_waiting);
    // help a student
    pthread_mutex_lock(&help_mutex);
    sem_post(&student_sem);
    // decrement number_waiting
    pthread_mutex_lock(&count_mutex);
    number_waiting--;
    pthread_mutex_unlock(&count_mutex);
    // help for a time
    printf("tid = 0x%08x, TA helping student, number_waiting = %d\n",
           (unsigned)pthread_self(), number_waiting);
    nanosleep(&time, NULL);
    pthread_mutex_unlock(&help_mutex);
  }
  pthread_exit(0);
}

void* simulate_student(void* param) {
  struct timespec time;
  time.tv_sec = 0;
  time.tv_nsec = ((sleepAndCount*)param)->nsecSleep;
  int count = ((sleepAndCount*)param)->numberOfIterations;
  int i;
  for (i = 0; i < count; ++i) {
    // program for a time
    printf("tid = 0x%08x, student programming, number_waiting = %d\n",
           (unsigned)pthread_self(), number_waiting);
    nanosleep(&time, NULL);
    // if all the chairs are full, keep programming
    pthread_mutex_lock(&count_mutex);
    if (number_waiting >= 3) {
      printf("tid = 0x%08x, student waiting for chair, number_waiting = %d\n",
             (unsigned)pthread_self(), number_waiting);
      pthread_mutex_unlock(&count_mutex);
    } else {
      number_waiting++;
      pthread_mutex_unlock(&count_mutex);
      // wake up the TA if necessary
      sem_post(&TA_sem);
      // wait in line
      printf("tid = 0x%08x, student waiting for TA, number_waiting = %d\n",
             (unsigned)pthread_self(), number_waiting);
      sem_wait(&student_sem);
      // when we can lock help_mutex it means they're finished helping us
      pthread_mutex_lock(&help_mutex);
      pthread_mutex_unlock(&help_mutex);
    }
  }
  pthread_exit(0);
}
