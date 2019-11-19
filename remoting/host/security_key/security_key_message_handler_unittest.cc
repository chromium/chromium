// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_message_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "remoting/host/security_key/fake_security_key_ipc_client.h"
#include "remoting/host/security_key/fake_security_key_message_reader.h"
#include "remoting/host/security_key/fake_security_key_message_writer.h"
#include "remoting/host/security_key/security_key_ipc_constants.h"
#include "remoting/host/security_key/security_key_message.h"
#include "remoting/host/setup/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class SecurityKeyMessageHandlerTest : public testing::Test {
 public:
  SecurityKeyMessageHandlerTest();
  ~SecurityKeyMessageHandlerTest() override;

  // Passed to the object used for testing to be called back to signal
  // completion of an action.
  void OperationComplete();

 protected:
  // testing::Test interface.
  void SetUp() override;

  // Waits until the current |run_loop_| instance is signaled, then resets it.
  void WaitForOperationComplete();

  // Passed to |message_channel_| and called back when a message is received.
  void OnSecurityKeyMessage(SecurityKeyMessageType message_type,
                            const std::string& message_payload);

  bool last_operation_failed_ = false;
  SecurityKeyMessageType last_message_type_received_ =
      SecurityKeyMessageType::INVALID;
  std::string last_message_payload_received_;

  base::WeakPtr<FakeSecurityKeyIpcClient> ipc_client_weak_ptr_;
  base::WeakPtr<FakeSecurityKeyMessageReader> reader_weak_ptr_;
  base::WeakPtr<FakeSecurityKeyMessageWriter> writer_weak_ptr_;
  std::unique_ptr<SecurityKeyMessageHandler> message_handler_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(SecurityKeyMessageHandlerTest);
};

SecurityKeyMessageHandlerTest::SecurityKeyMessageHandlerTest() = default;

SecurityKeyMessageHandlerTest::~SecurityKeyMessageHandlerTest() = default;

void SecurityKeyMessageHandlerTest::OperationComplete() {
  run_loop_->Quit();
}

void SecurityKeyMessageHandlerTest::SetUp() {
  run_loop_.reset(new base::RunLoop());
  message_handler_.reset(new SecurityKeyMessageHandler());

  std::unique_ptr<FakeSecurityKeyIpcClient> ipc_client(
      new FakeSecurityKeyIpcClient(
          base::Bind(&SecurityKeyMessageHandlerTest::OperationComplete,
                     base::Unretained(this))));
  ipc_client_weak_ptr_ = ipc_client->AsWeakPtr();

  std::unique_ptr<FakeSecurityKeyMessageReader> reader(
      new FakeSecurityKeyMessageReader());
  reader_weak_ptr_ = reader->AsWeakPtr();

  std::unique_ptr<FakeSecurityKeyMessageWriter> writer(
      new FakeSecurityKeyMessageWriter(
          base::Bind(&SecurityKeyMessageHandlerTest::OperationComplete,
                     base::Unretained(this))));
  writer_weak_ptr_ = writer->AsWeakPtr();

  message_handler_->SetSecurityKeyMessageReaderForTest(std::move(reader));

  message_handler_->SetSecurityKeyMessageWriterForTest(std::move(writer));

  base::File read_file;
  base::File write_file;
  ASSERT_TRUE(MakePipe(&read_file, &write_file));
  message_handler_->Start(
      std::move(read_file), std::move(write_file), std::move(ipc_client),
      base::Bind(&SecurityKeyMessageHandlerTest::OperationComplete,
                 base::Unretained(this)));
}

