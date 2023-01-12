// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {
namespace {

class LambdaThreadDelegate : public base::PlatformThread::Delegate {
 public:
  explicit LambdaThreadDelegate(base::RepeatingClosure f) : f_(std::move(f)) {}
  void ThreadMain() override { f_.Run(); }

 private:
  base::RepeatingClosure f_;
};

}  // namespace

TEST(RecursiveMutexTest, LockUnlock) {
  RecursiveMutex mutex;
  mutex.lock();
  mutex.AssertAcquired();
  mutex.unlock();
}

// NO_THREAD_SAFTEY_ANALYSIS: The thread checker (rightfully so) doesn't like
// recursive lock acquisition. Disable it in this test. We prefer to keep lock
// checking in the production code, to at least prevent some easy recursive
// locking cases from being added.
TEST(RecursiveMutexTest, LockUnlockRecursive) NO_THREAD_SAFETY_ANALYSIS {
  RecursiveMutex mutex;
  mutex.lock();
  mutex.lock();
  mutex.AssertAcquired();
  mutex.unlock();
  mutex.AssertAcquired();
  mutex.unlock();

  EXPECT_EQ(mutex.owner_, base::kInvalidThreadId);
}

TEST(RecursiveMutexTest, LockUnlockThreads) NO_THREAD_SAFETY_ANALYSIS {
  RecursiveMutex mutex;
  std::atomic<bool> locked_mutex{false};
  std::atomic<bool> can_proceed{false};
  std::atomic<bool> locked_mutex_recursively{false};

  LambdaThreadDelegate delegate{
      base::BindLambdaForTesting([&]() NO_THREAD_SAFETY_ANALYSIS {
        mutex.lock();
        locked_mutex.store(true);
        while (!can_proceed.load()) {
        }
        can_proceed.store(false);
        mutex.lock();
        locked_mutex_recursively.store(true);
        while (!can_proceed.load()) {
        }

        mutex.unlock();
        mutex.unlock();
      })};
  base::PlatformThreadHandle handle;
  base::PlatformThread::Create(0, &delegate, &handle);

  while (!locked_mutex.load()) {
  }
  EXPECT_FALSE(mutex.TryLock());
  can_proceed.store(true);
  while (!locked_mutex_recursively.load()) {
  }
  EXPECT_FALSE(mutex.TryLock());
  can_proceed.store(true);

  base::PlatformThread::Join(handle);
  EXPECT_TRUE(mutex.TryLock());
  mutex.unlock();
}

}  // namespace WTF
