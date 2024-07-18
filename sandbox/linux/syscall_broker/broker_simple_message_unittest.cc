// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "sandbox/linux/syscall_broker/broker_simple_message.h"

#include <linux/kcmp.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "sandbox/linux/syscall_broker/broker_channel.h"
#include "sandbox/linux/syscall_broker/broker_simple_message.h"
#include "sandbox/linux/system_headers/linux_syscalls.h"
#include "sandbox/linux/tests/test_utils.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

namespace syscall_broker {

namespace {

void PostWaitableEventToThread(base::Thread* thread,
                               base::WaitableEvent* wait_event) {
  thread->task_runner()->PostTask(FROM_HERE,
                                  base::BindOnce(&base::WaitableEvent::Signal,
                                                 base::Unretained(wait_event)));
}

}  // namespace

class ExpectedResultValue {
 public:
  virtual bool NextMessagePieceMatches(BrokerSimpleMessage* message) = 0;
  virtual size_t Size() = 0;
};

class ExpectedResultDataValue : public ExpectedResultValue {
 public:
  ExpectedResultDataValue(const char* data, size_t length);

  bool NextMessagePieceMatches(BrokerSimpleMessage* message) override;
  size_t Size() override;

 private:
  const char* data_;
  size_t length_;
  ExpectedResultDataValue();
};

class ExpectedResultIntValue : public ExpectedResultValue {
 public:
  explicit ExpectedResultIntValue(int value);

  bool NextMessagePieceMatches(BrokerSimpleMessage* message) override;
  size_t Size() override;

 private:
  int value_;

  ExpectedResultIntValue();
};

class BrokerSimpleMessageTestHelper {
 public:
  static bool MessageContentMatches(const BrokerSimpleMessage& message,
                                    const uint8_t* content,
                                    size_t length);

  static void SendMsg(int write_fd, BrokerSimpleMessage* message, int fd);

  static void RecvMsg(BrokerChannel::EndPoint* ipc_reader,
                      ExpectedResultValue** expected_values,
                      int expected_values_length);

  static void RecvMsgAndReply(BrokerChannel::EndPoint* ipc_reader,
                              ExpectedResultValue** expected_values,
                              int expected_values_length,
                              const char* response_msg,
                              int fd);

  static void RecvMsgBadRead(BrokerChannel::EndPoint* ipc_reader, int fd);

  // Helper functions to write the respective BrokerSimpleMessage::EntryType to
  // a buffer. Returns the pointer to the memory right after where the value
  // was written to in |dst|.
  static uint8_t* WriteDataType(uint8_t* dst);
  static uint8_t* WriteIntType(uint8_t* dst);

