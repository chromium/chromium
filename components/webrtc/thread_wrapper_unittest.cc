// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc/thread_wrapper.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc_overrides/metronome_source.h"
#include "third_party/webrtc_overrides/test/metronome_like_task_queue_test.h"

using ::blink::MetronomeLikeTaskQueueTest;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MockFunction;

namespace webrtc {

static const uint32_t kTestMessage1 = 1;
static const uint32_t kTestMessage2 = 2;

static const int kTestDelayMs1 = 10;
static const int kTestDelayMs2 = 20;
static const int kTestDelayMs3 = 30;
static const int kTestDelayMs4 = 40;
static const int kMaxTestDelay = 40;

namespace {

class MockMessageHandler : public rtc::MessageHandlerAutoCleanup {
 public:
  MOCK_METHOD1(OnMessage, void(rtc::Message* msg));
};

MATCHER_P3(MatchMessage, handler, message_id, data, "") {
  return arg->phandler == handler && arg->message_id == message_id &&
         arg->pdata == data;
}

ACTION(DeleteMessageData) {
  delete arg0->pdata;
}

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
  MockMessageHandler handler1_;
  MockMessageHandler handler2_;
};

TEST_F(ThreadWrapperTest, Post) {
  rtc::MessageData* data1 = new rtc::MessageData();
  rtc::MessageData* data2 = new rtc::MessageData();
  rtc::MessageData* data3 = new rtc::MessageData();
  rtc::MessageData* data4 = new rtc::MessageData();

  thread_->Post(RTC_FROM_HERE, &handler1_, kTestMessage1, data1, false);
  thread_->Post(RTC_FROM_HERE, &handler1_, kTestMessage2, data2, false);
  thread_->Post(RTC_FROM_HERE, &handler2_, kTestMessage1, data3, false);
  thread_->Post(RTC_FROM_HERE, &handler2_, kTestMessage1, data4, false);

  InSequence in_seq;

  EXPECT_CALL(handler1_,
              OnMessage(MatchMessage(&handler1_, kTestMessage1, data1)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler1_,
              OnMessage(MatchMessage(&handler1_, kTestMessage2, data2)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_,
              OnMessage(MatchMessage(&handler2_, kTestMessage1, data3)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_,
              OnMessage(MatchMessage(&handler2_, kTestMessage1, data4)))
      .WillOnce(DeleteMessageData());

  base::RunLoop().RunUntilIdle();
}

TEST_F(ThreadWrapperTest, PostDelayed) {
  rtc::MessageData* data1 = new rtc::MessageData();
  rtc::MessageData* data2 = new rtc::MessageData();
  rtc::MessageData* data3 = new rtc::MessageData();
  rtc::MessageData* data4 = new rtc::MessageData();

  thread_->PostDelayed(RTC_FROM_HERE, kTestDelayMs1, &handler1_, kTestMessage1,
                       data1);
  thread_->PostDelayed(RTC_FROM_HERE, kTestDelayMs2, &handler1_, kTestMessage2,
                       data2);
  thread_->PostDelayed(RTC_FROM_HERE, kTestDelayMs3, &handler2_, kTestMessage1,
                       data3);
  thread_->PostDelayed(RTC_FROM_HERE, kTestDelayMs4, &handler2_, kTestMessage1,
                       data4);

  InSequence in_seq;

  EXPECT_CALL(handler1_,
              OnMessage(MatchMessage(&handler1_, kTestMessage1, data1)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler1_,
              OnMessage(MatchMessage(&handler1_, kTestMessage2, data2)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_,
              OnMessage(MatchMessage(&handler2_, kTestMessage1, data3)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_,
              OnMessage(MatchMessage(&handler2_, kTestMessage1, data4)))
      .WillOnce(DeleteMessageData());

  base::RunLoop run_loop;
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(kMaxTestDelay));
  run_loop.Run();
}

TEST_F(ThreadWrapperTest, Clear) {
  thread_->Post(RTC_FROM_HERE, &handler1_, kTestMessage1, NULL, false);
  thread_->Post(RTC_FROM_HERE, &handler1_, kTestMessage2, NULL, false);
  thread_->Post(RTC_FROM_HERE, &handler2_, kTestMessage1, NULL, false);
  thread_->Post(RTC_FROM_HERE, &handler2_, kTestMessage2, NULL, false);

  thread_->Clear(&handler1_, kTestMessage2, nullptr);

  InSequence in_seq;

  rtc::MessageData* null_data = NULL;
  EXPECT_CALL(handler1_,
              OnMessage(MatchMessage(&handler1_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_,
              OnMessage(MatchMessage(&handler2_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_,
              OnMessage(MatchMessage(&handler2_, kTestMessage2, null_data)))
      .WillOnce(DeleteMessageData());

  base::RunLoop().RunUntilIdle();
}

TEST_F(ThreadWrapperTest, ClearDelayed) {
  thread_->PostDelayed(RTC_FROM_HERE, kTestDelayMs1, &handler1_, kTestMessage1,
                       NULL);
  thread_->PostDelayed(RTC_FROM_HERE, kTestDelayMs2, &handler1_, kTestMessage2,
                       NULL);
  thread_->PostDelayed(RTC_FROM_HERE, kTestDelayMs3, &handler2_, kTestMessage1,
                       NULL);
  thread_->PostDelayed(RTC_FROM_HERE, kTestDelayMs4, &handler2_, kTestMessage1,
                       NULL);

  thread_->Clear(&handler1_, kTestMessage2, nullptr);

  InSequence in_seq;

  rtc::MessageData* null_data = NULL;
  EXPECT_CALL(handler1_,
              OnMessage(MatchMessage(&handler1_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_,
              OnMessage(MatchMessage(&handler2_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());
  EXPECT_CALL(handler2_,
              OnMessage(MatchMessage(&handler2_, kTestMessage1, null_data)))
      .WillOnce(DeleteMessageData());

  base::RunLoop run_loop;
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(kMaxTestDelay));
  run_loop.Run();
}

// Verify that the queue is cleared when a handler is destroyed.
TEST_F(ThreadWrapperTest, ClearDestroyed) {
  MockMessageHandler* handler_ptr;
  {
    MockMessageHandler handler;
    handler_ptr = &handler;
    thread_->Post(RTC_FROM_HERE, &handler, kTestMessage1, NULL, false);
  }
  rtc::MessageList removed;
  thread_->Clear(handler_ptr, rtc::MQID_ANY, &removed);
  DCHECK_EQ(0U, removed.size());
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
    return blink::MetronomeSource::TimeSnappedToNextTick(now) - now;
  }
  base::TimeDelta MetronomeTick() const override {
    return blink::MetronomeSource::Tick();
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

}  // namespace

}  // namespace webrtc
