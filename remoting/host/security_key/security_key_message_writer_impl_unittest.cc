// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/host/security_key/security_key_message_writer_impl.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
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

  // Called back once the read operation has completed.
  void OnReadComplete(base::OnceClosure done_callback,
                      const std::string& result);

 protected:
  // testing::Test interface.
  void SetUp() override;

  // Writes |kTestMessageType| and |payload| to the output stream and verifies
  // they were written correctly.
  void WriteMessageToOutput(const std::string& payload);

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
  read_file_.ReadAtCurrentPos(std::data(message_header),
                              SecurityKeyMessage::kHeaderSizeBytes);

  std::string message_type(SecurityKeyMessage::kMessageTypeSizeBytes, '\0');
  read_file_.ReadAtCurrentPos(std::data(message_type),
                              SecurityKeyMessage::kMessageTypeSizeBytes);

  std::string message_data(payload_length_bytes, '\0');
  if (payload_length_bytes) {
    read_file_.ReadAtCurrentPos(std::data(message_data), payload_length_bytes);
  }

  return message_header + message_type + message_data;
}

void SecurityKeyMessageWriterImplTest::OnReadComplete(
    base::OnceClosure done_callback,
    const std::string& result) {
  message_result_ = result;
  std::move(done_callback).Run();
}

void SecurityKeyMessageWriterImplTest::SetUp() {
  ASSERT_TRUE(MakePipe(&read_file_, &write_file_));
  writer_ =
      std::make_unique<SecurityKeyMessageWriterImpl>(std::move(write_file_));
}

void SecurityKeyMessageWriterImplTest::WriteMessageToOutput(
    const std::string& payload) {
  // Thread used for blocking IO operations.
  base::Thread reader_thread("ReaderThread");

  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  reader_thread.StartWithOptions(std::move(options));

  // Used to block until the read complete callback is triggered.
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);
  base::RunLoop run_loop;

  ASSERT_TRUE(reader_thread.task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SecurityKeyMessageWriterImplTest::ReadMessage,
                     base::Unretained(this), payload.size()),
      base::BindOnce(&SecurityKeyMessageWriterImplTest::OnReadComplete,
                     base::Unretained(this), run_loop.QuitClosure())));

  if (payload.size()) {
    ASSERT_TRUE(writer_->WriteMessageWithPayload(kTestMessageType, payload));
  } else {
    ASSERT_TRUE(writer_->WriteMessage(kTestMessageType));
  }

  run_loop.Run();

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
  ASSERT_LE(read_file_.ReadAtCurrentPos(&unused, 1), 0);
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
    ASSERT_EQ(SecurityKeyMessage::kHeaderSizeBytes,
              read_file_.ReadAtCurrentPos(reinterpret_cast<char*>(&length), 4));
    ASSERT_EQ(SecurityKeyMessage::kMessageTypeSizeBytes, length);

    // Retrieve and verify the message type.
    std::string message_type(length, '\0');
    int bytes_read =
        read_file_.ReadAtCurrentPos(std::data(message_type), length);
    ASSERT_EQ(length, bytes_read);

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
  ASSERT_LE(read_file_.ReadAtCurrentPos(&unused, 1), 0);
}

TEST_F(SecurityKeyMessageWriterImplTest, EnsureWriteFailsWhenPipeClosed) {
  // Close the read end so that writing fails immediately.
  read_file_.Close();

  EXPECT_FALSE(writer_->WriteMessage(kTestMessageType));
}

}  // namespace remoting
