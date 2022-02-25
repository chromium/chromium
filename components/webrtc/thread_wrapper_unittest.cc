// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc_overrides/metronome_source.h"
#include "third_party/webrtc_overrides/test/metronome_like_task_queue_test.h"

using ::blink::MetronomeLikeTaskQueueTest;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;

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

// Helper class used in the Dispose test.
class DeletableObject {
 public:
  DeletableObject(bool* deleted) : deleted_(deleted) { *deleted = false; }

  ~DeletableObject() { *deleted_ = true; }

 private:
  raw_ptr<bool> deleted_;
};

class ThreadWrapperTest : public testing::Test {
 public:
  // This method is used by the SendDuringSend test. It sends message to the
  // main thread synchronously using Send().
  void PingMainThread() {
    rtc::MessageData* data = new rtc::MessageData();
    MockMessageHandler handler;

    EXPECT_CALL(handler, OnMessage(MatchMessage(&handler, kTestMessage2, data)))
        .WillOnce(DeleteMessageData());
    thread_->Send(RTC_FROM_HERE, &handler, kTestMessage2, data);
  }

 protected:
  ThreadWrapperTest() : thread_(nullptr) {}

  void SetUp() override {
    ThreadWrapper::EnsureForCurrentMessageLoop();
    thread_ = rtc::Thread::Current();
  }

  // ThreadWrapper destroys itself when |message_loop_| is destroyed.
  base::test::SingleThreadTaskEnvironment task_environment_;
  raw_ptr<rtc::Thread> thread_;
  MockMessageHandler handler1_;
  MockMessageHandler handler2_;
};

TEST_F(ThreadWrapperTest, Post) {
  rtc::MessageData* data1 = new rtc::MessageData();
  rtc::MessageData* data2 = new rtc::MessageData();
  rtc::MessageData* data3 = new rtc::MessageData();
  rtc::MessageData* data4 = new rtc::MessageData();

  thread_->Post(RTC_FROM_HERE, &handler1_, kTestMessage1, data1);
  thread_->Post(RTC_FROM_HERE, &handler1_, kTestMessage2, data2);
  thread_->Post(RTC_FROM_HERE, &handler2_, kTestMessage1, data3);
  thread_->Post(RTC_FROM_HERE, &handler2_, kTestMessage1, data4);

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
  thread_->Post(RTC_FROM_HERE, &handler1_, kTestMessage1, NULL);
  thread_->Post(RTC_FROM_HERE, &handler1_, kTestMessage2, NULL);
  thread_->Post(RTC_FROM_HERE, &handler2_, kTestMessage1, NULL);
  thread_->Post(RTC_FROM_HERE, &handler2_, kTestMessage2, NULL);

  thread_->Clear(&handler1_, kTestMessage2);

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

  thread_->Clear(&handler1_, kTestMessage2);

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
    thread_->Post(RTC_FROM_HERE, &handler, kTestMessage1, NULL);
  }
  rtc::MessageList removed;
  thread_->Clear(handler_ptr, rtc::MQID_ANY, &removed);
  DCHECK_EQ(0U, removed.size());
}

// Verify that Send() calls handler synchronously when called on the
// same thread.
TEST_F(ThreadWrapperTest, SendSameThread) {
  rtc::MessageData* data = new rtc::MessageData();

  EXPECT_CALL(handler1_,
              OnMessage(MatchMessage(&handler1_, kTestMessage1, data)))
      .WillOnce(DeleteMessageData());
  thread_->Send(RTC_FROM_HERE, &handler1_, kTestMessage1, data);
}

void InitializeWrapperForNewThread(rtc::Thread** thread,
                                   base::WaitableEvent* done_event) {
  ThreadWrapper::EnsureForCurrentMessageLoop();
  ThreadWrapper::current()->set_send_allowed(true);
  *thread = ThreadWrapper::current();
  done_event->Signal();
}

// Verify that Send() calls handler synchronously when called for a
// different thread.
TEST_F(ThreadWrapperTest, SendToOtherThread) {
  ThreadWrapper::current()->set_send_allowed(true);

  base::Thread second_thread("adWrapperTest");
  second_thread.Start();

  base::WaitableEvent initialized_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  rtc::Thread* target;
  second_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&InitializeWrapperForNewThread, &target,
                                &initialized_event));
  initialized_event.Wait();

  ASSERT_TRUE(target != NULL);

  rtc::MessageData* data = new rtc::MessageData();

  EXPECT_CALL(handler1_,
              OnMessage(MatchMessage(&handler1_, kTestMessage1, data)))
      .WillOnce(DeleteMessageData());
  target->Send(RTC_FROM_HERE, &handler1_, kTestMessage1, data);

  Mock::VerifyAndClearExpectations(&handler1_);
}

// Verify that thread handles Send() while another Send() is
// pending. The test creates second thread and Send()s kTestMessage1
// to that thread. kTestMessage1 handler calls PingMainThread() which
// tries to Send() kTestMessage2 to the main thread.
TEST_F(ThreadWrapperTest, SendDuringSend) {
  ThreadWrapper::current()->set_send_allowed(true);

  base::Thread second_thread("adWrapperTest");
  second_thread.Start();

  base::WaitableEvent initialized_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  rtc::Thread* target;
  second_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&InitializeWrapperForNewThread, &target,
                                &initialized_event));
  initialized_event.Wait();

  ASSERT_TRUE(target != NULL);

  rtc::MessageData* data = new rtc::MessageData();

  EXPECT_CALL(handler1_,
              OnMessage(MatchMessage(&handler1_, kTestMessage1, data)))
      .WillOnce(
          DoAll(InvokeWithoutArgs(this, &ThreadWrapperTest::PingMainThread),
                DeleteMessageData()));
  target->Send(RTC_FROM_HERE, &handler1_, kTestMessage1, data);

  Mock::VerifyAndClearExpectations(&handler1_);
}

TEST_F(ThreadWrapperTest, Dispose) {
  bool deleted_ = false;
  thread_->Dispose(new DeletableObject(&deleted_));
  EXPECT_FALSE(deleted_);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(deleted_);
}

// Provider needed for the MetronomeLikeTaskQueueTest suite.
class ThreadWrapperProvider : public blink::MetronomeLikeTaskQueueProvider {
 public:
  void Initialize() override {
    scoped_feature_list_.InitAndEnableFeature(kThreadWrapperUsesMetronome);
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
  base::test::ScopedFeatureList scoped_feature_list_;
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