void SecurityKeyMessageHandlerTest::WaitForOperationComplete() {
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void SecurityKeyMessageHandlerTest::OnSecurityKeyMessage(
    SecurityKeyMessageType message_type,
    const std::string& message_payload) {
  last_message_type_received_ = message_type;
  last_message_payload_received_ = message_payload;

  OperationComplete();
}

TEST_F(SecurityKeyMessageHandlerTest,
       ProcessConnectMessage_SessionExists_ConnectionAttemptSuccess) {
  ipc_client_weak_ptr_->set_check_for_ipc_channel_return_value(true);
  ipc_client_weak_ptr_->set_establish_ipc_connection_should_succeed(true);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(SecurityKeyMessageType::CONNECT,
                                               std::string()));
  WaitForOperationComplete();

  ASSERT_EQ(SecurityKeyMessageType::CONNECT_RESPONSE,
            writer_weak_ptr_->last_message_type());
  ASSERT_EQ(std::string(1, kConnectResponseActiveSession),
            writer_weak_ptr_->last_message_payload());
}

TEST_F(SecurityKeyMessageHandlerTest,
       ProcessConnectMessage_SessionExists_WriteFails) {
  ipc_client_weak_ptr_->set_check_for_ipc_channel_return_value(true);
  ipc_client_weak_ptr_->set_establish_ipc_connection_should_succeed(true);
  writer_weak_ptr_->set_write_request_succeeded(/*should_succeed=*/false);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(SecurityKeyMessageType::CONNECT,
                                               std::string()));
  WaitForOperationComplete();

  ASSERT_FALSE(ipc_client_weak_ptr_.get());
  ASSERT_FALSE(reader_weak_ptr_.get());
  ASSERT_FALSE(writer_weak_ptr_.get());
}

TEST_F(SecurityKeyMessageHandlerTest,
       ProcessConnectMessage_SessionExists_ConnectionAttemptFailure) {
  ipc_client_weak_ptr_->set_check_for_ipc_channel_return_value(true);
  ipc_client_weak_ptr_->set_establish_ipc_connection_should_succeed(false);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(SecurityKeyMessageType::CONNECT,
                                               std::string()));
  WaitForOperationComplete();

  ASSERT_EQ(SecurityKeyMessageType::CONNECT_ERROR,
            writer_weak_ptr_->last_message_type());
  ASSERT_FALSE(writer_weak_ptr_->last_message_payload().empty());
}

TEST_F(SecurityKeyMessageHandlerTest, ProcessConnectMessage_NoSessionExists) {
  ipc_client_weak_ptr_->set_check_for_ipc_channel_return_value(false);
  ipc_client_weak_ptr_->set_establish_ipc_connection_should_succeed(false);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(SecurityKeyMessageType::CONNECT,
                                               std::string()));
  WaitForOperationComplete();

  ASSERT_EQ(SecurityKeyMessageType::CONNECT_RESPONSE,
            writer_weak_ptr_->last_message_type());
  ASSERT_EQ(std::string(1, kConnectResponseNoSession),
            writer_weak_ptr_->last_message_payload());
}

TEST_F(SecurityKeyMessageHandlerTest, ProcessConnectMessage_IncorrectPayload) {
  ipc_client_weak_ptr_->set_check_for_ipc_channel_return_value(true);
  ipc_client_weak_ptr_->set_establish_ipc_connection_should_succeed(false);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(SecurityKeyMessageType::CONNECT,
                                               "Invalid request payload"));
  WaitForOperationComplete();

  ASSERT_EQ(SecurityKeyMessageType::CONNECT_ERROR,
            writer_weak_ptr_->last_message_type());
  ASSERT_FALSE(writer_weak_ptr_->last_message_payload().empty());
}

TEST_F(SecurityKeyMessageHandlerTest,
       ProcessRequestMessage_ValidPayload_IpcSendSuccess) {
  std::string request_payload("I AM A VALID REQUEST PAYLOAD!");
  std::string response_payload("I AM A VALID RESPONSE PAYLOAD!");
  ipc_client_weak_ptr_->set_send_security_request_should_succeed(true);
  ipc_client_weak_ptr_->set_security_key_response_payload(response_payload);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(SecurityKeyMessageType::REQUEST,
                                               request_payload));
  WaitForOperationComplete();

  ASSERT_EQ(SecurityKeyMessageType::REQUEST_RESPONSE,
            writer_weak_ptr_->last_message_type());
  ASSERT_EQ(response_payload, writer_weak_ptr_->last_message_payload());
}

