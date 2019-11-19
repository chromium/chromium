// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <map>
#include <memory>
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/trap.h"
#include "mojo/public/c/system/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace core {
namespace {

using TrapTest = test::MojoTestBase;

class TriggerHelper {
 public:
  using ContextCallback = base::RepeatingCallback<void(const MojoTrapEvent&)>;

  TriggerHelper() {}
  ~TriggerHelper() {}

  MojoResult CreateTrap(MojoHandle* handle) {
    return MojoCreateTrap(&Notify, nullptr, handle);
  }

  template <typename Handler>
  uintptr_t CreateContext(Handler handler) {
    return CreateContextWithCancel(handler, [] {});
  }

  template <typename Handler, typename CancelHandler>
  uintptr_t CreateContextWithCancel(Handler handler,
                                    CancelHandler cancel_handler) {
    auto* context =
        new NotificationContext(base::BindLambdaForTesting(handler));
    context->SetCancelCallback(
        base::BindOnce(base::BindLambdaForTesting([cancel_handler, context] {
          cancel_handler();
          delete context;
        })));
    return reinterpret_cast<uintptr_t>(context);
  }

 private:
  class NotificationContext {
   public:
    explicit NotificationContext(const ContextCallback& callback)
        : callback_(callback) {}

    ~NotificationContext() {}

    void SetCancelCallback(base::OnceClosure cancel_callback) {
      cancel_callback_ = std::move(cancel_callback);
    }

    void Notify(const MojoTrapEvent& event) {
      if (event.result == MOJO_RESULT_CANCELLED && cancel_callback_)
        std::move(cancel_callback_).Run();
      else
        callback_.Run(event);
    }

   private:
    const ContextCallback callback_;
    base::OnceClosure cancel_callback_;

    DISALLOW_COPY_AND_ASSIGN(NotificationContext);
  };

  static void Notify(const MojoTrapEvent* event) {
    reinterpret_cast<NotificationContext*>(event->trigger_context)
        ->Notify(*event);
  }

  DISALLOW_COPY_AND_ASSIGN(TriggerHelper);
};

class ThreadedRunner : public base::SimpleThread {
 public:
  explicit ThreadedRunner(base::OnceClosure callback)
      : SimpleThread("ThreadedRunner"), callback_(std::move(callback)) {}
  ~ThreadedRunner() override {}

  void Run() override { std::move(callback_).Run(); }

 private:
  base::OnceClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(ThreadedRunner);
};

void ExpectNoNotification(const MojoTrapEvent* event) {
  NOTREACHED();
}

void ExpectOnlyCancel(const MojoTrapEvent* event) {
  EXPECT_EQ(event->result, MOJO_RESULT_CANCELLED);
}

TEST_F(TrapTest, InvalidArguments) {
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoCreateTrap(&ExpectNoNotification, nullptr, nullptr));
  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateTrap(&ExpectNoNotification, nullptr, &t));

  // Try to add triggers for handles which don't raise trappable signals.
  EXPECT_EQ(
      MOJO_RESULT_INVALID_ARGUMENT,
      MojoAddTrigger(t, t, MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED, 0, nullptr));
  MojoHandle buffer_handle = CreateBuffer(42);
  EXPECT_EQ(
      MOJO_RESULT_INVALID_ARGUMENT,
      MojoAddTrigger(t, buffer_handle, MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED, 0, nullptr));

  // Try to remove a trigger on a non-trap handle.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoRemoveTrigger(buffer_handle, 0, nullptr));

  // Try to arm an invalid or non-trap handle.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoArmTrap(MOJO_HANDLE_INVALID, nullptr, nullptr, nullptr));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoArmTrap(buffer_handle, nullptr, nullptr, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(buffer_handle));

  // Try to arm with a non-null count but a null output buffer.
  uint32_t num_blocking_events = 1;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoArmTrap(t, nullptr, &num_blocking_events, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
}

TEST_F(TrapTest, TrapMessagePipeReadable) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  TriggerHelper helper;
  int num_expected_notifications = 1;
  const uintptr_t readable_a_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_GT(num_expected_notifications, 0);
        num_expected_notifications -= 1;

        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        wait.Signal();
      });

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_a_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  const char kMessage1[] = "hey hey hey hey";
  const char kMessage2[] = "i said hey";
  const char kMessage3[] = "what's goin' on?";

  // Writing to |b| multiple times should notify exactly once.
  WriteMessage(b, kMessage1);
  WriteMessage(b, kMessage2);
  wait.Wait();

  // This also shouldn't fire a notification; the trap is still disarmed.
  WriteMessage(b, kMessage3);

  // Arming should fail with relevant information.
  constexpr size_t kMaxBlockingEvents = 3;
  uint32_t num_blocking_events = kMaxBlockingEvents;
  MojoTrapEvent blocking_events[kMaxBlockingEvents] = {
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])}};
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_events[0]));
  EXPECT_EQ(1u, num_blocking_events);
  EXPECT_EQ(readable_a_context, blocking_events[0].trigger_context);
  EXPECT_EQ(MOJO_RESULT_OK, blocking_events[0].result);

  // Flush the three messages from above.
  EXPECT_EQ(kMessage1, ReadMessage(a));
  EXPECT_EQ(kMessage2, ReadMessage(a));
  EXPECT_EQ(kMessage3, ReadMessage(a));

  // Now we can rearm the trap.
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
}

TEST_F(TrapTest, CloseWatchedMessagePipeHandle) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  TriggerHelper helper;
  const uintptr_t readable_a_context = helper.CreateContextWithCancel(
      [](const MojoTrapEvent&) {}, [&] { wait.Signal(); });

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_a_context, nullptr));

  // Test that closing a watched handle fires an appropriate notification, even
  // when the trap is unarmed.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  wait.Wait();

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
}

