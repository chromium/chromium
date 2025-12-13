// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/log_event_dispatcher.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/raw_event_subscriber.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media::cast {

namespace {

class MockRawEventSubscriber : public RawEventSubscriber {
 public:
  MOCK_METHOD(void, OnReceiveFrameEvent, (const FrameEvent&), (override));
  MOCK_METHOD(void, OnReceivePacketEvent, (const PacketEvent&), (override));
};

class LogEventDispatcherTest : public ::testing::Test {
 public:
  LogEventDispatcherTest()
      : dispatcher_(std::make_unique<LogEventDispatcher>(
            task_environment_.GetMainThreadTaskRunner(),
            base::BindOnce(&LogEventDispatcherTest::OnDispatcherDeletion,
                           // Safe because we wait to delete `this` until
                           // this callback is executed.
                           base::Unretained(this)))) {
    dispatcher_->Subscribe(&subscriber_);
    is_subscribed_ = true;
  }

  void OnDispatcherDeletion() {
    ASSERT_TRUE(quit_closure_);
    std::move(quit_closure_).Run();
  }

  ~LogEventDispatcherTest() override {
    quit_closure_ = task_environment_.QuitClosure();
    Unsubscribe();
    dispatcher_.reset();

    // Ensure that the Impl gets deleted on the main thread.
    task_environment_.RunUntilQuit();
  }

  void Unsubscribe() {
    if (is_subscribed_) {
      dispatcher_->Unsubscribe(&subscriber_);
      is_subscribed_ = false;
    }
  }

  scoped_refptr<base::SingleThreadTaskRunner> CreateTaskRunner() {
    return base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
         base::WithBaseSyncPrimitives(), base::MayBlock()},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  LogEventDispatcher& dispatcher() { return *dispatcher_; }

  testing::StrictMock<MockRawEventSubscriber>& subscriber() {
    return subscriber_;
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<LogEventDispatcher> dispatcher_;
  testing::StrictMock<MockRawEventSubscriber> subscriber_;
  bool is_subscribed_ = false;

  // Used to ensure destruction.
  base::OnceClosure quit_closure_;
};

}  // namespace

// Simple test: do nothing expect construct and deconstruct the test suite,
// which automatically subscribes and unsubscribes.
TEST_F(LogEventDispatcherTest, SubscribeAndUnsubscribe) {}

TEST_F(LogEventDispatcherTest, DispatchFrameEvent) {
  auto frame_event = std::make_unique<FrameEvent>();
  frame_event->type = FRAME_CAPTURE_BEGIN;
  EXPECT_CALL(subscriber(), OnReceiveFrameEvent(testing::Ref(*frame_event)))
      .WillOnce([closure = task_environment().QuitClosure()]() {
        std::move(closure).Run();
      });
  dispatcher().DispatchFrameEvent(std::move(frame_event));
  task_environment().RunUntilQuit();
}

TEST_F(LogEventDispatcherTest, DispatchFrameEventOnAnotherThread) {
  auto frame_event = std::make_unique<FrameEvent>();
  frame_event->type = FRAME_CAPTURE_BEGIN;
  EXPECT_CALL(subscriber(), OnReceiveFrameEvent(testing::Ref(*frame_event)))
      .WillOnce([closure = task_environment().QuitClosure()]() {
        std::move(closure).Run();
      });

  auto task_runner = CreateTaskRunner();
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](LogEventDispatcherTest* test,
             std::unique_ptr<FrameEvent> frame_event) {
            test->dispatcher().DispatchFrameEvent(std::move(frame_event));
          }
          // Safe because we own the task runner.
          ,
          base::Unretained(this), std::move(frame_event)));
  task_environment().RunUntilQuit();
}

TEST_F(LogEventDispatcherTest, DispatchPacketEvent) {
  auto packet_event = std::make_unique<PacketEvent>();
  packet_event->type = PACKET_SENT_TO_NETWORK;
  EXPECT_CALL(subscriber(), OnReceivePacketEvent(testing::Ref(*packet_event)))
      .WillOnce([closure = task_environment().QuitClosure()]() {
        std::move(closure).Run();
      });
  dispatcher().DispatchPacketEvent(std::move(packet_event));
  task_environment().RunUntilQuit();
}

TEST_F(LogEventDispatcherTest, DispatchPacketEventOnAnotherThread) {
  auto packet_event = std::make_unique<PacketEvent>();
  packet_event->type = PACKET_SENT_TO_NETWORK;
  EXPECT_CALL(subscriber(), OnReceivePacketEvent(testing::Ref(*packet_event)))
      .WillOnce([closure = task_environment().QuitClosure()]() {
        std::move(closure).Run();
      });

  auto task_runner = CreateTaskRunner();
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](LogEventDispatcherTest* test,
             std::unique_ptr<PacketEvent> packet_event) {
            test->dispatcher().DispatchPacketEvent(std::move(packet_event));
          }
          // Safe because we own the task runner.
          ,
          base::Unretained(this), std::move(packet_event)));
  task_environment().RunUntilQuit();
}