  static size_t entry_type_size() {
    return sizeof(BrokerSimpleMessage::EntryType);
  }
};

ExpectedResultDataValue::ExpectedResultDataValue() {}

ExpectedResultDataValue::ExpectedResultDataValue(const char* data,
                                                 size_t length)
    : data_(data), length_(length) {}

bool ExpectedResultDataValue::NextMessagePieceMatches(
    BrokerSimpleMessage* message) {
  const char* next_data;
  size_t next_length;

  if (!message->ReadData(&next_data, &next_length))
    return false;

  if (next_length != length_)
    return false;

  return strncmp(data_, next_data, length_) == 0;
}

size_t ExpectedResultDataValue::Size() {
  return sizeof(size_t) + length_ * sizeof(char) +
         BrokerSimpleMessageTestHelper::entry_type_size();
}

ExpectedResultIntValue::ExpectedResultIntValue() {}

ExpectedResultIntValue::ExpectedResultIntValue(int value) : value_(value) {}

bool ExpectedResultIntValue::NextMessagePieceMatches(
    BrokerSimpleMessage* message) {
  int next_value;

  if (!message->ReadInt(&next_value))
    return false;

  return next_value == value_;
}

size_t ExpectedResultIntValue::Size() {
  return sizeof(int) + BrokerSimpleMessageTestHelper::entry_type_size();
}

// static
bool BrokerSimpleMessageTestHelper::MessageContentMatches(
    const BrokerSimpleMessage& message,
    const uint8_t* content,
    size_t length) {
  return length == message.length_ &&
         memcmp(message.message_, content, length) == 0;
}

// static
void BrokerSimpleMessageTestHelper::SendMsg(int write_fd,
                                            BrokerSimpleMessage* message,
                                            int fd) {
  EXPECT_NE(-1, message->SendMsg(write_fd, fd));
}

// static
void BrokerSimpleMessageTestHelper::RecvMsg(
    BrokerChannel::EndPoint* ipc_reader,
    ExpectedResultValue** expected_values,
    int expected_values_length) {
  base::ScopedFD return_fd;
  BrokerSimpleMessage message;
  ssize_t len = message.RecvMsgWithFlags(ipc_reader->get(), 0, &return_fd);

  EXPECT_LE(0, len) << "RecvMsgWithFlags response invalid";

  size_t expected_message_size = 0;
  for (int i = 0; i < expected_values_length; i++) {
    ExpectedResultValue* expected_result = expected_values[i];
    EXPECT_TRUE(expected_result->NextMessagePieceMatches(&message));
    expected_message_size += expected_result->Size();
  }

  EXPECT_EQ(static_cast<ssize_t>(expected_message_size), len);
}

// static
void BrokerSimpleMessageTestHelper::RecvMsgBadRead(
    BrokerChannel::EndPoint* ipc_reader,
    int fd) {
  base::ScopedFD return_fd;
  BrokerSimpleMessage message;
  ssize_t len = message.RecvMsgWithFlags(ipc_reader->get(), 0, &return_fd);

  EXPECT_LE(0, len) << "RecvMSgWithFlags response invalid";

  // Try to read a string instead of the int.
  const char* bad_str;
  EXPECT_FALSE(message.ReadString(&bad_str));

  // Now try to read the int, the message should be marked as broken at this
  // point.
  int expected_int;
  EXPECT_FALSE(message.ReadInt(&expected_int));

  // Now try to read the actual string.
  const char* expected_str;
  EXPECT_FALSE(message.ReadString(&expected_str));

  BrokerSimpleMessage response_message;
  SendMsg(return_fd.get(), &response_message, -1);
}

// static
void BrokerSimpleMessageTestHelper::RecvMsgAndReply(
    BrokerChannel::EndPoint* ipc_reader,
    ExpectedResultValue** expected_values,
    int expected_values_length,
    const char* response_msg,
    int fd) {
  base::ScopedFD return_fd;
  BrokerSimpleMessage message;
  ssize_t len = message.RecvMsgWithFlags(ipc_reader->get(), 0, &return_fd);

  EXPECT_LT(0, len);

  size_t expected_message_size = 0;
  for (int i = 0; i < expected_values_length; i++) {
    ExpectedResultValue* expected_result = expected_values[i];
    EXPECT_TRUE(expected_result->NextMessagePieceMatches(&message));
    expected_message_size += expected_result->Size();
  }

  EXPECT_EQ(expected_message_size, static_cast<size_t>(len));

  BrokerSimpleMessage response_message;
  response_message.AddDataToMessage(response_msg, strlen(response_msg) + 1);
  SendMsg(return_fd.get(), &response_message, -1);
}

// static
uint8_t* BrokerSimpleMessageTestHelper::WriteDataType(uint8_t* dst) {
  BrokerSimpleMessage::EntryType type = BrokerSimpleMessage::EntryType::DATA;
  memcpy(dst, &type, sizeof(BrokerSimpleMessage::EntryType));
  return dst + sizeof(BrokerSimpleMessage::EntryType);
}

// static
uint8_t* BrokerSimpleMessageTestHelper::WriteIntType(uint8_t* dst) {
  BrokerSimpleMessage::EntryType type = BrokerSimpleMessage::EntryType::INT;
  memcpy(dst, &type, sizeof(BrokerSimpleMessage::EntryType));
  return dst + sizeof(BrokerSimpleMessage::EntryType);
}

TEST(BrokerSimpleMessage, AddData) {
  const char data1[] = "hello, world";
  const char data2[] = "foobar";
  const int int1 = 42;
  const int int2 = 24;
  uint8_t message_content[BrokerSimpleMessage::kMaxMessageLength];
  uint8_t* next;
  size_t len;

  // Simple string
  {
    BrokerSimpleMessage message;
    message.AddDataToMessage(data1, strlen(data1));

    next = BrokerSimpleMessageTestHelper::WriteDataType(message_content);
    len = strlen(data1);
    memcpy(next, &len, sizeof(len));
    next = next + sizeof(len);
    memcpy(next, data1, strlen(data1));
    next = next + strlen(data1);

    EXPECT_TRUE(BrokerSimpleMessageTestHelper::MessageContentMatches(
        message, message_content, next - message_content));
  }

  // Simple int
  {
    BrokerSimpleMessage message;
    message.AddIntToMessage(int1);

    next = BrokerSimpleMessageTestHelper::WriteIntType(message_content);
    memcpy(next, &int1, sizeof(int));
    next = next + sizeof(int);

    EXPECT_TRUE(BrokerSimpleMessageTestHelper::MessageContentMatches(
        message, message_content, next - message_content));
  }

  // string then int
  {
    BrokerSimpleMessage message;
    message.AddDataToMessage(data1, strlen(data1));
    message.AddIntToMessage(int1);

    // string
    next = BrokerSimpleMessageTestHelper::WriteDataType(message_content);
    len = strlen(data1);
    memcpy(next, &len, sizeof(len));
    next = next + sizeof(len);
    memcpy(next, data1, strlen(data1));
    next = next + strlen(data1);

    // int
    next = BrokerSimpleMessageTestHelper::WriteIntType(next);
    memcpy(next, &int1, sizeof(int));
    next = next + sizeof(int);

    EXPECT_TRUE(BrokerSimpleMessageTestHelper::MessageContentMatches(
        message, message_content, next - message_content));
  }

  // int then string
  {
    BrokerSimpleMessage message;
    message.AddIntToMessage(int1);
    message.AddDataToMessage(data1, strlen(data1));

    // int
    next = BrokerSimpleMessageTestHelper::WriteIntType(message_content);
    memcpy(next, &int1, sizeof(int));
    next = next + sizeof(int);

    // string
    next = BrokerSimpleMessageTestHelper::WriteDataType(next);
    len = strlen(data1);
    memcpy(next, &len, sizeof(len));
    next = next + sizeof(len);
    memcpy(next, data1, strlen(data1));
    next = next + strlen(data1);

    EXPECT_TRUE(BrokerSimpleMessageTestHelper::MessageContentMatches(
        message, message_content, next - message_content));
  }

  // string int string int
  {
    BrokerSimpleMessage message;
    message.AddDataToMessage(data1, strlen(data1));
    message.AddIntToMessage(int1);
    message.AddDataToMessage(data2, strlen(data2));
    message.AddIntToMessage(int2);

    // string
    next = BrokerSimpleMessageTestHelper::WriteDataType(message_content);
    len = strlen(data1);
    memcpy(next, &len, sizeof(len));
    next = next + sizeof(len);
    memcpy(next, data1, strlen(data1));
    next = next + strlen(data1);

    // int
    next = BrokerSimpleMessageTestHelper::WriteIntType(next);
    memcpy(next, &int1, sizeof(int));
    next = next + sizeof(int);

    // string
    next = BrokerSimpleMessageTestHelper::WriteDataType(next);
    len = strlen(data2);
    memcpy(next, &len, sizeof(len));
    next = next + sizeof(len);
    memcpy(next, data2, strlen(data2));
    next = next + strlen(data2);

    // int
    next = BrokerSimpleMessageTestHelper::WriteIntType(next);
    memcpy(next, &int2, sizeof(int));
    next = next + sizeof(int);

    EXPECT_TRUE(BrokerSimpleMessageTestHelper::MessageContentMatches(
        message, message_content, next - message_content));
  }

  // Add too much data
  {
    BrokerSimpleMessage message;

    char foo[8192];
    memset(foo, 'x', sizeof(foo));

    EXPECT_FALSE(message.AddDataToMessage(foo, sizeof(foo)));
  }
}

TEST(BrokerSimpleMessage, SendAndRecvMsg) {
  const char data1[] = "hello, world";
  const char data2[] = "foobar";
  const int int1 = 42;
  const int int2 = 24;

  base::Thread message_thread("SendMessageThread");
  ASSERT_TRUE(message_thread.Start());
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Empty message case
  {
    SCOPED_TRACE("Empty message case");
    BrokerChannel::EndPoint ipc_reader;
    BrokerChannel::EndPoint ipc_writer;
    BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);

    BrokerSimpleMessage send_message;
    message_thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&BrokerSimpleMessageTestHelper::SendMsg,
                                  ipc_writer.get(), &send_message, -1));

    PostWaitableEventToThread(&message_thread, &wait_event);

    BrokerSimpleMessageTestHelper::RecvMsg(&ipc_reader, nullptr, 0);

    wait_event.Wait();
  }

  // Simple string case
  {
    SCOPED_TRACE("Simple string case");
    BrokerChannel::EndPoint ipc_reader;
    BrokerChannel::EndPoint ipc_writer;
    BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);

    BrokerSimpleMessage send_message;
    send_message.AddDataToMessage(data1, strlen(data1) + 1);
    message_thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&BrokerSimpleMessageTestHelper::SendMsg,
                                  ipc_writer.get(), &send_message, -1));

    PostWaitableEventToThread(&message_thread, &wait_event);

    ExpectedResultDataValue data1_value(data1, strlen(data1) + 1);
    ExpectedResultValue* expected_results[] = {&data1_value};

    BrokerSimpleMessageTestHelper::RecvMsg(&ipc_reader, expected_results,
                                           std::size(expected_results));

    wait_event.Wait();
  }

  // Simple int case
  {
    SCOPED_TRACE("Simple int case");
    BrokerChannel::EndPoint ipc_reader;
    BrokerChannel::EndPoint ipc_writer;
    BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);

    BrokerSimpleMessage send_message;
    send_message.AddIntToMessage(int1);
    message_thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&BrokerSimpleMessageTestHelper::SendMsg,
                                  ipc_writer.get(), &send_message, -1));

    PostWaitableEventToThread(&message_thread, &wait_event);

    ExpectedResultIntValue int1_value(int1);
    ExpectedResultValue* expected_results[] = {&int1_value};

    BrokerSimpleMessageTestHelper::RecvMsg(&ipc_reader, expected_results,
                                           std::size(expected_results));

    wait_event.Wait();
  }

  // Mixed message 1
  {
    SCOPED_TRACE("Mixed message 1");
    base::Thread message_thread_2("SendMessageThread");
    ASSERT_TRUE(message_thread_2.Start());
    BrokerChannel::EndPoint ipc_reader;
    BrokerChannel::EndPoint ipc_writer;
    BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);

    BrokerSimpleMessage send_message;
    send_message.AddDataToMessage(data1, strlen(data1) + 1);
    send_message.AddIntToMessage(int1);
    message_thread_2.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&BrokerSimpleMessageTestHelper::SendMsg,
                                  ipc_writer.get(), &send_message, -1));

    PostWaitableEventToThread(&message_thread_2, &wait_event);

    ExpectedResultDataValue data1_value(data1, strlen(data1) + 1);
    ExpectedResultIntValue int1_value(int1);
    ExpectedResultValue* expected_results[] = {&data1_value, &int1_value};

    BrokerSimpleMessageTestHelper::RecvMsg(&ipc_reader, expected_results,
                                           std::size(expected_results));

    wait_event.Wait();
  }

  // Mixed message 2
  {
    SCOPED_TRACE("Mixed message 2");
    base::Thread message_thread_2("SendMessageThread");
    ASSERT_TRUE(message_thread_2.Start());
    BrokerChannel::EndPoint ipc_reader;
    BrokerChannel::EndPoint ipc_writer;
    BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);

    BrokerSimpleMessage send_message;
    send_message.AddIntToMessage(int1);
    send_message.AddDataToMessage(data1, strlen(data1) + 1);
    send_message.AddDataToMessage(data2, strlen(data2) + 1);
    send_message.AddIntToMessage(int2);
    message_thread_2.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&BrokerSimpleMessageTestHelper::SendMsg,
                                  ipc_writer.get(), &send_message, -1));

    PostWaitableEventToThread(&message_thread_2, &wait_event);

    ExpectedResultDataValue data1_value(data1, strlen(data1) + 1);
    ExpectedResultDataValue data2_value(data2, strlen(data2) + 1);
    ExpectedResultIntValue int1_value(int1);
    ExpectedResultIntValue int2_value(int2);
    ExpectedResultValue* expected_results[] = {&int1_value, &data1_value,
                                               &data2_value, &int2_value};

    BrokerSimpleMessageTestHelper::RecvMsg(&ipc_reader, expected_results,
                                           std::size(expected_results));

    wait_event.Wait();
  }
}