TEST_F(TrapTest, CloseWatchedMessagePipeHandlePeer) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  TriggerHelper helper;
  const uintptr_t readable_a_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, event.result);
        wait.Signal();
      });

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_a_context, nullptr));

  // Test that closing a watched handle's peer with an armed trap fires an
  // appropriate notification.
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  wait.Wait();

  // And now arming should fail with correct information about |a|'s state.
  constexpr size_t kMaxBlockingEvents = 3;
  uint32_t num_blocking_events = kMaxBlockingEvents;
  MojoTrapEvent blocking_events[kMaxBlockingEvents] = {
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])}};
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_events[0]));
  EXPECT_EQ(1u, num_blocking_events);
  EXPECT_EQ(readable_a_context, blocking_events[0].trigger_context);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, blocking_events[0].result);
  EXPECT_TRUE(blocking_events[0].signals_state.satisfied_signals &
              MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_FALSE(blocking_events[0].signals_state.satisfiable_signals &
               MOJO_HANDLE_SIGNAL_READABLE);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
}

TEST_F(TrapTest, TrapDataPipeConsumerReadable) {
  constexpr size_t kTestPipeCapacity = 64;
  MojoHandle producer, consumer;
  CreateDataPipe(&producer, &consumer, kTestPipeCapacity);

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  TriggerHelper helper;
  int num_expected_notifications = 1;
  const uintptr_t readable_consumer_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_GT(num_expected_notifications, 0);
        num_expected_notifications -= 1;

        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        wait.Signal();
      });

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, consumer, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_consumer_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  const char kMessage1[] = "hey hey hey hey";
  const char kMessage2[] = "i said hey";
  const char kMessage3[] = "what's goin' on?";

  // Writing to |producer| multiple times should notify exactly once.
  WriteData(producer, kMessage1);
  WriteData(producer, kMessage2);
  wait.Wait();

  // This also shouldn't fire a notification; the trap is still disarmed.
  WriteData(producer, kMessage3);

  // Arming should fail with relevant information.
  constexpr size_t kMaxBlockingEvents = 3;
  uint32_t num_blocking_events = kMaxBlockingEvents;
  MojoTrapEvent blocking_events[kMaxBlockingEvents] = {
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])}};
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_events[0]));
  EXPECT_EQ(1u, num_blocking_events);
  EXPECT_EQ(readable_consumer_context, blocking_events[0].trigger_context);
  EXPECT_EQ(MOJO_RESULT_OK, blocking_events[0].result);

  // Flush the three messages from above.
  EXPECT_EQ(kMessage1, ReadData(consumer, sizeof(kMessage1) - 1));
  EXPECT_EQ(kMessage2, ReadData(consumer, sizeof(kMessage2) - 1));
  EXPECT_EQ(kMessage3, ReadData(consumer, sizeof(kMessage3) - 1));

  // Now we can rearm the trap.
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(producer));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(consumer));
}

TEST_F(TrapTest, TrapDataPipeConsumerNewDataReadable) {
  constexpr size_t kTestPipeCapacity = 64;
  MojoHandle producer, consumer;
  CreateDataPipe(&producer, &consumer, kTestPipeCapacity);

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  TriggerHelper helper;
  int num_new_data_notifications = 0;
  const uintptr_t new_data_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        num_new_data_notifications += 1;

        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        wait.Signal();
      });

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, consumer, MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           new_data_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  const char kMessage1[] = "hey hey hey hey";
  const char kMessage2[] = "i said hey";
  const char kMessage3[] = "what's goin' on?";

  // Writing to |producer| multiple times should notify exactly once.
  WriteData(producer, kMessage1);
  WriteData(producer, kMessage2);
  wait.Wait();

  // This also shouldn't fire a notification; the trap is still disarmed.
  WriteData(producer, kMessage3);

  // Arming should fail with relevant information.
  constexpr size_t kMaxBlockingEvents = 3;
  uint32_t num_blocking_events = kMaxBlockingEvents;
  MojoTrapEvent blocking_events[kMaxBlockingEvents] = {
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])}};
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_events[0]));
  EXPECT_EQ(1u, num_blocking_events);
  EXPECT_EQ(new_data_context, blocking_events[0].trigger_context);
  EXPECT_EQ(MOJO_RESULT_OK, blocking_events[0].result);

  // Attempt to read more data than is available. Should fail but clear the
  // NEW_DATA_READABLE signal.
  char large_buffer[512];
  uint32_t large_read_size = 512;
  MojoReadDataOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_READ_DATA_FLAG_ALL_OR_NONE;
  EXPECT_EQ(MOJO_RESULT_OUT_OF_RANGE,
            MojoReadData(consumer, &options, large_buffer, &large_read_size));

  // Attempt to arm again. Should succeed.
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  // Write more data. Should notify.
  wait.Reset();
  WriteData(producer, kMessage1);
  wait.Wait();

  // Reading some data should clear NEW_DATA_READABLE again so we can rearm.
  EXPECT_EQ(kMessage1, ReadData(consumer, sizeof(kMessage1) - 1));

  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  EXPECT_EQ(2, num_new_data_notifications);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(producer));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(consumer));
}