TEST_F(LogEventDispatcherTest, DispatchBatchOfEvents) {
  auto frame_events = std::make_unique<std::vector<FrameEvent>>();
  frame_events->push_back(FrameEvent());
  frame_events->back().type = FRAME_CAPTURE_BEGIN;
  frame_events->push_back(FrameEvent());
  frame_events->back().type = FRAME_CAPTURE_END;
  auto packet_events = std::make_unique<std::vector<PacketEvent>>();
  packet_events->push_back(PacketEvent());
  packet_events->back().type = PACKET_SENT_TO_NETWORK;

  constexpr int kExpectedEventCount = 3;
  int event_count = 0;

  auto event_closure = [&, quit_closure = task_environment().QuitClosure()] {
    if (++event_count == kExpectedEventCount) {
      std::move(quit_closure).Run();
    }
  };
  EXPECT_CALL(subscriber(),
              OnReceiveFrameEvent(testing::Ref(frame_events->at(0))))
      .WillOnce(testing::InvokeWithoutArgs(event_closure));
  EXPECT_CALL(subscriber(),
              OnReceiveFrameEvent(testing::Ref(frame_events->at(1))))
      .WillOnce(testing::InvokeWithoutArgs(event_closure));
  EXPECT_CALL(subscriber(),
              OnReceivePacketEvent(testing::Ref(packet_events->at(0))))
      .WillOnce(testing::InvokeWithoutArgs(event_closure));

  dispatcher().DispatchBatchOfEvents(std::move(frame_events),
                                     std::move(packet_events));
  task_environment().RunUntilQuit();
}

TEST_F(LogEventDispatcherTest, DispatchBatchOfEventsOnAnotherThread) {
  auto frame_events = std::make_unique<std::vector<FrameEvent>>();
  frame_events->push_back(FrameEvent());
  frame_events->back().type = FRAME_CAPTURE_BEGIN;
  frame_events->push_back(FrameEvent());
  frame_events->back().type = FRAME_CAPTURE_END;
  auto packet_events = std::make_unique<std::vector<PacketEvent>>();
  packet_events->push_back(PacketEvent());
  packet_events->back().type = PACKET_SENT_TO_NETWORK;

  constexpr int kExpectedEventCount = 3;
  int event_count = 0;

  auto event_closure = [&, quit_closure = task_environment().QuitClosure()] {
    if (++event_count == kExpectedEventCount) {
      std::move(quit_closure).Run();
    }
  };
  EXPECT_CALL(subscriber(),
              OnReceiveFrameEvent(testing::Ref(frame_events->at(0))))
      .WillOnce(testing::InvokeWithoutArgs(event_closure));
  EXPECT_CALL(subscriber(),
              OnReceiveFrameEvent(testing::Ref(frame_events->at(1))))
      .WillOnce(testing::InvokeWithoutArgs(event_closure));
  EXPECT_CALL(subscriber(),
              OnReceivePacketEvent(testing::Ref(packet_events->at(0))))
      .WillOnce(testing::InvokeWithoutArgs(event_closure));

  auto task_runner = CreateTaskRunner();
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](LogEventDispatcherTest* test,
             std::unique_ptr<std::vector<FrameEvent>> frame_events,
             std::unique_ptr<std::vector<PacketEvent>> packet_events) {
            test->dispatcher().DispatchBatchOfEvents(std::move(frame_events),
                                                     std::move(packet_events));
          }
          // Safe because we own the task runner.
          ,
          base::Unretained(this), std::move(frame_events),
          std::move(packet_events)));
  task_environment().RunUntilQuit();
}

TEST_F(LogEventDispatcherTest, UnsubscribeDuringDispatch) {
  auto frame_event = std::make_unique<FrameEvent>();
  frame_event->type = FRAME_CAPTURE_BEGIN;

  EXPECT_CALL(subscriber(), OnReceiveFrameEvent(testing::Ref(*frame_event)))
      .WillOnce(
          [&, closure = task_environment().QuitClosure()](const FrameEvent&) {
            Unsubscribe();
            std::move(closure).Run();
          });

  dispatcher().DispatchFrameEvent(std::move(frame_event));
  task_environment().RunUntilQuit();
}

TEST_F(LogEventDispatcherTest, UnsubscribeOnDifferentThreadDuringDispatch) {
  base::WaitableEvent wait_for_unsubscribe;
  auto frame_event = std::make_unique<FrameEvent>();
  frame_event->type = FRAME_CAPTURE_BEGIN;

  auto task_runner = CreateTaskRunner();
  EXPECT_CALL(subscriber(), OnReceiveFrameEvent(testing::Ref(*frame_event)))
      .WillOnce([&, quit_closure =
                        task_environment().QuitClosure()](const FrameEvent&) {
        task_runner->PostTask(FROM_HERE,
                              base::BindOnce(
                                  [](LogEventDispatcherTest* test,
                                     base::OnceClosure quit_closure) {
                                    test->Unsubscribe();
                                    std::move(quit_closure).Run();
                                  },
                                  // Safe because we own the task runner.
                                  base::Unretained(this), quit_closure));
      });

  dispatcher().DispatchFrameEvent(std::move(frame_event));
  task_environment().RunUntilQuit();
}

}  // namespace media::cast
