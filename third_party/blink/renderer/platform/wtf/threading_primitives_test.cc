// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
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

class MockBlockingObserver : public base::internal::BlockingObserver {
 public:
  // base::internal::BlockingObserver
  MOCK_METHOD1(BlockingStarted, void(base::BlockingType));
  MOCK_METHOD0(BlockingTypeUpgraded, void());
  MOCK_METHOD0(BlockingEnded, void());
};

class ThreadConditionTest : public testing::Test {
 public:
  ThreadConditionTest() : condition_(mutex_) {}

  void RunOtherThreadInfiniteWait() {
    MutexLocker lock(mutex_);
    ready_.Signal();

    base::internal::SetBlockingObserverForCurrentThread(&observer_);
    EXPECT_CALL(observer_, BlockingStarted(base::BlockingType::MAY_BLOCK));
    EXPECT_CALL(observer_, BlockingEnded());
    condition_.Wait();
    testing::Mock::VerifyAndClear(&observer_);
  }

 protected:
  // Used to make sure that the other thread gets to wait before the main thread
  // signals it. Otherwise it may wait forever.
  base::WaitableEvent ready_;

  testing::StrictMock<MockBlockingObserver> observer_;
  Mutex mutex_;
  ThreadCondition condition_;
};

TEST_F(ThreadConditionTest, WaitReportsBlockingCall) {
  base::Thread other_thread("other thread");
  other_thread.StartAndWaitForTesting();
  other_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ThreadConditionTest::RunOtherThreadInfiniteWait,
                     base::Unretained(this)));

  ready_.Wait();
  MutexLocker lock(mutex_);
  condition_.Signal();
}

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