TEST_F(TrapTest, TrapDataPipeProducerWritable) {
  constexpr size_t kTestPipeCapacity = 8;
  MojoHandle producer, consumer;
  CreateDataPipe(&producer, &consumer, kTestPipeCapacity);

  // Half the capacity of the data pipe.
  const char kTestData[] = "aaaa";
  static_assert((sizeof(kTestData) - 1) * 2 == kTestPipeCapacity,
                "Invalid test data for this test.");

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  TriggerHelper helper;
  int num_expected_notifications = 1;
  const uintptr_t writable_producer_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_GT(num_expected_notifications, 0);
        num_expected_notifications -= 1;

        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        wait.Signal();
      });

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, producer, MOJO_HANDLE_SIGNAL_WRITABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           writable_producer_context, nullptr));

  // The producer is already writable, so arming should fail with relevant
  // information.
  constexpr size_t kMaxBlockingEvents = 3;
  uint32_t num_blocking_events = kMaxBlockingEvents;
  MojoTrapEvent blocking_events[kMaxBlockingEvents] = {
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])}};
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_events[0]));
  EXPECT_EQ(1u, num_blocking_events);
  EXPECT_EQ(writable_producer_context, blocking_events[0].trigger_context);
  EXPECT_EQ(MOJO_RESULT_OK, blocking_events[0].result);
  EXPECT_TRUE(blocking_events[0].signals_state.satisfied_signals &
              MOJO_HANDLE_SIGNAL_WRITABLE);

  // Write some data, but don't fill the pipe yet. Arming should fail again.
  WriteData(producer, kTestData);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_events[0]));
  EXPECT_EQ(1u, num_blocking_events);
  EXPECT_EQ(writable_producer_context, blocking_events[0].trigger_context);
  EXPECT_EQ(MOJO_RESULT_OK, blocking_events[0].result);
  EXPECT_TRUE(blocking_events[0].signals_state.satisfied_signals &
              MOJO_HANDLE_SIGNAL_WRITABLE);

  // Write more data, filling the pipe to capacity. Arming should succeed now.
  WriteData(producer, kTestData);
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  // Now read from the pipe, making the producer writable again. Should notify.
  EXPECT_EQ(kTestData, ReadData(consumer, sizeof(kTestData) - 1));
  wait.Wait();

  // Arming should fail again.
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_events[0]));
  EXPECT_EQ(1u, num_blocking_events);
  EXPECT_EQ(writable_producer_context, blocking_events[0].trigger_context);
  EXPECT_EQ(MOJO_RESULT_OK, blocking_events[0].result);
  EXPECT_TRUE(blocking_events[0].signals_state.satisfied_signals &
              MOJO_HANDLE_SIGNAL_WRITABLE);

  // Fill the pipe once more and arm the trap. Should succeed.
  WriteData(producer, kTestData);
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(producer));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(consumer));
}

TEST_F(TrapTest, CloseWatchedDataPipeConsumerHandle) {
  constexpr size_t kTestPipeCapacity = 8;
  MojoHandle producer, consumer;
  CreateDataPipe(&producer, &consumer, kTestPipeCapacity);

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  TriggerHelper helper;
  const uintptr_t readable_consumer_context = helper.CreateContextWithCancel(
      [](const MojoTrapEvent&) {}, [&] { wait.Signal(); });

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, consumer, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_consumer_context, nullptr));

  // Closing the consumer should fire a cancellation notification.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(consumer));
  wait.Wait();

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(producer));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
}

TEST_F(TrapTest, CloseWatchedDataPipeConsumerHandlePeer) {
  constexpr size_t kTestPipeCapacity = 8;
  MojoHandle producer, consumer;
  CreateDataPipe(&producer, &consumer, kTestPipeCapacity);

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  TriggerHelper helper;
  const uintptr_t readable_consumer_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, event.result);
        wait.Signal();
      });

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, consumer, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_consumer_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  // Closing the producer should fire a notification for an unsatisfiable
  // condition.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(producer));
  wait.Wait();

  // Now attempt to rearm and expect appropriate error feedback.
  constexpr size_t kMaxBlockingEvents = 3;
  uint32_t num_blocking_events = kMaxBlockingEvents;
  MojoTrapEvent blocking_events[kMaxBlockingEvents] = {
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])}};
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_events[0]));
  EXPECT_EQ(1u, num_blocking_events);
  EXPECT_EQ(readable_consumer_context, blocking_events[0].trigger_context);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, blocking_events[0].result);
  EXPECT_FALSE(blocking_events[0].signals_state.satisfiable_signals &
               MOJO_HANDLE_SIGNAL_READABLE);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(consumer));
}

TEST_F(TrapTest, CloseWatchedDataPipeProducerHandle) {
  constexpr size_t kTestPipeCapacity = 8;
  MojoHandle producer, consumer;
  CreateDataPipe(&producer, &consumer, kTestPipeCapacity);

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  TriggerHelper helper;
  const uintptr_t writable_producer_context = helper.CreateContextWithCancel(
      [](const MojoTrapEvent&) {}, [&] { wait.Signal(); });

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, producer, MOJO_HANDLE_SIGNAL_WRITABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           writable_producer_context, nullptr));

  // Closing the consumer should fire a cancellation notification.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(producer));
  wait.Wait();

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(consumer));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
}

TEST_F(TrapTest, CloseWatchedDataPipeProducerHandlePeer) {
  constexpr size_t kTestPipeCapacity = 8;
  MojoHandle producer, consumer;
  CreateDataPipe(&producer, &consumer, kTestPipeCapacity);

  const char kTestMessageFullCapacity[] = "xxxxxxxx";
  static_assert(sizeof(kTestMessageFullCapacity) - 1 == kTestPipeCapacity,
                "Invalid test message size for this test.");

  // Make the pipe unwritable initially.
  WriteData(producer, kTestMessageFullCapacity);

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  TriggerHelper helper;
  const uintptr_t writable_producer_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, event.result);
        wait.Signal();
      });

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, producer, MOJO_HANDLE_SIGNAL_WRITABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           writable_producer_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  // Closing the consumer should fire a notification for an unsatisfiable
  // condition, as the full data pipe can never be read from again and is
  // therefore permanently full and unwritable.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(consumer));
  wait.Wait();

  // Now attempt to rearm and expect appropriate error feedback.
  constexpr size_t kMaxBlockingEvents = 3;
  uint32_t num_blocking_events = kMaxBlockingEvents;
  MojoTrapEvent blocking_events[kMaxBlockingEvents] = {
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])}};
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_events[0]));
  EXPECT_EQ(1u, num_blocking_events);
  EXPECT_EQ(writable_producer_context, blocking_events[0].trigger_context);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, blocking_events[0].result);
  EXPECT_FALSE(blocking_events[0].signals_state.satisfiable_signals &
               MOJO_HANDLE_SIGNAL_WRITABLE);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(producer));
}

