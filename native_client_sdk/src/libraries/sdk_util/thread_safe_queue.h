// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBRARIES_SDK_UTIL_THREAD_SAFE_QUEUE_H_
#define LIBRARIES_SDK_UTIL_THREAD_SAFE_QUEUE_H_

#include <pthread.h>

#include <list>

#include "sdk_util/auto_lock.h"
#include "sdk_util/macros.h"

namespace sdk_util {

// ThreadSafeQueue
//
// A simple template to support multithreaded and optionally blocking access
// to a Queue of object pointers.
//
template<class T> class ThreadSafeQueue {
 public:
  ThreadSafeQueue() {
    pthread_cond_init(&cond_, NULL);
  }

  ThreadSafeQueue(const ThreadSafeQueue&) = delete;
  ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

  ~ThreadSafeQueue() {
    pthread_cond_destroy(&cond_);
  }

  void Enqueue(T* item) {
    AUTO_LOCK(lock_);
    list_.push_back(item);

    pthread_cond_signal(&cond_);
  }

  T* Dequeue(bool block) {
    AUTO_LOCK(lock_);

    // If blocking enabled, wait until we queue is non-empty
    if (block) {
      while (list_.empty()) pthread_cond_wait(&cond_, lock_.mutex());
    }

    if (list_.empty()) return NULL;

    T* item = list_.front();
    list_.pop_front();
    return item;
  }

 private:
  std::list<T*> list_;
  pthread_cond_t  cond_;
  SimpleLock lock_;
};

}  // namespace sdk_util

#endif  // LIBRARIES_SDK_UTIL_THREAD_SAFE_QUEUE_H_