TEST_F(SecurityKeyMessageHandlerTest, ProcessRequestMessage_WriteFails) {
  std::string request_payload("I AM A VALID REQUEST PAYLOAD!");
  std::string response_payload("I AM A VALID RESPONSE PAYLOAD!");

  ipc_client_weak_ptr_->set_send_security_request_should_succeed(true);
  ipc_client_weak_ptr_->set_security_key_response_payload(response_payload);
  writer_weak_ptr_->set_write_request_succeeded(/*should_succeed=*/false);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(SecurityKeyMessageType::REQUEST,
                                               request_payload));
  WaitForOperationComplete();

  ASSERT_FALSE(ipc_client_weak_ptr_.get());
  ASSERT_FALSE(reader_weak_ptr_.get());
  ASSERT_FALSE(writer_weak_ptr_.get());
}

TEST_F(SecurityKeyMessageHandlerTest,
       ProcessRequestMessage_ValidPayload_IpcSendFailure) {
  std::string request_payload("I AM A VALID REQUEST PAYLOAD!");
  ipc_client_weak_ptr_->set_send_security_request_should_succeed(false);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(SecurityKeyMessageType::REQUEST,
                                               request_payload));
  WaitForOperationComplete();

  ASSERT_EQ(SecurityKeyMessageType::REQUEST_ERROR,
            writer_weak_ptr_->last_message_type());
  ASSERT_FALSE(writer_weak_ptr_->last_message_payload().empty());
}

TEST_F(SecurityKeyMessageHandlerTest,
       ProcessRequestMessage_ValidPayload_EmptyClientResponse) {
  std::string request_payload("I AM A VALID REQUEST PAYLOAD!");
  std::string response_payload("");
  ipc_client_weak_ptr_->set_send_security_request_should_succeed(true);
  ipc_client_weak_ptr_->set_security_key_response_payload(response_payload);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(SecurityKeyMessageType::REQUEST,
                                               request_payload));
  WaitForOperationComplete();

  ASSERT_EQ(SecurityKeyMessageType::REQUEST_ERROR,
            writer_weak_ptr_->last_message_type());
  ASSERT_FALSE(writer_weak_ptr_->last_message_payload().empty());
}

TEST_F(SecurityKeyMessageHandlerTest,
       ProcessRequestMessage_ValidPayload_ClientResponseError) {
  std::string request_payload("I AM A VALID REQUEST PAYLOAD!");
  std::string response_payload(kSecurityKeyConnectionError);
  ipc_client_weak_ptr_->set_send_security_request_should_succeed(true);
  ipc_client_weak_ptr_->set_security_key_response_payload(response_payload);

  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(SecurityKeyMessageType::REQUEST,
                                               request_payload));
  WaitForOperationComplete();

  ASSERT_EQ(SecurityKeyMessageType::REQUEST_ERROR,
            writer_weak_ptr_->last_message_type());
  ASSERT_FALSE(writer_weak_ptr_->last_message_payload().empty());
}

TEST_F(SecurityKeyMessageHandlerTest, ProcessRequestMessage_InvalidPayload) {
  std::string invalid_payload("");
  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(SecurityKeyMessageType::REQUEST,
                                               invalid_payload));
  WaitForOperationComplete();

  ASSERT_EQ(SecurityKeyMessageType::REQUEST_ERROR,
            writer_weak_ptr_->last_message_type());
  ASSERT_FALSE(writer_weak_ptr_->last_message_payload().empty());
}

TEST_F(SecurityKeyMessageHandlerTest, ProcessUnknownMessage) {
  reader_weak_ptr_->message_callback().Run(
      SecurityKeyMessage::CreateMessageForTest(
          SecurityKeyMessageType::UNKNOWN_ERROR, std::string()));
  WaitForOperationComplete();

  ASSERT_EQ(SecurityKeyMessageType::UNKNOWN_COMMAND,
            writer_weak_ptr_->last_message_type());
}

}  // namespace remoting
