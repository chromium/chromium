// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/c/system/quota.h"

namespace mojo {
namespace core {
namespace {

using QuotaTest = test::MojoTestBase;

void QuotaExceededEventHandler(const MojoTrapEvent* event) {
  // Always treat trigger context as the address of a bool to set to |true|.
  if (event->result == MOJO_RESULT_OK)
    *reinterpret_cast<bool*>(event->trigger_context) = true;
}

TEST_F(QuotaTest, InvalidArguments) {
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSetQuota(MOJO_HANDLE_INVALID,
                         MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH, 2, nullptr));

  const MojoQuotaType kInvalidQuotaType = 0xfffffffful;
  MojoHandle message_pipe0, message_pipe1;
  CreateMessagePipe(&message_pipe0, &message_pipe1);
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSetQuota(message_pipe0, kInvalidQuotaType, 0, nullptr));

  const MojoSetQuotaOptions kInvalidSetQuotaOptions = {0};
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSetQuota(message_pipe0, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH, 0,
                         &kInvalidSetQuotaOptions));

  uint64_t limit = 0;
  uint64_t usage = 0;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoQueryQuota(message_pipe0, kInvalidQuotaType, nullptr, &limit,
                           &usage));

  const MojoQueryQuotaOptions kInvalidQueryQuotaOptions = {0};
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoQueryQuota(message_pipe0, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH,
                           &kInvalidQueryQuotaOptions, &limit, &usage));

  MojoClose(message_pipe0);
  MojoClose(message_pipe1);

  MojoHandle producer, consumer;
  CreateDataPipe(&producer, &consumer, 1);
  EXPECT_EQ(
      MOJO_RESULT_INVALID_ARGUMENT,
      MojoSetQuota(producer, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH, 0, nullptr));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSetQuota(producer, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_MEMORY_SIZE, 0,
                         nullptr));
  EXPECT_EQ(
      MOJO_RESULT_INVALID_ARGUMENT,
      MojoSetQuota(consumer, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH, 0, nullptr));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSetQuota(consumer, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_MEMORY_SIZE, 0,
                         nullptr));
  MojoClose(producer);
  MojoClose(consumer);
}

TEST_F(QuotaTest, BasicReceiveQueueLength) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  uint64_t limit = 0;
  uint64_t usage = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(a, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH, nullptr,
                           &limit, &usage));
  EXPECT_EQ(MOJO_QUOTA_LIMIT_NONE, limit);
  EXPECT_EQ(0u, usage);

  const uint64_t kTestLimit = 42;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoSetQuota(a, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH, kTestLimit,
                         nullptr));

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(a, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH, nullptr,
                           &limit, &usage));
  EXPECT_EQ(kTestLimit, limit);
  EXPECT_EQ(0u, usage);

  const std::string kTestMessage = "doot";
  WriteMessage(b, kTestMessage);
  WaitForSignals(a, MOJO_HANDLE_SIGNAL_READABLE);

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(a, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH, nullptr,
                           &limit, &usage));
  EXPECT_EQ(kTestLimit, limit);
  EXPECT_EQ(1u, usage);
}

TEST_F(QuotaTest, BasicReceiveQueueMemorySize) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  uint64_t limit = 0;
  uint64_t usage = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(a, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_MEMORY_SIZE,
                           nullptr, &limit, &usage));
  EXPECT_EQ(MOJO_QUOTA_LIMIT_NONE, limit);
  EXPECT_EQ(0u, usage);

  const uint64_t kTestLimit = 42;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoSetQuota(a, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_MEMORY_SIZE,
                         kTestLimit, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(a, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_MEMORY_SIZE,
                           nullptr, &limit, &usage));
  EXPECT_EQ(kTestLimit, limit);
  EXPECT_EQ(0u, usage);

  const std::string kTestMessage = "doot";
  WriteMessage(b, kTestMessage);
  WaitForSignals(a, MOJO_HANDLE_SIGNAL_READABLE);

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(a, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_MEMORY_SIZE,
                           nullptr, &limit, &usage));
  EXPECT_EQ(kTestLimit, limit);
  EXPECT_EQ(usage, kTestMessage.size());

  MojoClose(a);
  MojoClose(b);
}

