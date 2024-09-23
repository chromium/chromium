// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc/thread_wrapper.h"

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/task_queue/task_queue_factory.h"
#include "third_party/webrtc/api/task_queue/task_queue_test.h"
#include "third_party/webrtc_overrides/metronome_source.h"
#include "third_party/webrtc_overrides/test/metronome_like_task_queue_test.h"
#include "third_party/webrtc_overrides/timer_based_tick_provider.h"

namespace webrtc {
namespace {

using ::blink::MetronomeLikeTaskQueueTest;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MockFunction;

constexpr TimeDelta kTestDelay1 = TimeDelta::Millis(10);
constexpr TimeDelta kTestDelay2 = TimeDelta::Millis(20);
constexpr TimeDelta kTestDelay3 = TimeDelta::Millis(30);
constexpr TimeDelta kTestDelay4 = TimeDelta::Millis(40);
constexpr base::TimeDelta kMaxTestDelay = base::Milliseconds(40);

class ThreadWrapperTest : public testing::Test {
 public:
  // This method is used by the BlockingCallDuringBlockingCall test.
  // It sends message to the main thread synchronously using BlockingCall().
  void PingMainThread() {
    MockFunction<void()> handler;
    EXPECT_CALL(handler, Call);
    thread_->BlockingCall(handler.AsStdFunction());
  }

 protected:
  ThreadWrapperTest() : thread_(nullptr) {}

  void SetUp() override {
    ThreadWrapper::EnsureForCurrentMessageLoop();
    thread_ = ThreadWrapper::current();
  }

  // ThreadWrapper destroys itself when |message_loop_| is destroyed.
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<ThreadWrapper> thread_;
};

TEST_F(ThreadWrapperTest, PostTask) {
  MockFunction<void()> handler1;
  MockFunction<void()> handler2;
  MockFunction<void()> handler3;
  MockFunction<void()> handler4;

  thread_->PostTask(handler1.AsStdFunction());
  thread_->PostTask(handler2.AsStdFunction());
  thread_->PostTask(handler3.AsStdFunction());
  thread_->PostTask(handler4.AsStdFunction());

  InSequence in_seq;
  EXPECT_CALL(handler1, Call);
  EXPECT_CALL(handler2, Call);
  EXPECT_CALL(handler3, Call);
  EXPECT_CALL(handler4, Call);

  base::RunLoop().RunUntilIdle();
}

TEST_F(ThreadWrapperTest, PostDelayedTask) {
  MockFunction<void()> handler1;
  MockFunction<void()> handler2;
  MockFunction<void()> handler3;
  MockFunction<void()> handler4;

  thread_->PostDelayedHighPrecisionTask(handler1.AsStdFunction(), kTestDelay1);
  thread_->PostDelayedHighPrecisionTask(handler2.AsStdFunction(), kTestDelay2);
  thread_->PostDelayedHighPrecisionTask(handler3.AsStdFunction(), kTestDelay3);
  thread_->PostDelayedHighPrecisionTask(handler4.AsStdFunction(), kTestDelay4);

  InSequence in_seq;
  EXPECT_CALL(handler1, Call);
  EXPECT_CALL(handler2, Call);
  EXPECT_CALL(handler3, Call);
  EXPECT_CALL(handler4, Call);

  base::RunLoop run_loop;
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), kMaxTestDelay);
  run_loop.Run();
}

// Verify that BlockingCall() calls handler synchronously when called on the
// same thread.
TEST_F(ThreadWrapperTest, BlockingCallSameThread) {
  MockFunction<void()> handler;
  EXPECT_CALL(handler, Call);
  thread_->BlockingCall(handler.AsStdFunction());
}

void InitializeWrapperForNewThread(ThreadWrapper** thread,
                                   base::WaitableEvent* done_event) {
  ThreadWrapper::EnsureForCurrentMessageLoop();
  ThreadWrapper::current()->set_send_allowed(true);
  *thread = ThreadWrapper::current();
  done_event->Signal();
}

// Verify that BlockingCall() calls handler synchronously when called for a
// different thread.
TEST_F(ThreadWrapperTest, BlockingCallToOtherThread) {
  ThreadWrapper::current()->set_send_allowed(true);

  base::Thread second_thread("adWrapperTest");
  second_thread.Start();

  base::WaitableEvent initialized_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  ThreadWrapper* target;
  second_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&InitializeWrapperForNewThread, &target,
                                &initialized_event));
  initialized_event.Wait();

  ASSERT_TRUE(target != nullptr);

  MockFunction<void()> handler;
  EXPECT_CALL(handler, Call);
  target->BlockingCall(handler.AsStdFunction());
}

// Verify that thread handles BlockingCall() while another BlockingCall() is
// pending. The test creates second thread and BlockingCall()s
// to that thread. handler calls PingMainThread() on the BlockingCall which
// tries to BlockingCall() to the main thread.
TEST_F(ThreadWrapperTest, BlockingCallDuringBlockingCall) {
  ThreadWrapper::current()->set_send_allowed(true);

  base::Thread second_thread("adWrapperTest");
  second_thread.Start();

  base::WaitableEvent initialized_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  ThreadWrapper* target;
  second_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&InitializeWrapperForNewThread, &target,
                                &initialized_event));
  initialized_event.Wait();

  ASSERT_TRUE(target != nullptr);

  MockFunction<void()> handler;
  EXPECT_CALL(handler, Call)
      .WillOnce(Invoke(this, &ThreadWrapperTest::PingMainThread));
  target->BlockingCall(handler.AsStdFunction());
}

// Provider needed for the MetronomeLikeTaskQueueTest suite.
class ThreadWrapperProvider : public blink::MetronomeLikeTaskQueueProvider {
 public:
  void Initialize() override {
    ThreadWrapper::EnsureForCurrentMessageLoop();
    thread_ = rtc::Thread::Current();
  }

  base::TimeDelta DeltaToNextTick() const override {
    base::TimeTicks now = base::TimeTicks::Now();
    return blink::TimerBasedTickProvider::TimeSnappedToNextTick(
               now, blink::TimerBasedTickProvider::kDefaultPeriod) -
           now;
  }
  base::TimeDelta MetronomeTick() const override {
    return blink::TimerBasedTickProvider::kDefaultPeriod;
  }
  webrtc::TaskQueueBase* TaskQueue() const override { return thread_; }

 private:
  // ThreadWrapper destroys itself when |message_loop_| is destroyed.
  raw_ptr<rtc::Thread> thread_;
};

// Instantiate suite to run all tests defined in
// third_party/webrtc_overrides/test/metronome_like_task_queue_test.h
INSTANTIATE_TEST_SUITE_P(
    ThreadWrapper,
    MetronomeLikeTaskQueueTest,
    ::testing::Values(std::make_unique<ThreadWrapperProvider>));

class ThreadWrapperTaskQueueFactory : public TaskQueueFactory {
 public:
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
      std::string_view name,
      Priority priority) const override {
    std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
    thread->Start();
    return std::unique_ptr<webrtc::TaskQueueBase, webrtc::TaskQueueDeleter>(
        thread.release());
  }
};

std::unique_ptr<TaskQueueFactory> CreateTaskQueueFactory(
    const webrtc::FieldTrialsView*) {
  return std::make_unique<ThreadWrapperTaskQueueFactory>();
}

// Instantiate suite to run all tests defined in
// //third_party/webrtc/api/task_queue:task_queue_test.
INSTANTIATE_TEST_SUITE_P(ThreadWrapper,
                         TaskQueueTest,
                         ::testing::Values(CreateTaskQueueFactory));

}  // namespace

}  // namespace webrtc