TEST_F(TrapTest, ArmWithNoTriggers) {
  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateTrap(&ExpectNoNotification, nullptr, &t));
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND, MojoArmTrap(t, nullptr, nullptr, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
}

TEST_F(TrapTest, DuplicateTriggerContext) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateTrap(&ExpectOnlyCancel, nullptr, &t));
  EXPECT_EQ(
      MOJO_RESULT_OK,
      MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED, 0, nullptr));
  EXPECT_EQ(
      MOJO_RESULT_ALREADY_EXISTS,
      MojoAddTrigger(t, b, MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED, 0, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
}

TEST_F(TrapTest, RemoveUnknownTrigger) {
  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateTrap(&ExpectNoNotification, nullptr, &t));
  EXPECT_EQ(MOJO_RESULT_NOT_FOUND, MojoRemoveTrigger(t, 1234, nullptr));
}

TEST_F(TrapTest, ArmWithTriggerConditionAlreadySatisfied) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateTrap(&ExpectOnlyCancel, nullptr, &t));
  EXPECT_EQ(
      MOJO_RESULT_OK,
      MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_WRITABLE,
                     MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED, 0, nullptr));

  // |a| is always writable, so we can never arm this trap.
  constexpr size_t kMaxBlockingEvents = 3;
  uint32_t num_blocking_events = kMaxBlockingEvents;
  MojoTrapEvent blocking_events[kMaxBlockingEvents] = {
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])}};
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_events[0]));
  EXPECT_EQ(1u, num_blocking_events);
  EXPECT_EQ(0u, blocking_events[0].trigger_context);
  EXPECT_EQ(MOJO_RESULT_OK, blocking_events[0].result);
  EXPECT_TRUE(blocking_events[0].signals_state.satisfied_signals &
              MOJO_HANDLE_SIGNAL_WRITABLE);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
}

TEST_F(TrapTest, ArmWithTriggerConditionAlreadyUnsatisfiable) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateTrap(&ExpectOnlyCancel, nullptr, &t));
  EXPECT_EQ(
      MOJO_RESULT_OK,
      MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED, 0, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));

  // |b| is closed and never wrote any messages, so |a| won't be readable again.
  // MojoArmTrap() should fail, incidcating as much.
  constexpr size_t kMaxBlockingEvents = 3;
  uint32_t num_blocking_events = kMaxBlockingEvents;
  MojoTrapEvent blocking_events[kMaxBlockingEvents] = {
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])}};
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_events[0]));
  EXPECT_EQ(1u, num_blocking_events);
  EXPECT_EQ(0u, blocking_events[0].trigger_context);
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION, blocking_events[0].result);
  EXPECT_TRUE(blocking_events[0].signals_state.satisfied_signals &
              MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  EXPECT_FALSE(blocking_events[0].signals_state.satisfiable_signals &
               MOJO_HANDLE_SIGNAL_READABLE);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
}

TEST_F(TrapTest, MultipleTriggers) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  base::WaitableEvent a_wait(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent b_wait(base::WaitableEvent::ResetPolicy::MANUAL,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  TriggerHelper helper;
  int num_a_notifications = 0;
  int num_b_notifications = 0;
  uintptr_t readable_a_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        num_a_notifications += 1;
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        a_wait.Signal();
      });
  uintptr_t readable_b_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        num_b_notifications += 1;
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        b_wait.Signal();
      });

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));

  // Add two independent triggers to trap |a| or |b| readability.
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_a_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, b, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_b_context, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  const char kMessage1[] = "things are happening";
  const char kMessage2[] = "ok. ok. ok. ok.";
  const char kMessage3[] = "plz wake up";

  // Writing to |b| should signal |a|'s watch.
  WriteMessage(b, kMessage1);
  a_wait.Wait();
  a_wait.Reset();

  // Subsequent messages on |b| should not trigger another notification.
  WriteMessage(b, kMessage2);
  WriteMessage(b, kMessage3);

  // Messages on |a| also shouldn't trigger |b|'s notification, since the
  // trap should be disarmed by now.
  WriteMessage(a, kMessage1);
  WriteMessage(a, kMessage2);
  WriteMessage(a, kMessage3);

  // Arming should fail. Since we only ask for at most one context's information
  // that's all we should get back. Which one we get is unspecified.
  constexpr size_t kMaxBlockingEvents = 3;
  uint32_t num_blocking_events = 1;
  MojoTrapEvent blocking_events[kMaxBlockingEvents] = {
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])},
      {sizeof(blocking_events[0])}};
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_events[0]));
  EXPECT_EQ(1u, num_blocking_events);
  EXPECT_TRUE(blocking_events[0].trigger_context == readable_a_context ||
              blocking_events[0].trigger_context == readable_b_context);
  EXPECT_EQ(MOJO_RESULT_OK, blocking_events[0].result);
  EXPECT_TRUE(blocking_events[0].signals_state.satisfied_signals &
              MOJO_HANDLE_SIGNAL_WRITABLE);

  // Now try arming again, verifying that both contexts are returned.
  num_blocking_events = kMaxBlockingEvents;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_events[0]));
  EXPECT_EQ(2u, num_blocking_events);
  EXPECT_EQ(MOJO_RESULT_OK, blocking_events[0].result);
  EXPECT_EQ(MOJO_RESULT_OK, blocking_events[1].result);
  EXPECT_TRUE(blocking_events[0].signals_state.satisfied_signals &
              MOJO_HANDLE_SIGNAL_WRITABLE);
  EXPECT_TRUE(blocking_events[1].signals_state.satisfied_signals &
              MOJO_HANDLE_SIGNAL_WRITABLE);
  EXPECT_TRUE((blocking_events[0].trigger_context == readable_a_context &&
               blocking_events[1].trigger_context == readable_b_context) ||
              (blocking_events[0].trigger_context == readable_b_context &&
               blocking_events[1].trigger_context == readable_a_context));

  // Flush out the test messages so we should be able to successfully rearm.
  EXPECT_EQ(kMessage1, ReadMessage(a));
  EXPECT_EQ(kMessage2, ReadMessage(a));
  EXPECT_EQ(kMessage3, ReadMessage(a));
  EXPECT_EQ(kMessage1, ReadMessage(b));
  EXPECT_EQ(kMessage2, ReadMessage(b));
  EXPECT_EQ(kMessage3, ReadMessage(b));

  // Add a trigger whose condition is always satisfied so we can't arm. Arming
  // should fail with only this new watch's information.
  uintptr_t writable_c_context =
      helper.CreateContext([](const MojoTrapEvent&) { NOTREACHED(); });
  MojoHandle c, d;
  CreateMessagePipe(&c, &d);

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, c, MOJO_HANDLE_SIGNAL_WRITABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           writable_c_context, nullptr));
  num_blocking_events = kMaxBlockingEvents;
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_events[0]));
  EXPECT_EQ(1u, num_blocking_events);
  EXPECT_EQ(writable_c_context, blocking_events[0].trigger_context);
  EXPECT_EQ(MOJO_RESULT_OK, blocking_events[0].result);
  EXPECT_TRUE(blocking_events[0].signals_state.satisfied_signals &
              MOJO_HANDLE_SIGNAL_WRITABLE);

  // Remove the new trigger and arming should succeed once again.
  EXPECT_EQ(MOJO_RESULT_OK, MojoRemoveTrigger(t, writable_c_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(c));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(TrapTest, ActivateOtherTriggerFromEventHandler) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  static const char kTestMessageToA[] = "hello a";
  static const char kTestMessageToB[] = "hello b";

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);

  TriggerHelper helper;
  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));

  uintptr_t readable_a_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        EXPECT_EQ("hello a", ReadMessage(a));

        // Re-arm the trap and signal |b|.
        EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));
        WriteMessage(a, kTestMessageToB);
      });

  uintptr_t readable_b_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        EXPECT_EQ(kTestMessageToB, ReadMessage(b));
        EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));
        wait.Signal();
      });

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_a_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, b, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_b_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  // Send a message to |a|. The relevant trigger should be notified and the
  // event handler should send a message to |b|, in turn notifying the other
  // trigger. The second event handler will signal |wait|.
  WriteMessage(b, kTestMessageToA);
  wait.Wait();
}

