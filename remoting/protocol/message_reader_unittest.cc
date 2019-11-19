// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "remoting/protocol/fake_stream_socket.h"
#include "remoting/protocol/message_reader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/rtc_base/byte_order.h"

using testing::_;
using testing::DoAll;
using testing::Mock;
using testing::SaveArg;

namespace remoting {
namespace protocol {

namespace {
const char kTestMessage1[] = "Message1";
const char kTestMessage2[] = "Message2";
}  // namespace

class MockMessageReceivedCallback {
 public:
  MOCK_METHOD0(OnMessage, void());
};

class MessageReaderTest : public testing::Test {
 public:
  // Following two methods are used by the ReadFromCallback test.
  void AddSecondMessage() { AddMessage(kTestMessage2); }

  // Used by the DeleteFromCallback() test.
  void DeleteReader() { reader_.reset(); }

 protected:
  void SetUp() override {
    reader_.reset(new MessageReader());
  }

  void InitReader() {
    reader_->StartReading(
        &socket_,
        base::Bind(&MessageReaderTest::OnMessage, base::Unretained(this)),
        base::Bind(&MessageReaderTest::OnReadError, base::Unretained(this)));
  }

  void AddMessage(const std::string& message) {
    std::string data = std::string(4, ' ') + message;
    rtc::SetBE32(const_cast<char*>(data.data()), message.size());

    socket_.AppendInputData(data);
  }

  bool CompareResult(CompoundBuffer* buffer, const std::string& expected) {
    std::string result(buffer->total_bytes(), ' ');
    buffer->CopyTo(const_cast<char*>(result.data()), result.size());
    return result == expected;
  }

  void OnReadError(int error) {
    read_error_ = error;
    reader_.reset();
  }

  void OnMessage(std::unique_ptr<CompoundBuffer> buffer) {
    messages_.push_back(std::move(buffer));
    callback_.OnMessage();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<MessageReader> reader_;
  FakeStreamSocket socket_;
  MockMessageReceivedCallback callback_;
  int read_error_ = 0;
  std::vector<std::unique_ptr<CompoundBuffer>> messages_;
};

// Receive one message.
TEST_F(MessageReaderTest, OneMessage) {
  AddMessage(kTestMessage1);

  EXPECT_CALL(callback_, OnMessage()).Times(1);

  InitReader();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(socket_.read_pending());
  EXPECT_EQ(1U, messages_.size());
}

// Receive two messages in one packet.
TEST_F(MessageReaderTest, TwoMessages_Together) {
  AddMessage(kTestMessage1);
  AddMessage(kTestMessage2);

  EXPECT_CALL(callback_, OnMessage()).Times(2);

  InitReader();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(&callback_);
  Mock::VerifyAndClearExpectations(&socket_);

  EXPECT_TRUE(CompareResult(messages_[0].get(), kTestMessage1));
  EXPECT_TRUE(CompareResult(messages_[1].get(), kTestMessage2));

  EXPECT_TRUE(socket_.read_pending());
}

// Receive two messages in separate packets.
TEST_F(MessageReaderTest, TwoMessages_Separately) {
  AddMessage(kTestMessage1);

  EXPECT_CALL(callback_, OnMessage())
      .Times(1);

  InitReader();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(&callback_);
  Mock::VerifyAndClearExpectations(&socket_);

  EXPECT_TRUE(CompareResult(messages_[0].get(), kTestMessage1));

  EXPECT_TRUE(socket_.read_pending());

  // Write another message and verify that we receive it.
  EXPECT_CALL(callback_, OnMessage())
      .Times(1);
  AddMessage(kTestMessage2);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(CompareResult(messages_[1].get(), kTestMessage2));

  EXPECT_TRUE(socket_.read_pending());
}

// Read() returns error.
TEST_F(MessageReaderTest, ReadError) {
  socket_.SetReadError(net::ERR_FAILED);

  EXPECT_CALL(callback_, OnMessage()).Times(0);

  InitReader();

  EXPECT_EQ(net::ERR_FAILED, read_error_);
  EXPECT_FALSE(reader_);
}

// Read() returns 0 (end of stream).
TEST_F(MessageReaderTest, EndOfStream) {
  socket_.SetReadError(0);

  EXPECT_CALL(callback_, OnMessage()).Times(0);

  InitReader();

  EXPECT_EQ(0, read_error_);
  EXPECT_FALSE(reader_);
}

// Verify that we the OnMessage callback is not reentered.
TEST_F(MessageReaderTest, ReadFromCallback) {
  AddMessage(kTestMessage1);

  EXPECT_CALL(callback_, OnMessage())
      .Times(2)
      .WillOnce(Invoke(this, &MessageReaderTest::AddSecondMessage));

  InitReader();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(socket_.read_pending());
}

// Verify that we stop getting callbacks after deleting MessageReader.
TEST_F(MessageReaderTest, DeleteFromCallback) {
  AddMessage(kTestMessage1);
  AddMessage(kTestMessage2);

  // OnMessage() should never be called for the second message.
  EXPECT_CALL(callback_, OnMessage())
      .Times(1)
      .WillOnce(Invoke(this, &MessageReaderTest::DeleteReader));

  InitReader();
  base::RunLoop().RunUntilIdle();
}

}  // namespace protocol
}  // namespace remoting
