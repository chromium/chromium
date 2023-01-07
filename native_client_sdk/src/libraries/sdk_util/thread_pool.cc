// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sdk_util/thread_pool.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#include "sdk_util/auto_lock.h"

namespace sdk_util {

#ifdef __APPLE__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

// Initializes mutex, semaphores and a pool of threads.  If 0 is passed for
// num_threads, all work will be performed on the dispatch thread.
ThreadPool::ThreadPool(int num_threads)
    : threads_(NULL), counter_(0), num_threads_(num_threads), exiting_(false),
      user_data_(NULL), user_work_function_(NULL) {
  if (num_threads_ > 0) {
    int status;
    status = sem_init(&work_sem_, 0, 0);
    if (-1 == status) {
      fprintf(stderr, "Failed to initialize semaphore!\n");
      exit(-1);
    }
    status = sem_init(&done_sem_, 0, 0);
    if (-1 == status) {
      fprintf(stderr, "Failed to initialize semaphore!\n");
      exit(-1);
    }
    threads_ = new pthread_t[num_threads_];
    for (int i = 0; i < num_threads_; i++) {
      status = pthread_create(&threads_[i], NULL, WorkerThreadEntry, this);
      if (0 != status) {
        fprintf(stderr, "Failed to create thread!\n");
        exit(-1);
      }
    }
  }
}

// Post exit request, wait for all threads to join, and cleanup.
ThreadPool::~ThreadPool() {
  if (num_threads_ > 0) {
    PostExitAndJoinAll();
    delete[] threads_;
    sem_destroy(&done_sem_);
    sem_destroy(&work_sem_);
  }
}

// Setup work parameters.  This function is called from the dispatch thread,
// when all worker threads are sleeping.
void ThreadPool::Setup(int counter, WorkFunction work, void *data) {
  counter_ = counter;
  user_work_function_ = work;
  user_data_ = data;
}

// Return decremented task counter.  This function
// can be called from multiple threads at any given time.
int ThreadPool::DecCounter() {
  return AtomicAddFetch(&counter_, -1);
}

// Set exit flag, post and join all the threads in the pool.  This function is
// called only from the dispatch thread, and only when all worker threads are
// sleeping.
void ThreadPool::PostExitAndJoinAll() {
  exiting_ = true;
  // Wake up all the sleeping worker threads.
  for (int i = 0; i < num_threads_; ++i)
    sem_post(&work_sem_);
  void* retval;
  for (int i = 0; i < num_threads_; ++i)
    pthread_join(threads_[i], &retval);
}

// Main work loop - one for each worker thread.
void ThreadPool::WorkLoop() {
  while (true) {
    // Wait for work. If no work is available, this thread will sleep here.
    sem_wait(&work_sem_);
    if (exiting_) break;
    while (true) {
      // Grab a task index to work on from the counter.
      int task_index = DecCounter();
      if (task_index < 0)
        break;
      user_work_function_(task_index, user_data_);
    }
    // Post to dispatch thread work is done.
    sem_post(&done_sem_);
  }
}

// pthread entry point for a worker thread.
void* ThreadPool::WorkerThreadEntry(void* thiz) {
  static_cast<ThreadPool*>(thiz)->WorkLoop();
  return NULL;
}

// DispatchMany() will dispatch a set of tasks across worker threads.
// Note: This function will block until all work has completed.
void ThreadPool::DispatchMany(int num_tasks, WorkFunction work, void* data) {
  // On entry, all worker threads are sleeping.
  Setup(num_tasks, work, data);

  // Wake up the worker threads & have them process tasks.
  for (int i = 0; i < num_threads_; i++)
    sem_post(&work_sem_);

  // Worker threads are now awake and busy.

  // This dispatch thread will now sleep-wait for the worker threads to finish.
  for (int i = 0; i < num_threads_; i++)
    sem_wait(&done_sem_);
  // On exit, all tasks are done and all worker threads are sleeping again.
}

#ifdef __APPLE__
#pragma clang diagnostic pop
#endif

//  DispatchHere will dispatch all tasks on this thread.
void ThreadPool::DispatchHere(int num_tasks, WorkFunction work, void* data) {
  for (int i = 0; i < num_tasks; i++)
    work(i, data);
}

// Dispatch() will invoke the user supplied work function across
// one or more threads for each task.
// Note: This function will block until all work has completed.
void ThreadPool::Dispatch(int num_tasks, WorkFunction work, void* data) {
  if (num_threads_ > 0)
    DispatchMany(num_tasks, work, data);
  else
    DispatchHere(num_tasks, work, data);
}

}  // namespace sdk_util