TEST_F(TrapTest, ActivateSameTriggerFromEventHandler) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  static const char kTestMessageToA[] = "hello a";

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);

  TriggerHelper helper;
  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));

  int expected_notifications = 10;
  uintptr_t readable_a_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        EXPECT_EQ("hello a", ReadMessage(a));

        EXPECT_GT(expected_notifications, 0);
        expected_notifications -= 1;
        if (expected_notifications == 0) {
          wait.Signal();
          return;
        } else {
          // Re-arm the trap and signal |a| again.
          EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));
          WriteMessage(b, kTestMessageToA);
        }
      });

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_a_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  // Send a message to |a|. When the trigger above is activated, the event
  // handler will rearm the trap and send another message to |a|. This will
  // happen until |expected_notifications| reaches 0.
  WriteMessage(b, kTestMessageToA);
  wait.Wait();
}

TEST_F(TrapTest, ImplicitRemoveOtherTriggerWithinEventHandler) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  MojoHandle c, d;
  CreateMessagePipe(&c, &d);

  static const char kTestMessageToA[] = "hi a";
  static const char kTestMessageToC[] = "hi c";

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);

  TriggerHelper helper;
  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));

  uintptr_t readable_a_context = helper.CreateContextWithCancel(
      [](const MojoTrapEvent&) { NOTREACHED(); }, [&] { wait.Signal(); });

  uintptr_t readable_c_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        EXPECT_EQ(kTestMessageToC, ReadMessage(c));

        EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

        // Must result in exactly ONE notification from the above trigger, for
        // CANCELLED only. Because we cannot dispatch notifications until the
        // stack unwinds, and because we must never dispatch non-cancellation
        // notifications for a handle once it's been closed, we must be certain
        // that cancellation due to closure preemptively invalidates any
        // pending non-cancellation notifications queued on the current
        // RequestContext, such as the one resulting from the WriteMessage here.
        WriteMessage(b, kTestMessageToA);
        EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));

        // Rearming should be fine since |a|'s trigger should already be
        // implicitly removed (even though the notification will not have
        // been invoked yet.)
        EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

        // Nothing interesting should happen as a result of this.
        EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
      });

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_a_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, c, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_c_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  WriteMessage(d, kTestMessageToC);
  wait.Wait();

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(c));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(TrapTest, ExplicitRemoveOtherTriggerWithinEventHandler) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  MojoHandle c, d;
  CreateMessagePipe(&c, &d);

  static const char kTestMessageToA[] = "hi a";
  static const char kTestMessageToC[] = "hi c";

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);

  TriggerHelper helper;
  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));

  uintptr_t readable_a_context =
      helper.CreateContext([](const MojoTrapEvent&) { NOTREACHED(); });

  uintptr_t readable_c_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        EXPECT_EQ(kTestMessageToC, ReadMessage(c));

        // Now rearm the trap.
        EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

        // Should result in no notifications from the above trigger, because the
        // trigger will have been removed by the time the event handler can
        // execute.
        WriteMessage(b, kTestMessageToA);
        WriteMessage(b, kTestMessageToA);
        EXPECT_EQ(MOJO_RESULT_OK,
                  MojoRemoveTrigger(t, readable_a_context, nullptr));

        // Rearming should be fine now.
        EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

        // Nothing interesting should happen as a result of these.
        EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
        EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));

        wait.Signal();
      });

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_a_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, c, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_c_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  WriteMessage(d, kTestMessageToC);
  wait.Wait();

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(c));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(TrapTest, NestedCancellation) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  MojoHandle c, d;
  CreateMessagePipe(&c, &d);

  static const char kTestMessageToA[] = "hey a";
  static const char kTestMessageToC[] = "hey c";
  static const char kTestMessageToD[] = "hey d";

  // This is a tricky test. It establishes a trigger on |b| using one trap and
  // triggers on |c| and |d| using another trap.
  //
  // A message is written to |d| to activate |c|'s trigger, and the resuling
  // event handler invocation does the folllowing:
  //   1. Writes to |a| to eventually activate |b|'s trigger.
  //   2. Rearms |c|'s trap.
  //   3. Writes to |d| to eventually activate |c|'s trigger again.
  //
  // Meanwhile, |b|'s event handler removes |c|'s trigger altogether before
  // writing to |c| to activate |d|'s trigger.
  //
  // The net result should be that |c|'s trigger only gets activated once (from
  // the first write to |d| above) and everyone else gets notified as expected.

  MojoHandle b_trap;
  MojoHandle cd_trap;
  TriggerHelper helper;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&b_trap));
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&cd_trap));

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  uintptr_t readable_d_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        EXPECT_EQ(kTestMessageToD, ReadMessage(d));
        wait.Signal();
      });

  int num_expected_c_notifications = 1;
  uintptr_t readable_c_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        EXPECT_GT(num_expected_c_notifications--, 0);

        // Trigger an eventual |readable_b_context| notification.
        WriteMessage(a, kTestMessageToA);

        EXPECT_EQ(kTestMessageToC, ReadMessage(c));
        EXPECT_EQ(MOJO_RESULT_OK,
                  MojoArmTrap(cd_trap, nullptr, nullptr, nullptr));

        // Trigger another eventual |readable_c_context| notification.
        WriteMessage(d, kTestMessageToC);
      });

  uintptr_t readable_b_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK,
                  MojoRemoveTrigger(cd_trap, readable_c_context, nullptr));

        EXPECT_EQ(MOJO_RESULT_OK,
                  MojoArmTrap(cd_trap, nullptr, nullptr, nullptr));

        WriteMessage(c, kTestMessageToD);
      });

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(b_trap, b, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_b_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(cd_trap, c, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_c_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(cd_trap, d, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_d_context, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(b_trap, nullptr, nullptr, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(cd_trap, nullptr, nullptr, nullptr));

  WriteMessage(d, kTestMessageToC);
  wait.Wait();

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(cd_trap));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b_trap));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(c));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(d));
}