TEST(BrokerSimpleMessage, SendRecvMsgSynchronous) {
  const char data1[] = "hello, world";
  const char data2[] = "foobar";
  const int int1 = 42;
  const int int2 = 24;
  const char reply_data1[] = "baz";

  base::Thread message_thread("SendMessageThread");
  ASSERT_TRUE(message_thread.Start());
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Simple string case
  {
    SCOPED_TRACE("Simple string case");
    BrokerChannel::EndPoint ipc_reader;
    BrokerChannel::EndPoint ipc_writer;
    BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);

    ExpectedResultDataValue data1_value(data1, strlen(data1) + 1);
    ExpectedResultValue* expected_results[] = {&data1_value};
    message_thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&BrokerSimpleMessageTestHelper::RecvMsgAndReply,
                       &ipc_reader, expected_results,
                       std::size(expected_results), reply_data1, -1));

    PostWaitableEventToThread(&message_thread, &wait_event);

    BrokerSimpleMessage send_message;
    send_message.AddDataToMessage(data1, strlen(data1) + 1);
    BrokerSimpleMessage reply_message;
    base::ScopedFD returned_fd;
    ssize_t len = send_message.SendRecvMsgWithFlags(
        ipc_writer.get(), 0, &returned_fd, &reply_message);

    ExpectedResultDataValue response_value(reply_data1,
                                           strlen(reply_data1) + 1);

    EXPECT_TRUE(response_value.NextMessagePieceMatches(&reply_message));
    EXPECT_EQ(len, static_cast<ssize_t>(response_value.Size()));

    wait_event.Wait();
  }

  // Simple int case
  {
    SCOPED_TRACE("Simple int case");
    BrokerChannel::EndPoint ipc_reader;
    BrokerChannel::EndPoint ipc_writer;
    BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);

    ExpectedResultIntValue int1_value(int1);
    ExpectedResultValue* expected_results[] = {&int1_value};
    message_thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&BrokerSimpleMessageTestHelper::RecvMsgAndReply,
                       &ipc_reader, expected_results,
                       std::size(expected_results), reply_data1, -1));

    PostWaitableEventToThread(&message_thread, &wait_event);

    BrokerSimpleMessage send_message;
    send_message.AddIntToMessage(int1);
    BrokerSimpleMessage reply_message;
    base::ScopedFD returned_fd;
    ssize_t len = send_message.SendRecvMsgWithFlags(
        ipc_writer.get(), 0, &returned_fd, &reply_message);

    ExpectedResultDataValue response_value(reply_data1,
                                           strlen(reply_data1) + 1);

    EXPECT_TRUE(response_value.NextMessagePieceMatches(&reply_message));
    EXPECT_EQ(len, static_cast<ssize_t>(response_value.Size()));

    wait_event.Wait();
  }

  // Mixed message 1
  {
    SCOPED_TRACE("Mixed message 1");
    BrokerChannel::EndPoint ipc_reader;
    BrokerChannel::EndPoint ipc_writer;
    BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);

    ExpectedResultDataValue data1_value(data1, strlen(data1) + 1);
    ExpectedResultIntValue int1_value(int1);
    ExpectedResultValue* expected_results[] = {&data1_value, &int1_value};
    message_thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&BrokerSimpleMessageTestHelper::RecvMsgAndReply,
                       &ipc_reader, expected_results,
                       std::size(expected_results), reply_data1, -1));

    PostWaitableEventToThread(&message_thread, &wait_event);

    BrokerSimpleMessage send_message;
    send_message.AddDataToMessage(data1, strlen(data1) + 1);
    send_message.AddIntToMessage(int1);
    BrokerSimpleMessage reply_message;
    base::ScopedFD returned_fd;
    ssize_t len = send_message.SendRecvMsgWithFlags(
        ipc_writer.get(), 0, &returned_fd, &reply_message);

    ExpectedResultDataValue response_value(reply_data1,
                                           strlen(reply_data1) + 1);

    EXPECT_TRUE(response_value.NextMessagePieceMatches(&reply_message));
    EXPECT_EQ(len, static_cast<ssize_t>(response_value.Size()));

    wait_event.Wait();
  }

  // Mixed message 2
  {
    SCOPED_TRACE("Mixed message 2");
    BrokerChannel::EndPoint ipc_reader;
    BrokerChannel::EndPoint ipc_writer;
    BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);

    ExpectedResultDataValue data1_value(data1, strlen(data1) + 1);
    ExpectedResultIntValue int1_value(int1);
    ExpectedResultIntValue int2_value(int2);
    ExpectedResultDataValue data2_value(data2, strlen(data2) + 1);
    ExpectedResultValue* expected_results[] = {&data1_value, &int1_value,
                                               &int2_value, &data2_value};
    message_thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&BrokerSimpleMessageTestHelper::RecvMsgAndReply,
                       &ipc_reader, expected_results,
                       std::size(expected_results), reply_data1, -1));

    PostWaitableEventToThread(&message_thread, &wait_event);

    BrokerSimpleMessage send_message;
    send_message.AddDataToMessage(data1, strlen(data1) + 1);
    send_message.AddIntToMessage(int1);
    send_message.AddIntToMessage(int2);
    send_message.AddDataToMessage(data2, strlen(data2) + 1);
    BrokerSimpleMessage reply_message;
    base::ScopedFD returned_fd;
    ssize_t len = send_message.SendRecvMsgWithFlags(
        ipc_writer.get(), 0, &returned_fd, &reply_message);

    ExpectedResultDataValue response_value(reply_data1,
                                           strlen(reply_data1) + 1);

    EXPECT_TRUE(response_value.NextMessagePieceMatches(&reply_message));
    EXPECT_EQ(len, static_cast<ssize_t>(response_value.Size()));

    wait_event.Wait();
  }

  // Bad read case
  {
    SCOPED_TRACE("Bad read case");
    BrokerChannel::EndPoint ipc_reader;
    BrokerChannel::EndPoint ipc_writer;
    BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);

    message_thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&BrokerSimpleMessageTestHelper::RecvMsgBadRead,
                       &ipc_reader, -1));

    PostWaitableEventToThread(&message_thread, &wait_event);

    BrokerSimpleMessage send_message;
    EXPECT_TRUE(send_message.AddIntToMessage(5));
    EXPECT_TRUE(send_message.AddStringToMessage("test"));
    BrokerSimpleMessage reply_message;
    base::ScopedFD returned_fd;
    ssize_t len = send_message.SendRecvMsgWithFlags(
        ipc_writer.get(), 0, &returned_fd, &reply_message);

    EXPECT_GE(len, 0);

    wait_event.Wait();
  }
}