TEST_F(QuotaTest, ReceiveQueueLengthLimitExceeded) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  const uint64_t kMaxMessages = 1;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoSetQuota(b, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH, kMaxMessages,
                         nullptr));

  MojoHandleSignalsState signals;
  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &signals));
  EXPECT_FALSE(signals.satisfied_signals & MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  const std::string kTestMessage = "this message is lit, fam";
  WriteMessage(a, kTestMessage);
  WaitForSignals(b, MOJO_HANDLE_SIGNAL_READABLE);

  uint64_t limit = 0;
  uint64_t usage = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(b, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH, nullptr,
                           &limit, &usage));
  EXPECT_EQ(kMaxMessages, limit);
  EXPECT_EQ(1u, usage);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &signals));
  EXPECT_FALSE(signals.satisfied_signals & MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  // Push the endpoint over quota and ensure that it signals accordingly.
  WriteMessage(a, kTestMessage);
  WaitForSignals(b, MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &signals));
  EXPECT_TRUE(signals.satisfied_signals & MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(b, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH, nullptr,
                           &limit, &usage));
  EXPECT_EQ(kMaxMessages, limit);
  EXPECT_EQ(2u, usage);

  // Read a message and wait for QUOTA_EXCEEDED to go back low.
  EXPECT_EQ(kTestMessage, ReadMessage(b));
  WaitForSignals(b, MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED,
                 MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &signals));
  EXPECT_FALSE(signals.satisfied_signals & MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(b, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH, nullptr,
                           &limit, &usage));
  EXPECT_EQ(kMaxMessages, limit);
  EXPECT_EQ(1u, usage);

  MojoClose(a);
  MojoClose(b);
}

TEST_F(QuotaTest, ReceiveQueueMemorySizeLimitExceeded) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  const uint64_t kMaxMessageBytes = 6;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoSetQuota(b, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_MEMORY_SIZE,
                         kMaxMessageBytes, nullptr));

  MojoHandleSignalsState signals;
  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &signals));
  EXPECT_FALSE(signals.satisfied_signals & MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  const std::string kTestMessage = "four";
  WriteMessage(a, kTestMessage);
  WaitForSignals(b, MOJO_HANDLE_SIGNAL_READABLE);

  uint64_t limit = 0;
  uint64_t usage = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(b, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_MEMORY_SIZE,
                           nullptr, &limit, &usage));
  EXPECT_EQ(kMaxMessageBytes, limit);
  EXPECT_EQ(kTestMessage.size(), usage);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &signals));
  EXPECT_FALSE(signals.satisfied_signals & MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  // Push the endpoint over quota and ensure that it signals accordingly.
  WriteMessage(a, kTestMessage);
  WaitForSignals(b, MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &signals));
  EXPECT_TRUE(signals.satisfied_signals & MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(b, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_MEMORY_SIZE,
                           nullptr, &limit, &usage));
  EXPECT_EQ(kMaxMessageBytes, limit);
  EXPECT_EQ(kTestMessage.size() * 2, usage);

  // Read a message and wait for QUOTA_EXCEEDED to go back low.
  EXPECT_EQ(kTestMessage, ReadMessage(b));
  WaitForSignals(b, MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED,
                 MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &signals));
  EXPECT_FALSE(signals.satisfied_signals & MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(b, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_MEMORY_SIZE,
                           nullptr, &limit, &usage));
  EXPECT_EQ(kMaxMessageBytes, limit);
  EXPECT_EQ(kTestMessage.size(), usage);

  MojoClose(a);
  MojoClose(b);
}

TEST_F(QuotaTest, BasicUnreadMessageCount) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  uint64_t limit = 0;
  uint64_t usage = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(a, MOJO_QUOTA_TYPE_UNREAD_MESSAGE_COUNT, nullptr,
                           &limit, &usage));
  EXPECT_EQ(MOJO_QUOTA_LIMIT_NONE, limit);
  EXPECT_EQ(0u, usage);

  const uint64_t kTestLimit = 42;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoSetQuota(a, MOJO_QUOTA_TYPE_UNREAD_MESSAGE_COUNT, kTestLimit,
                         nullptr));

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(a, MOJO_QUOTA_TYPE_UNREAD_MESSAGE_COUNT, nullptr,
                           &limit, &usage));
  EXPECT_EQ(kTestLimit, limit);
  EXPECT_EQ(0u, usage);

  const std::string kTestMessage = "doot";
  WriteMessage(a, kTestMessage);

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(a, MOJO_QUOTA_TYPE_UNREAD_MESSAGE_COUNT, nullptr,
                           &limit, &usage));
  EXPECT_EQ(kTestLimit, limit);
  EXPECT_EQ(usage, 1u);

  MojoClose(a);
  MojoClose(b);
}

