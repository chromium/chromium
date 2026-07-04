// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_data_channel_handler.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "net/base/io_buffer.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/proto/security_key.pb.h"
#include "remoting/protocol/fake_message_pipe.h"
#include "remoting/protocol/fake_message_pipe_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using ::testing::_;

class SecurityKeyDataChannelHandlerTest : public ::testing::Test {
 public:
  SecurityKeyDataChannelHandlerTest();
  ~SecurityKeyDataChannelHandlerTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  // Helper to send a SecurityKeyMessage proto over the fake pipe.
  void SendMessage(int connection_id, const std::string& data);

  // Helper to receive a message from the fake pipe and parse it.
  bool GetSentMessage(int* connection_id,
                      std::string* data,
                      bool* error = nullptr);

  base::test::SingleThreadTaskEnvironment task_environment_;

  protocol::FakeMessagePipe fake_pipe_{/* asynchronous= */ false};
  MockSecurityKeyAuthHandler mock_auth_handler_;

  // The handler under test. It is self-deleting, so we track it via WeakPtr.
  base::WeakPtr<SecurityKeyDataChannelHandler> handler_;
};

SecurityKeyDataChannelHandlerTest::SecurityKeyDataChannelHandlerTest() =
    default;
SecurityKeyDataChannelHandlerTest::~SecurityKeyDataChannelHandlerTest() =
    default;

void SecurityKeyDataChannelHandlerTest::SetUp() {
  // The handler is self-deleting and manages its own lifetime (deleting itself
  // when the pipe notifies it of closure). We must not wrap it in a
  // std::unique_ptr here.
  auto* handler = new SecurityKeyDataChannelHandler(
      fake_pipe_.Wrap(), mock_auth_handler_.GetWeakPtr(), base::OnceClosure());
  handler_ = handler->GetWeakPtr();

  fake_pipe_.OpenPipe();
}

void SecurityKeyDataChannelHandlerTest::TearDown() {
  if (handler_) {
    fake_pipe_.ClosePipe();
  }
  // Ensure the handler is deleted.
  EXPECT_FALSE(handler_);
}

void SecurityKeyDataChannelHandlerTest::SendMessage(int connection_id,
                                                    const std::string& data) {
  protocol::SecurityKeyMessage message;
  message.set_connection_id(connection_id);
  message.set_data(data);

  fake_pipe_.ReceiveProtobufMessage(message);
}

bool SecurityKeyDataChannelHandlerTest::GetSentMessage(int* connection_id,
                                                       std::string* data,
                                                       bool* error) {
  if (fake_pipe_.sent_messages().empty()) {
    return false;
  }

  protocol::SecurityKeyMessage message;
  if (!message.ParseFromString(fake_pipe_.sent_messages().back())) {
    return false;
  }

  *connection_id = message.connection_id();
  *data = message.data();
  if (error) {
    *error = message.error();
  }
  return true;
}

TEST_F(SecurityKeyDataChannelHandlerTest, OnConnectedRegistersCallback) {
  // SetUp opens the pipe, which triggers OnConnected.
  // Verify that the callback was registered in the mock.
  EXPECT_FALSE(mock_auth_handler_.GetSendMessageCallback().is_null());
}

TEST_F(SecurityKeyDataChannelHandlerTest, ReceiveValidMessage) {
  int connection_id = 42;
  std::string data = "valid security key request data";

  EXPECT_CALL(mock_auth_handler_, SendClientResponse(connection_id, data))
      .Times(1);

  SendMessage(connection_id, data);
}

TEST_F(SecurityKeyDataChannelHandlerTest, ReceiveTooLargePayload) {
  int connection_id = 42;
  // 64KB + 1 byte = 65537 bytes.
  // This exceeds kMaxPayloadSize (65536) but the total message size is less
  // than kMaxMessageSize (66560), so it tests the payload limit check.
  std::string too_large_data(65537, 'A');

  EXPECT_CALL(mock_auth_handler_, SendClientResponse(_, _)).Times(0);

  // We expect the connection to be surgically closed on the host,
  // and an error response sent to the client.
  EXPECT_CALL(mock_auth_handler_, SendErrorAndCloseConnection(connection_id));

  // Sending too large payload should NOT cause the handler to close the pipe.
  SendMessage(connection_id, too_large_data);

  EXPECT_TRUE(handler_);

  // Verify that an error message was sent to the client.
  int sent_connection_id;
  std::string sent_data;
  bool sent_error = false;
  ASSERT_TRUE(GetSentMessage(&sent_connection_id, &sent_data, &sent_error));
  EXPECT_EQ(sent_connection_id, connection_id);
  EXPECT_TRUE(sent_error);
}

