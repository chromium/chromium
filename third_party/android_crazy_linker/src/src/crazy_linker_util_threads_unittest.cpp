// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_util_threads.h"

#include <gtest/gtest.h>

#include <memory>

#include <sched.h>

namespace crazy {

TEST(ThreadBase, SimpleThread) {
  // Create a test thread class that simply sets a flag to true in its
  // main body and exits immediately.
  class SimpleThread : public ThreadBase {
   public:
    SimpleThread(bool* flag_ptr) : flag_ptr_(flag_ptr) {}

   private:
    void Main() override { *flag_ptr_ = true; }
    bool* flag_ptr_;
  };

  bool flag = false;
  SimpleThread thread(&flag);
  thread.Join();
  ASSERT_TRUE(flag);
}

TEST(Mutex, SimpleLockUnlock) {
  Mutex m;
  m.Lock();
  m.Unlock();
}

TEST(Mutex, ThreadSynchronization) {
  // State shared by all threads, i.e. a mutex-protected counter.
  struct SharedState {
    Mutex mutex;
    int counter = 0;
  };
  // Create kMaxThreads that will use a common lock to increment a counter
  // in succession. Each thread has a numerical id, and will loop until
  // the counter reaches before incrementing it then exiting.
  class TestThread : public ThreadBase {
   public:
    TestThread(SharedState* state, int id) : state_(state), id_(id) {}

   private:
    void Main() override {
      bool quit = false;
      while (!quit) {
        state_->mutex.Lock();
        if (state_->counter == id_) {
          state_->counter += 1;
          quit = true;
        } else {
          sched_yield();
        }
        state_->mutex.Unlock();
      }
    }

    SharedState* state_;
    int id_;
  };

  SharedState state;
  const int kMaxThreads = 100;
  std::unique_ptr<TestThread> threads[kMaxThreads];

  // Launch all threads
  for (int n = kMaxThreads; n > 0; n--) {
    threads[n - 1].reset(new TestThread(&state, n - 1));
  }

  // Join the last thread, this should only return when all other threads
  // have completed.
  threads[kMaxThreads - 1]->Join();

  ASSERT_EQ(kMaxThreads, state.counter);
  for (int n = 0; n < kMaxThreads - 1; n++)
    threads[n]->Join();
}

TEST(Condition, ThreadSynchonization) {
  // A TestThread class which will wait until Start() is called to increment
  // a given integer counter then exiting.
  class TestThread : public ThreadBase {
   public:
    TestThread(int* counter_ptr) : counter_ptr_(counter_ptr), cond_(&mutex_) {}

    void Start() {
      mutex_.Lock();
      started_ = true;
      cond_.Signal();
      mutex_.Unlock();
    }

   private:
    void Main() override {
      mutex_.Lock();
      while (!started_)
        cond_.Wait();

      *counter_ptr_ += 1;
      mutex_.Unlock();
    }

    int* counter_ptr_;
    Mutex mutex_;
    Condition cond_;
    bool started_ = false;
  };

  int counter = 0;
  const int kMaxThreads = 100;
  std::unique_ptr<TestThread> threads[kMaxThreads];

  for (int n = 0; n < kMaxThreads; ++n) {
    threads[n].reset(new TestThread(&counter));
  }

  ASSERT_EQ(0, counter);
  for (int n = 0; n < kMaxThreads; ++n) {
    threads[n]->Start();
    threads[n]->Join();
    ASSERT_EQ(n + 1, counter);
  }
}

}  // namespace crazy
