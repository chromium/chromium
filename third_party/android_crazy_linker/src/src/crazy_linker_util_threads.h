// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_UTIL_THREADS_H
#define CRAZY_LINKER_UTIL_THREADS_H

#include <pthread.h>

#include "crazy_linker_macros.h"

// Convenience classes for managing and synchronizing threads.
// Reminder: the crazy linker cannot use std::thread and other C++ library
// features at all.

namespace crazy {

// Small abstraction for simple (non-recursive) mutexes.
class Mutex {
 public:
  Mutex() = default;
  ~Mutex() { pthread_mutex_destroy(&mutex_); }
  void Lock() { pthread_mutex_lock(&mutex_); }
  void Unlock() { pthread_mutex_unlock(&mutex_); }

  // Futexes cannot be copied or moved to different addresses.
  CRAZY_DISALLOW_COPY_AND_MOVE_OPERATIONS(Mutex)

 private:
  friend class Condition;
  pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;
};

// Scoped auto-lock convenience class.
// Locks the mutex on construction, releases it on destruction unless
// Release() was called before.
class AutoLock {
 public:
  // Constructor takes pointer to Mutex instance and locks it.
  AutoLock() = delete;
  explicit AutoLock(Mutex* mutex) : mutex_(mutex) { mutex_->Lock(); }
  // Destructor unlocks it if it wasn't released yet.
  ~AutoLock() {
    if (mutex_)
      mutex_->Unlock();
  }

  CRAZY_DISALLOW_COPY_AND_MOVE_OPERATIONS(AutoLock);

 private:
  Mutex* mutex_ = nullptr;
};

// Small abstraction for condition variables.
class Condition {
 public:
  // Constructor takes pointer to associated mutex.
  Condition() = delete;
  explicit Condition(Mutex* mutex) : mutex_(mutex) {}
  ~Condition() { pthread_cond_destroy(&cond_); }
  void Signal() { pthread_cond_signal(&cond_); }
  void Wait() { pthread_cond_wait(&cond_, &mutex_->mutex_); }

  // Futexes cannot be copied or moved to different addresses.
  CRAZY_DISALLOW_COPY_AND_MOVE_OPERATIONS(Condition);

 private:
  Mutex* mutex_;
  pthread_cond_t cond_ = PTHREAD_COND_INITIALIZER;
};

// Small waitable event to synchronize two threads.
class WaitableEvent {
 public:
  WaitableEvent() : mutex_(), cond_(&mutex_) {}
  ~WaitableEvent() = default;

  // Returns true iff the event was signaled.
  bool IsSignaled() const {
    AutoLock lock(&mutex_);
    return signaled_;
  }

  // Signal event, it will be reset after Wait() is called.
  // Can be called several times.
  void Signal() {
    AutoLock lock(&mutex_);
    signaled_ = true;
    cond_.Signal();
  }

  // Wait for the event being signaled. Always reset the event.
  void Wait() {
    AutoLock lock(&mutex_);
    while (!signaled_)
      cond_.Wait();
    signaled_ = false;
  }

 private:
  mutable Mutex mutex_;
  Condition cond_;
  bool signaled_ = false;
};

// Small abstract base thread class. Usage is the following:
// 1) Define derived class that implements the Main() method.
// 2) Create new instance of derived class, which starts the thread
//    immediately.
// 3) Call Join() method to wait for thread exit.
// 4) Delete the instance.
class ThreadBase {
 public:
  // Constructor creates background thread, and starts it immediately.
  ThreadBase() {
    pthread_create(
        &handle_, nullptr,
        [](void* arg) -> void* {
          reinterpret_cast<ThreadBase*>(arg)->Main();
          return nullptr;
        },
        reinterpret_cast<void*>(this));
  }

  // Destructor.
  virtual ~ThreadBase() = default;

  // Wait until the thread terminates.
  void Join() {
    void* dummy = nullptr;
    pthread_join(handle_, &dummy);
  }

  // Must be implemented by derived classes.
  virtual void Main() = 0;

 private:
  pthread_t handle_;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_UTIL_THREADS_H