TEST_F(SecurityKeyDataChannelHandlerTest, ReceiveTooLargeFrame) {
  int connection_id = 42;
  // kMaxMessageSize is 66560. A 70KB payload will definitely exceed the
  // maximum allowed message size (frame limit) including protobuf overhead.
  std::string too_large_frame_data(70000, 'A');

  EXPECT_CALL(mock_auth_handler_, SendClientResponse(_, _)).Times(0);
  EXPECT_CALL(mock_auth_handler_, SendErrorAndCloseConnection(_)).Times(0);

  // Sending too large frame should cause the message to be discarded,
  // but the handler must remain open.
  SendMessage(connection_id, too_large_frame_data);

  EXPECT_TRUE(handler_);
}

TEST_F(SecurityKeyDataChannelHandlerTest, ReceiveInvalidProto) {
  EXPECT_CALL(mock_auth_handler_, SendClientResponse(_, _)).Times(0);
  EXPECT_CALL(mock_auth_handler_, SendErrorAndCloseConnection(_)).Times(0);

  // Sending invalid proto should cause the message to be discarded,
  // but the handler must remain open.
  std::vector<uint8_t> garbage(10, 0xFF);
  auto buffer = std::make_unique<CompoundBuffer>();
  buffer->AppendCopyOf(garbage);

  fake_pipe_.Receive(std::move(buffer));

  EXPECT_TRUE(handler_);
}

TEST_F(SecurityKeyDataChannelHandlerTest, SendMessage) {
  int connection_id = 42;
  std::string data = "security key response data";

  // Simulate host sending a message via the registered callback.
  mock_auth_handler_.GetSendMessageCallback().Run(connection_id, data);

  int sent_connection_id;
  std::string sent_data;
  ASSERT_TRUE(GetSentMessage(&sent_connection_id, &sent_data));
  EXPECT_EQ(sent_connection_id, connection_id);
  EXPECT_EQ(sent_data, data);
}

TEST_F(SecurityKeyDataChannelHandlerTest, SendTooLargeMessage) {
  int connection_id = 42;
  // 64KB + 1 byte = 65537 bytes.
  std::string too_large_data(65537, 'A');

  // We expect the connection to be surgically closed on the host,
  // and an error response sent to the client.
  EXPECT_CALL(mock_auth_handler_, SendErrorAndCloseConnection(connection_id));

  // Simulate host sending an oversized message via the registered callback.
  // This should NOT cause the handler to close the pipe.
  mock_auth_handler_.GetSendMessageCallback().Run(connection_id,
                                                  too_large_data);

  EXPECT_TRUE(handler_);

  // Verify that an error message was sent to the client.
  int sent_connection_id;
  std::string sent_data;
  bool sent_error = false;
  ASSERT_TRUE(GetSentMessage(&sent_connection_id, &sent_data, &sent_error));
  EXPECT_EQ(sent_connection_id, connection_id);
  EXPECT_TRUE(sent_error);
}

TEST_F(SecurityKeyDataChannelHandlerTest, PipeDisconnectClosesHostConnections) {
  // TODO(crbug.com/517007701): Verify CloseAllConnections is called when the
  // interface supports it in the next CL.
  fake_pipe_.ClosePipe();
  EXPECT_FALSE(handler_);
}

TEST_F(SecurityKeyDataChannelHandlerTest, TakeoverCallbackRunOnConnect) {
  // 1. Destroy the default handler created in SetUp.
  fake_pipe_.ClosePipe();
  ASSERT_FALSE(handler_);

  // 2. Create a new fake pipe and a mock callback.
  protocol::FakeMessagePipe local_fake_pipe{/* asynchronous= */ false};
  bool callback_run = false;
  base::OnceClosure takeover_callback =
      base::BindOnce([](bool* run) { *run = true; }, &callback_run);

  // 3. Create a new handler with the callback.
  auto* handler = new SecurityKeyDataChannelHandler(
      local_fake_pipe.Wrap(), mock_auth_handler_.GetWeakPtr(),
      std::move(takeover_callback));
  base::WeakPtr<SecurityKeyDataChannelHandler> weak_handler =
      handler->GetWeakPtr();

  EXPECT_FALSE(callback_run);

  // 4. Open the pipe. This triggers OnConnected() synchronously.
  local_fake_pipe.OpenPipe();

  // 5. Verify the callback was run.
  EXPECT_TRUE(callback_run);

  // Cleanup.
  local_fake_pipe.ClosePipe();
  EXPECT_FALSE(weak_handler);
}

}  // namespace remoting