TEST_F(QuotaTest, UnreadMessageCountLimitExceeded) {
  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  const uint64_t kMaxUnreadMessageCount = 4;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoSetQuota(a, MOJO_QUOTA_TYPE_UNREAD_MESSAGE_COUNT,
                         kMaxUnreadMessageCount, nullptr));

  MojoHandleSignalsState signals;
  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(a, &signals));
  EXPECT_FALSE(signals.satisfied_signals & MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  const std::string kTestMessage = "msg";
  WriteMessage(a, kTestMessage);

  uint64_t limit = 0;
  uint64_t usage = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(a, MOJO_QUOTA_TYPE_UNREAD_MESSAGE_COUNT, nullptr,
                           &limit, &usage));
  EXPECT_EQ(kMaxUnreadMessageCount, limit);
  EXPECT_EQ(1u, usage);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(a, &signals));
  EXPECT_FALSE(signals.satisfied_signals & MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  // Push the endpoint over quota and ensure that it signals accordingly.
  WriteMessage(a, kTestMessage);
  WriteMessage(a, kTestMessage);
  WriteMessage(a, kTestMessage);
  WriteMessage(a, kTestMessage);
  WaitForSignals(a, MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(a, &signals));
  EXPECT_TRUE(signals.satisfied_signals & MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(a, MOJO_QUOTA_TYPE_UNREAD_MESSAGE_COUNT, nullptr,
                           &limit, &usage));
  EXPECT_EQ(kMaxUnreadMessageCount, limit);
  EXPECT_EQ(5u, usage);

  // Read the messages and wait for QUOTA_EXCEEDED on the other end to go back
  // low. There's some hysteresis in the signaling, so it's not sufficient to
  // read a single packet, but reading below the quota size should work.
  EXPECT_EQ(kTestMessage, ReadMessage(b));
  EXPECT_EQ(kTestMessage, ReadMessage(b));
  EXPECT_EQ(kTestMessage, ReadMessage(b));
  EXPECT_EQ(kTestMessage, ReadMessage(b));
  WaitForSignals(a, MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED,
                 MOJO_TRIGGER_CONDITION_SIGNALS_UNSATISFIED);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(a, &signals));
  EXPECT_FALSE(signals.satisfied_signals & MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(a, MOJO_QUOTA_TYPE_UNREAD_MESSAGE_COUNT, nullptr,
                           &limit, &usage));
  EXPECT_EQ(kMaxUnreadMessageCount, limit);
  EXPECT_LE(1u, usage);

  MojoClose(a);
  MojoClose(b);
}

TEST_F(QuotaTest, TrapQuotaExceeded) {
  // Simple sanity check to verify that QUOTA_EXCEEDED signals can be trapped
  // like any other signals.

  MojoHandle a, b;
  CreateMessagePipe(&a, &b);

  const uint64_t kMaxMessages = 42;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoSetQuota(b, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH, kMaxMessages,
                         nullptr));

  bool signal_event_fired = false;
  MojoHandle quota_trap;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoCreateTrap(&QuotaExceededEventHandler, nullptr, &quota_trap));
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAddTrigger(quota_trap, b, MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED,
                           MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                           reinterpret_cast<uintptr_t>(&signal_event_fired),
                           nullptr));
  EXPECT_EQ(MOJO_RESULT_OK, MojoArmTrap(quota_trap, nullptr, nullptr, nullptr));

  const std::string kTestMessage("sup");
  for (uint64_t i = 0; i < kMaxMessages; ++i)
    WriteMessage(a, kTestMessage);

  // We're at quota but not yet over.
  MojoHandleSignalsState signals;
  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &signals));
  EXPECT_FALSE(signals.satisfied_signals & MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);
  EXPECT_FALSE(signal_event_fired);

  // Push over quota. The event handler should be invoked before this returns.
  WriteMessage(a, kTestMessage);
  EXPECT_TRUE(signal_event_fired);

  EXPECT_EQ(MOJO_RESULT_OK, MojoQueryHandleSignalsState(b, &signals));
  EXPECT_TRUE(signals.satisfied_signals & MOJO_HANDLE_SIGNAL_QUOTA_EXCEEDED);

  uint64_t limit = 0;
  uint64_t usage = 0;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoQueryQuota(b, MOJO_QUOTA_TYPE_RECEIVE_QUEUE_LENGTH, nullptr,
                           &limit, &usage));
  EXPECT_EQ(kMaxMessages, limit);
  EXPECT_EQ(kMaxMessages + 1, usage);

  MojoClose(quota_trap);
  MojoClose(a);
  MojoClose(b);
}

}  // namespace
}  // namespace core
}  // namespace mojo