TEST_F(TrapTest, RemoveSelfWithinEventHandler) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  static const char kTestMessageToA[] = "hey a";

  MojoHandle t;
  TriggerHelper helper;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);

  static uintptr_t readable_a_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);

        // There should be no problem removing this trigger from its own
        // notification invocation.
        EXPECT_EQ(MOJO_RESULT_OK,
                  MojoRemoveTrigger(t, readable_a_context, nullptr));
        EXPECT_EQ(kTestMessageToA, ReadMessage(a));

        // Arming should fail because there are no longer any registered
        // triggers on the trap.
        EXPECT_EQ(MOJO_RESULT_NOT_FOUND,
                  MojoArmTrap(t, nullptr, nullptr, nullptr));

        // And closing |a| should be fine (and should not invoke this
        // notification with MOJO_RESULT_CANCELLED) for the same reason.
        EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));

        wait.Signal();
      });

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_a_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  WriteMessage(b, kTestMessageToA);
  wait.Wait();

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
}

TEST_F(TrapTest, CloseTrapWithinEventHandler) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  static const char kTestMessageToA1[] = "hey a";
  static const char kTestMessageToA2[] = "hey a again";

  MojoHandle t;
  TriggerHelper helper;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);

  uintptr_t readable_a_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        EXPECT_EQ(kTestMessageToA1, ReadMessage(a));
        EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

        // There should be no problem closing this trap from its own
        // notification callback.
        EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));

        // And these should not trigger more notifications, because |t| has been
        // closed already.
        WriteMessage(b, kTestMessageToA2);
        EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
        EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));

        wait.Signal();
      });

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_a_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  WriteMessage(b, kTestMessageToA1);
  wait.Wait();
}

TEST_F(TrapTest, CloseTrapAfterImplicitTriggerRemoval) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  static const char kTestMessageToA[] = "hey a";

  MojoHandle t;
  TriggerHelper helper;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);

  uintptr_t readable_a_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        EXPECT_EQ(kTestMessageToA, ReadMessage(a));
        EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

        // This will cue up a notification for |MOJO_RESULT_CANCELLED|...
        EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));

        // ...but it should never fire because we close the trap here.
        EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));

        wait.Signal();
      });

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_a_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  WriteMessage(b, kTestMessageToA);
  wait.Wait();

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
}

TEST_F(TrapTest, OtherThreadRemovesTriggerDuringEventHandler) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  static const char kTestMessageToA[] = "hey a";

  MojoHandle t;
  TriggerHelper helper;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));

  base::WaitableEvent wait_for_notification(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  base::WaitableEvent wait_for_cancellation(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  static bool callback_done = false;
  uintptr_t readable_a_context = helper.CreateContextWithCancel(
      [&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        EXPECT_EQ(kTestMessageToA, ReadMessage(a));

        wait_for_notification.Signal();

        // Give the other thread sufficient time to race with the completion
        // of this callback. There should be no race, since the cancellation
        // notification must be mutually exclusive to this notification.
        base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));

        callback_done = true;
      },
      [&] {
        EXPECT_TRUE(callback_done);
        wait_for_cancellation.Signal();
      });

  ThreadedRunner runner(base::BindOnce(base::BindLambdaForTesting([&] {
    wait_for_notification.Wait();

    // Cancel the watch while the notification is still running.
    EXPECT_EQ(MOJO_RESULT_OK,
              MojoRemoveTrigger(t, readable_a_context, nullptr));

    wait_for_cancellation.Wait();

    EXPECT_TRUE(callback_done);
  })));
  runner.Start();

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_a_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  WriteMessage(b, kTestMessageToA);
  runner.Join();

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
}

