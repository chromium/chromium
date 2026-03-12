// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_message_writer_impl.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "remoting/host/security_key/security_key_message.h"
#include "remoting/host/setup/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const remoting::SecurityKeyMessageType kTestMessageType =
    remoting::SecurityKeyMessageType::CONNECT;
const unsigned int kLargeMessageSizeBytes = 200000;
}  // namespace

namespace remoting {

class SecurityKeyMessageWriterImplTest : public testing::Test {
 public:
  SecurityKeyMessageWriterImplTest();

  SecurityKeyMessageWriterImplTest(const SecurityKeyMessageWriterImplTest&) =
      delete;
  SecurityKeyMessageWriterImplTest& operator=(
      const SecurityKeyMessageWriterImplTest&) = delete;

  ~SecurityKeyMessageWriterImplTest() override;

  // Run on a separate thread, this method reads the message written to the
  // output stream and returns the result.
  std::string ReadMessage(int payload_length_bytes);

 protected:
  // testing::Test interface.
  void SetUp() override;

  // Writes |kTestMessageType| and |payload| to the output stream and verifies
  // they were written correctly.
  void WriteMessageToOutput(const std::string& payload);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  std::unique_ptr<SecurityKeyMessageWriter> writer_;
  base::File read_file_;
  base::File write_file_;

  // Stores the result of the last read operation.
  std::string message_result_;
};

SecurityKeyMessageWriterImplTest::SecurityKeyMessageWriterImplTest() = default;

SecurityKeyMessageWriterImplTest::~SecurityKeyMessageWriterImplTest() = default;

std::string SecurityKeyMessageWriterImplTest::ReadMessage(
    int payload_length_bytes) {
  std::string message_header(SecurityKeyMessage::kHeaderSizeBytes, '\0');
  read_file_.ReadAtCurrentPos(base::as_writable_byte_span(message_header));

  std::string message_type(SecurityKeyMessage::kMessageTypeSizeBytes, '\0');
  read_file_.ReadAtCurrentPos(base::as_writable_byte_span(message_type));

  std::string message_data(payload_length_bytes, '\0');
  if (payload_length_bytes) {
    read_file_.ReadAtCurrentPos(base::as_writable_byte_span(message_data));
  }

  return message_header + message_type + message_data;
}

void SecurityKeyMessageWriterImplTest::SetUp() {
  ASSERT_TRUE(MakePipe(&read_file_, &write_file_));
  writer_ =
      std::make_unique<SecurityKeyMessageWriterImpl>(std::move(write_file_));
}

void SecurityKeyMessageWriterImplTest::WriteMessageToOutput(
    const std::string& payload) {
  base::test::TestFuture<std::string> future;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SecurityKeyMessageWriterImplTest::ReadMessage,
                     base::Unretained(this), payload.size()),
      future.GetCallback());

  if (payload.size()) {
    ASSERT_TRUE(writer_->WriteMessageWithPayload(kTestMessageType, payload));
  } else {
    ASSERT_TRUE(writer_->WriteMessage(kTestMessageType));
  }

  message_result_ = future.Take();

  size_t total_size = SecurityKeyMessage::kHeaderSizeBytes +
                      SecurityKeyMessage::kMessageTypeSizeBytes +
                      payload.size();
  ASSERT_EQ(message_result_.size(), total_size);

  SecurityKeyMessageType type =
      SecurityKeyMessage::MessageTypeFromValue(message_result_[4]);
  ASSERT_EQ(kTestMessageType, type);

  if (payload.size()) {
    ASSERT_EQ(message_result_.substr(5), payload);
  }

  // Destroy the writer and verify the other end of the pipe is clean.
  writer_.reset();
  char unused;
  ASSERT_LE(read_file_.ReadAtCurrentPos(base::byte_span_from_ref(unused)), 0);
}

TEST_F(SecurityKeyMessageWriterImplTest, WriteMessageWithoutPayload) {
  std::string empty_payload;
  WriteMessageToOutput(empty_payload);
}

TEST_F(SecurityKeyMessageWriterImplTest, WriteMessageWithPayload) {
  WriteMessageToOutput("Super-test-payload!");
}

TEST_F(SecurityKeyMessageWriterImplTest, WriteMessageWithLargePayload) {
  WriteMessageToOutput(std::string(kLargeMessageSizeBytes, 'Y'));
}

TEST_F(SecurityKeyMessageWriterImplTest, WriteMultipleMessages) {
  int total_messages_to_write = 10;
  for (int i = 0; i < total_messages_to_write; i++) {
    if (i % 2 == 0) {
      ASSERT_TRUE(writer_->WriteMessage(SecurityKeyMessageType::CONNECT));
    } else {
      ASSERT_TRUE(writer_->WriteMessage(SecurityKeyMessageType::REQUEST));
    }
  }

  for (int i = 0; i < total_messages_to_write; i++) {
    // Retrieve and verify the message header.
    int length;
    ASSERT_TRUE(
        read_file_.ReadAtCurrentPosAndCheck(base::byte_span_from_ref(length)));
    ASSERT_EQ(SecurityKeyMessage::kMessageTypeSizeBytes, length);

    // Retrieve and verify the message type.
    std::string message_type(length, '\0');
    ASSERT_TRUE(read_file_.ReadAtCurrentPosAndCheck(
        base::as_writable_byte_span(message_type)));

    SecurityKeyMessageType type =
        SecurityKeyMessage::MessageTypeFromValue(message_type[0]);
    if (i % 2 == 0) {
      ASSERT_EQ(SecurityKeyMessageType::CONNECT, type);
    } else {
      ASSERT_EQ(SecurityKeyMessageType::REQUEST, type);
    }
  }

  // Destroy the writer and verify the other end of the pipe is clean.
  writer_.reset();
  char unused;
  ASSERT_LE(read_file_.ReadAtCurrentPos(base::byte_span_from_ref(unused)), 0);
}

TEST_F(SecurityKeyMessageWriterImplTest, EnsureWriteFailsWhenPipeClosed) {
  // Close the read end so that writing fails immediately.
  read_file_.Close();

  EXPECT_FALSE(writer_->WriteMessage(kTestMessageType));
}

}  // namespace remoting