namespace {
// Adds a gtest failure and returns false iff any of the following conditions
// are true:
// 1. |fd1| or |fd2| are invalid fds
// 2. Kcmp fails
// 3. fd1 and fd2 do not compare equal under kcmp.
bool CheckKcmpResult(int fd1, int fd2) {
  if (fd1 < 0) {
    ADD_FAILURE() << "fd1 invalid";
    return false;
  }
  if (fd2 < 0) {
    ADD_FAILURE() << "fd2 invalid";
    return false;
  }
  pid_t pid = getpid();
  int ret = syscall(__NR_kcmp, pid, pid, KCMP_FILE, fd1, fd2);
  if (ret < 0) {
    ADD_FAILURE() << "Kcmp failed, errno = " << errno;
    return false;
  }
  if (ret != 0) {
    ADD_FAILURE() << "File description did not compare equal to stdout. Kcmp("
                  << fd1 << ", " << fd2 << ") = " << ret;
    return false;
  }

  return true;
}

// Receives an fd over |ipc_reader|, and if it does not point to the same
// description as stdout, prints a message and returns false.
// On any other error, also prints a message and returns false.
void ReceiveStdoutDupFd(BrokerChannel::EndPoint* ipc_reader) {
  // Receive an fd from |ipc_reader|.
  base::ScopedFD recv_fd;

  BrokerSimpleMessage msg;
  ssize_t len = msg.RecvMsgWithFlags(ipc_reader->get(), 0, &recv_fd);
  ASSERT_GE(len, 0) << "Error on RecvMsgWithFlags, errno = " << errno;

  CheckKcmpResult(STDOUT_FILENO, recv_fd.get());
}

void ReceiveTwoDupFds(BrokerChannel::EndPoint* ipc_reader) {
  // Receive two fds from |ipc_reader|.
  BrokerSimpleMessage msg;
  base::ScopedFD recv_fds[2];
  ssize_t len =
      msg.RecvMsgWithFlagsMultipleFds(ipc_reader->get(), 0, {recv_fds});
  ASSERT_GE(len, 0) << "Error on RecvMsgWithFlags, errno = " << errno;

  CheckKcmpResult(STDOUT_FILENO, recv_fds[0].get());
  CheckKcmpResult(STDIN_FILENO, recv_fds[1].get());
}

void ReceiveThreeFdsSendTwoBack(BrokerChannel::EndPoint* ipc_reader) {
  // Receive two fds from |ipc_reader|.
  BrokerSimpleMessage msg;
  base::ScopedFD recv_fds[3];
  ssize_t len =
      msg.RecvMsgWithFlagsMultipleFds(ipc_reader->get(), 0, {recv_fds});
  ASSERT_GE(len, 0) << "Error on RecvMsgWithFlags, errno = " << errno;
  ASSERT_TRUE(recv_fds[0].is_valid());

  if (!CheckKcmpResult(STDOUT_FILENO, recv_fds[1].get()) ||
      !CheckKcmpResult(STDIN_FILENO, recv_fds[2].get())) {
    return;
  }

  BrokerSimpleMessage resp;
  int send_fds[2];
  send_fds[0] = recv_fds[1].get();
  send_fds[1] = recv_fds[2].get();
  resp.AddIntToMessage(0);  // Dummy int to send message
  ASSERT_TRUE(resp.SendMsgMultipleFds(recv_fds[0].get(), {send_fds}));
}
}  // namespace