TEST_F(TrapTest, TriggersRemoveEachOtherWithinEventHandlers) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  static const char kTestMessageToA[] = "hey a";
  static const char kTestMessageToB[] = "hey b";

  base::WaitableEvent wait_for_a_to_notify(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent wait_for_b_to_notify(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent wait_for_a_to_cancel(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent wait_for_b_to_cancel(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  MojoHandle a_trap;
  MojoHandle b_trap;
  TriggerHelper helper;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&a_trap));
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&b_trap));

  // We set up two traps, one triggered on |a| readability and one triggered on
  // |b| readability. Each removes the other's trigger from within its own event
  // handler. This should be safe, i.e., it should not deadlock in spite of the
  // fact that we also guarantee mutually exclusive event handler invocation
  // (including cancellations) on any given trap.
  bool a_cancelled = false;
  bool b_cancelled = false;
  static uintptr_t readable_b_context;
  uintptr_t readable_a_context = helper.CreateContextWithCancel(
      [&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        EXPECT_EQ(kTestMessageToA, ReadMessage(a));
        wait_for_a_to_notify.Signal();
        wait_for_b_to_notify.Wait();
        EXPECT_EQ(MOJO_RESULT_OK,
                  MojoRemoveTrigger(b_trap, readable_b_context, nullptr));
        EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b_trap));
      },
      [&] {
        a_cancelled = true;
        wait_for_a_to_cancel.Signal();
        wait_for_b_to_cancel.Wait();
      });

  readable_b_context = helper.CreateContextWithCancel(
      [&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        EXPECT_EQ(kTestMessageToB, ReadMessage(b));
        wait_for_b_to_notify.Signal();
        wait_for_a_to_notify.Wait();
        EXPECT_EQ(MOJO_RESULT_OK,
                  MojoRemoveTrigger(a_trap, readable_a_context, nullptr));
        EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a_trap));
      },
      [&] {
        b_cancelled = true;
        wait_for_b_to_cancel.Signal();
        wait_for_a_to_cancel.Wait();
      });

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(a_trap, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_a_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(a_trap, nullptr, nullptr, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(b_trap, b, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_b_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(b_trap, nullptr, nullptr, nullptr));

  ThreadedRunner runner(base::BindOnce(
      [](MojoHandle b) { WriteMessage(b, kTestMessageToA); }, b));
  runner.Start();

  // To enforce that the two traps run concurrently, wait until the WriteMessage
  // above has made a readable before firing the readable trap on b.
  wait_for_a_to_notify.Wait();

  WriteMessage(a, kTestMessageToB);

  wait_for_a_to_cancel.Wait();
  wait_for_b_to_cancel.Wait();
  runner.Join();

  EXPECT_TRUE(a_cancelled);
  EXPECT_TRUE(b_cancelled);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
}

TEST_F(TrapTest, AlwaysCancel) {
  // Basic sanity check to ensure that all possible ways to remove a trigger
  // result in a final MOJO_RESULT_CANCELLED notification.

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  MojoHandle t;
  TriggerHelper helper;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto ignore_event = [](const MojoTrapEvent&) {};
  auto signal_wait = [&] { wait.Signal(); };

  // Cancel via |MojoRemoveTrigger()|.
  uintptr_t context = helper.CreateContextWithCancel(ignore_event, signal_wait);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED, context,
                           nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoRemoveTrigger(t, context, nullptr));
  wait.Wait();
  wait.Reset();

  // Cancel by closing the trigger's watched handle.
  context = helper.CreateContextWithCancel(ignore_event, signal_wait);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED, context,
                           nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
  wait.Wait();
  wait.Reset();

  // Cancel by closing the trap handle.
  context = helper.CreateContextWithCancel(ignore_event, signal_wait);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, b, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED, context,
                           nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  wait.Wait();

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
}

TEST_F(TrapTest, ArmFailureCirculation) {
  // Sanity check to ensure that all ready trigger events will eventually be
  // returned over a finite number of calls to MojoArmTrap().

  constexpr size_t kNumTestPipes = 100;
  constexpr size_t kNumTestHandles = kNumTestPipes * 2;
  MojoHandle handles[kNumTestHandles];

  // Create a bunch of pipes and make sure they're all readable.
  for (size_t i = 0; i < kNumTestPipes; ++i) {
    CreateMessagePipe(&handles[i], &handles[i + kNumTestPipes]);
    WriteMessage(handles[i], "hey");
    WriteMessage(handles[i + kNumTestPipes], "hay");
    WaitForSignals(handles[i], MOJO_HANDLE_SIGNAL_READABLE);
    WaitForSignals(handles[i + kNumTestPipes], MOJO_HANDLE_SIGNAL_READABLE);
  }

  // Create a trap and watch all of them for readability.
  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateTrap(&ExpectOnlyCancel, nullptr, &t));
  for (size_t i = 0; i < kNumTestHandles; ++i) {
    EXPECT_EQ(
        MOJO_RESULT_OK,
        MojoAddTrigger(t, handles[i], MOJO_HANDLE_SIGNAL_READABLE,
                       MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED, i, nullptr));
  }

  // Keep trying to arm |t| until every trigger gets an entry in
  // |ready_contexts|. If MojoArmTrap() is well-behaved, this should terminate
  // eventually.
  std::set<uintptr_t> ready_contexts;
  while (ready_contexts.size() < kNumTestHandles) {
    uint32_t num_blocking_events = 1;
    MojoTrapEvent blocking_event = {sizeof(blocking_event)};
    EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
              MojoArmTrap(t, nullptr, &num_blocking_events, &blocking_event));
    EXPECT_EQ(1u, num_blocking_events);
    EXPECT_EQ(MOJO_RESULT_OK, blocking_event.result);
    ready_contexts.insert(blocking_event.trigger_context);
  }

  for (size_t i = 0; i < kNumTestHandles; ++i)
    EXPECT_EQ(MOJO_RESULT_OK, MojoClose(handles[i]));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
}

