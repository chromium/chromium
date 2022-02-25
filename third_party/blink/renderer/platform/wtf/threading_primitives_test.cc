// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {
namespace {

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
}  // namespace WTF