class BrokerSimpleMessageFdTest : public testing::Test {
 public:
  void SetUp() override {
    task_environment_ = std::make_unique<base::test::TaskEnvironment>();
  }

  bool SkipIfKcmpNotSupported() {
    pid_t pid = getpid();
    if (syscall(__NR_kcmp, pid, pid, KCMP_FILE, STDOUT_FILENO, STDOUT_FILENO) <
        0) {
      LOG(INFO) << "Skipping test, kcmp not supported.";
      return false;
    }
    return true;
  }

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
};

// Passes one fd with RecvMsg, SendMsg.
TEST_F(BrokerSimpleMessageFdTest, PassOneFd) {
  if (!SkipIfKcmpNotSupported())
    return;

  BrokerChannel::EndPoint ipc_reader;
  BrokerChannel::EndPoint ipc_writer;
  BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);
  base::RunLoop run_loop;

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, base::BindOnce(&ReceiveStdoutDupFd, &ipc_reader),
      run_loop.QuitClosure());

  BrokerSimpleMessage msg;
  msg.AddIntToMessage(0);  // Must add a dummy value to send the message.
  ASSERT_TRUE(msg.SendMsg(ipc_writer.get(), STDOUT_FILENO));

  run_loop.Run();
}

TEST_F(BrokerSimpleMessageFdTest, PassTwoFds) {
  if (!SkipIfKcmpNotSupported())
    return;

  BrokerChannel::EndPoint ipc_reader;
  BrokerChannel::EndPoint ipc_writer;
  BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);
  base::RunLoop run_loop;

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, base::BindOnce(&ReceiveTwoDupFds, &ipc_reader),
      run_loop.QuitClosure());

  BrokerSimpleMessage msg;
  msg.AddIntToMessage(0);  // Must add a dummy value to send the message.
  int send_fds[2];
  send_fds[0] = STDOUT_FILENO;
  send_fds[1] = STDIN_FILENO;
  ASSERT_TRUE(msg.SendMsgMultipleFds(ipc_writer.get(), {send_fds}));

  run_loop.Run();
}

TEST_F(BrokerSimpleMessageFdTest, SynchronousPassTwoFds) {
  if (!SkipIfKcmpNotSupported())
    return;

  BrokerChannel::EndPoint ipc_reader;
  BrokerChannel::EndPoint ipc_writer;
  BrokerChannel::CreatePair(&ipc_reader, &ipc_writer);
  base::RunLoop run_loop;

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, base::BindOnce(&ReceiveThreeFdsSendTwoBack, &ipc_reader),
      run_loop.QuitClosure());

  BrokerSimpleMessage msg, reply;
  msg.AddIntToMessage(0);  // Must add a dummy value to send the message.
  int send_fds[2];
  send_fds[0] = STDOUT_FILENO;
  send_fds[1] = STDIN_FILENO;
  base::ScopedFD result_fds[2];
  msg.SendRecvMsgWithFlagsMultipleFds(ipc_writer.get(), 0, {send_fds},
                                      {result_fds}, &reply);

  run_loop.Run();

  ASSERT_TRUE(CheckKcmpResult(STDOUT_FILENO, result_fds[0].get()));
  ASSERT_TRUE(CheckKcmpResult(STDIN_FILENO, result_fds[1].get()));
}

}  // namespace syscall_broker

}  // namespace sandbox