TEST_F(TrapTest, TriggerOnUnsatisfiedSignals) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  TriggerHelper helper;
  const uintptr_t readable_a_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        wait.Signal();
      });

  MojoHandle t;
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           readable_a_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  const char kMessage[] = "this is not a message";

  WriteMessage(b, kMessage);
  wait.Wait();

  // Now we know |a| is readable. Remove the trigger and add a new one to watch
  // for a not-readable state.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  const uintptr_t not_readable_a_context =
      helper.CreateContext([&](const MojoTrapEvent& event) {
        EXPECT_EQ(MOJO_RESULT_OK, event.result);
        wait.Signal();
      });
  EXPECT_EQ(MOJO_RESULT_OK, helper.CreateTrap(&t));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(t, a, MOJO_HANDLE_SIGNAL_READABLE,
                           MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED,
                           not_readable_a_context, nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(t, nullptr, nullptr, nullptr));

  // This should not block, because the event should be signaled by
  // |not_readable_a_context| when we read the only available message off of
  // |a|.
  wait.Reset();
  EXPECT_EQ(kMessage, ReadMessage(a));
  wait.Wait();

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(t));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(b));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(a));
}

base::RepeatingClosure g_do_random_thing_callback;

void ReadAllMessages(const MojoTrapEvent* event) {
  if (event->result == MOJO_RESULT_OK) {
    MojoHandle handle = static_cast<MojoHandle>(event->trigger_context);
    MojoMessageHandle message;
    while (MojoReadMessage(handle, nullptr, &message) == MOJO_RESULT_OK)
      MojoDestroyMessage(message);
  }

  constexpr size_t kNumRandomThingsToDoOnNotify = 5;
  for (size_t i = 0; i < kNumRandomThingsToDoOnNotify; ++i)
    g_do_random_thing_callback.Run();
}

MojoHandle RandomHandle(MojoHandle* handles, size_t size) {
  return handles[base::RandInt(0, static_cast<int>(size) - 1)];
}

void DoRandomThing(MojoHandle* traps,
                   size_t num_traps,
                   MojoHandle* watched_handles,
                   size_t num_watched_handles) {
  switch (base::RandInt(0, 10)) {
    case 0:
      MojoClose(RandomHandle(traps, num_traps));
      break;
    case 1:
      MojoClose(RandomHandle(watched_handles, num_watched_handles));
      break;
    case 2:
    case 3:
    case 4: {
      MojoMessageHandle message;
      ASSERT_EQ(MOJO_RESULT_OK, MojoCreateMessage(nullptr, &message));
      ASSERT_EQ(MOJO_RESULT_OK,
                MojoSetMessageContext(message, 1, nullptr, nullptr, nullptr));
      MojoWriteMessage(RandomHandle(watched_handles, num_watched_handles),
                       message, nullptr);
      break;
    }
    case 5:
    case 6: {
      MojoHandle t = RandomHandle(traps, num_traps);
      MojoHandle h = RandomHandle(watched_handles, num_watched_handles);
      MojoAddTrigger(t, h, MOJO_HANDLE_SIGNAL_READABLE,
                     MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                     static_cast<uintptr_t>(h), nullptr);
      break;
    }
    case 7:
    case 8: {
      uint32_t num_blocking_events = 1;
      MojoTrapEvent blocking_event = {sizeof(blocking_event)};
      if (MojoArmTrap(RandomHandle(traps, num_traps), nullptr,
                      &num_blocking_events,
                      &blocking_event) == MOJO_RESULT_FAILED_PRECONDITION &&
          blocking_event.result == MOJO_RESULT_OK) {
        ReadAllMessages(&blocking_event);
      }
      break;
    }
    case 9:
    case 10: {
      MojoHandle t = RandomHandle(traps, num_traps);
      MojoHandle h = RandomHandle(watched_handles, num_watched_handles);
      MojoRemoveTrigger(t, static_cast<uintptr_t>(h), nullptr);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

TEST_F(TrapTest, ConcurrencyStressTest) {
  // Regression test for https://crbug.com/740044. Exercises racy usage of the
  // trap API to weed out potential crashes.

  constexpr size_t kNumTraps = 50;
  constexpr size_t kNumWatchedHandles = 50;
  static_assert(kNumWatchedHandles % 2 == 0, "Invalid number of test handles.");

  constexpr size_t kNumThreads = 10;
  static constexpr size_t kNumOperationsPerThread = 400;

  MojoHandle traps[kNumTraps];
  MojoHandle watched_handles[kNumWatchedHandles];
  g_do_random_thing_callback = base::BindRepeating(
      &DoRandomThing, traps, kNumTraps, watched_handles, kNumWatchedHandles);

  for (size_t i = 0; i < kNumTraps; ++i)
    MojoCreateTrap(&ReadAllMessages, nullptr, &traps[i]);
  for (size_t i = 0; i < kNumWatchedHandles; i += 2)
    CreateMessagePipe(&watched_handles[i], &watched_handles[i + 1]);

  std::unique_ptr<ThreadedRunner> threads[kNumThreads];
  for (size_t i = 0; i < kNumThreads; ++i) {
    threads[i] = std::make_unique<ThreadedRunner>(base::BindOnce([] {
      for (size_t i = 0; i < kNumOperationsPerThread; ++i)
        g_do_random_thing_callback.Run();
    }));
    threads[i]->Start();
  }
  for (size_t i = 0; i < kNumThreads; ++i)
    threads[i]->Join();
  for (size_t i = 0; i < kNumTraps; ++i)
    MojoClose(traps[i]);
  for (size_t i = 0; i < kNumWatchedHandles; ++i)
    MojoClose(watched_handles[i]);
}

}  // namespace
}  // namespace core
}  // namespace mojo
