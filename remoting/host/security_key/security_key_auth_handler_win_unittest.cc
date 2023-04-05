// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_auth_handler.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/security_key/fake_security_key_ipc_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kConnectionId1 = 1;
const int kConnectionId2 = 2;
}  // namespace

namespace remoting {

class SecurityKeyAuthHandlerWinTest : public testing::Test {
 public:
  SecurityKeyAuthHandlerWinTest();

  SecurityKeyAuthHandlerWinTest(const SecurityKeyAuthHandlerWinTest&) = delete;
  SecurityKeyAuthHandlerWinTest& operator=(
      const SecurityKeyAuthHandlerWinTest&) = delete;

  ~SecurityKeyAuthHandlerWinTest() override;

  // Passed to the object used for testing to be called back to signal
  // completion of an IPC connection state change or reception of an IPC
  // message.
  void OperationComplete();

 protected:
  // Waits until the current |run_loop_| instance is signaled, then resets it.
  void WaitForOperationComplete(int expected_call_count = 1);

  // Used as a callback given to the object under test, expected to be called
  // back when a security key request is received by it.
  void SendMessageToClient(int connection_id, const std::string& data);

  // Uses |fake_ipc_client| to connect to the auth handler via mojo pipe, it
  // then validates internal state of the object under test.
  void EstablishIpcConnection(FakeSecurityKeyIpcClient& fake_ipc_client,
                              int expected_connection_id);

  // Sends a security key response message using |fake_ipc_client| and
  // validates the state of the object under test.
  void SendRequestToSecurityKeyAuthHandler(
      FakeSecurityKeyIpcClient& fake_ipc_client,
      int connection_id,
      const std::string& request_payload);

  // Sends a security key response message to |fake_ipc_client| and validates
  // the state of the object under test.
  void SendResponseViaSecurityKeyAuthHandler(
      FakeSecurityKeyIpcClient& fake_ipc_client,
      int connection_id,
      const std::string& response_payload);

  // Closes a security key session IPC connection and validates state.
  void CloseSecurityKeySessionIpcConnection(
      FakeSecurityKeyIpcClient& fake_ipc_client,
      int connection_id);

  // IPC tests require a valid MessageLoop to run.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  // Used to allow |message_loop_| to run during tests.  The instance is reset
  // after each stage of the tests has been completed.
  std::unique_ptr<base::RunLoop> run_loop_;

  // Number of times OperationComplete() has been called since last call to
  // WaitForOperationComplete().
  int operation_complete_call_count_ = 0;

  // The object under test.
  std::unique_ptr<SecurityKeyAuthHandler> auth_handler_;

  // Used to validate the object under test uses the correct ID when
  // communicating over the IPC connection.
  int last_connection_id_received_ = -1;

  // Stores the contents of the last IPC message received for validation.
  std::string last_message_received_;

 private:
  testing::NiceMock<MockClientSessionDetails> mock_client_session_details_;
};

SecurityKeyAuthHandlerWinTest::SecurityKeyAuthHandlerWinTest()
    : run_loop_(new base::RunLoop()) {
  auth_handler_ = remoting::SecurityKeyAuthHandler::Create(
      &mock_client_session_details_,
      base::BindRepeating(&SecurityKeyAuthHandlerWinTest::SendMessageToClient,
                          base::Unretained(this)),
      /*file_task_runner=*/nullptr);
}

SecurityKeyAuthHandlerWinTest::~SecurityKeyAuthHandlerWinTest() {}

void SecurityKeyAuthHandlerWinTest::OperationComplete() {
  run_loop_->Quit();
  operation_complete_call_count_++;
}

void SecurityKeyAuthHandlerWinTest::WaitForOperationComplete(
    int expected_call_count) {
  run_loop_->Run();
  int actual_call_count = operation_complete_call_count_;
  operation_complete_call_count_ = 0;
  run_loop_ = std::make_unique<base::RunLoop>();
  if (actual_call_count < expected_call_count) {
    WaitForOperationComplete(expected_call_count - actual_call_count);
  }
}

void SecurityKeyAuthHandlerWinTest::SendMessageToClient(
    int connection_id,
    const std::string& data) {
  last_connection_id_received_ = connection_id;
  last_message_received_ = data;
  OperationComplete();
}

void SecurityKeyAuthHandlerWinTest::EstablishIpcConnection(
    FakeSecurityKeyIpcClient& fake_ipc_client,
    int expected_connection_id) {
  size_t expected_connection_count =
      auth_handler_->GetActiveConnectionCountForTest() + 1;

  ASSERT_FALSE(auth_handler_->IsValidConnectionId(expected_connection_id));

  auth_handler_->BindSecurityKeyForwarder(
      fake_ipc_client.BindNewPipeAndPassReceiver());
  WaitForOperationComplete();

  ASSERT_TRUE(fake_ipc_client.ipc_connected());

  // Verify the internal state of the SecurityKeyAuthHandler is correct.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(expected_connection_id));
  ASSERT_EQ(expected_connection_count,
            auth_handler_->GetActiveConnectionCountForTest());
}

void SecurityKeyAuthHandlerWinTest::SendRequestToSecurityKeyAuthHandler(
    FakeSecurityKeyIpcClient& fake_ipc_client,
    int connection_id,
    const std::string& request_payload) {
  size_t expected_connection_count =
      auth_handler_->GetActiveConnectionCountForTest();

  // Send a security key request using the fake IPC client.
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_payload);
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(connection_id, last_connection_id_received_);
  ASSERT_EQ(request_payload, last_message_received_);

  // Verify the internal state of the SecurityKeyAuthHandler is still correct.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(connection_id));
  ASSERT_EQ(expected_connection_count,
            auth_handler_->GetActiveConnectionCountForTest());
}

void SecurityKeyAuthHandlerWinTest::SendResponseViaSecurityKeyAuthHandler(
    FakeSecurityKeyIpcClient& fake_ipc_client,
    int connection_id,
    const std::string& response_payload) {
  size_t expected_connection_count =
      auth_handler_->GetActiveConnectionCountForTest();

  // Send a security key response using the new IPC connection.
  auth_handler_->SendClientResponse(connection_id, response_payload);
  WaitForOperationComplete();

  // Verify the security key response was received.
  ASSERT_EQ(response_payload, fake_ipc_client.last_message_received());

  // Verify the internal state of the SecurityKeyAuthHandler is still correct.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(connection_id));
  ASSERT_EQ(expected_connection_count,
            auth_handler_->GetActiveConnectionCountForTest());
}

void SecurityKeyAuthHandlerWinTest::CloseSecurityKeySessionIpcConnection(
    FakeSecurityKeyIpcClient& fake_ipc_client,
    int connection_id) {
  size_t expected_connection_count =
      auth_handler_->GetActiveConnectionCountForTest() - 1;

  fake_ipc_client.CloseIpcConnection();
  WaitForOperationComplete();

  // Make sure that all pending async work has been completed before checking
  // the validity of |expected_connection_id| from |auth_handler_|.
  task_environment_.RunUntilIdle();

  // Verify the internal state has been updated.
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(connection_id));
  ASSERT_EQ(expected_connection_count,
            auth_handler_->GetActiveConnectionCountForTest());
}

TEST_F(SecurityKeyAuthHandlerWinTest, HandleSingleSecurityKeyRequest) {
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(kConnectionId1));

  // Create a fake client and connect to the auth handler via mojo pipe.
  FakeSecurityKeyIpcClient fake_ipc_client(
      base::BindRepeating(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                          base::Unretained(this)));
  EstablishIpcConnection(fake_ipc_client, kConnectionId1);

  // Send a security key request using the fake IPC client.
  SendRequestToSecurityKeyAuthHandler(fake_ipc_client, kConnectionId1,
                                      "0123456789");

  // Send a security key response using the fake IPC client.
  SendResponseViaSecurityKeyAuthHandler(fake_ipc_client, kConnectionId1,
                                        "9876543210");

  CloseSecurityKeySessionIpcConnection(fake_ipc_client, kConnectionId1);
}

TEST_F(SecurityKeyAuthHandlerWinTest, HandleConcurrentSecurityKeyRequests) {
  // Create fake clients and connect each to the auth handler.
  FakeSecurityKeyIpcClient fake_ipc_client_1(
      base::BindRepeating(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                          base::Unretained(this)));
  FakeSecurityKeyIpcClient fake_ipc_client_2(
      base::BindRepeating(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                          base::Unretained(this)));

  EstablishIpcConnection(fake_ipc_client_1, kConnectionId1);
  EstablishIpcConnection(fake_ipc_client_2, kConnectionId2);

  // Connect and send a security key request using the first IPC connection.
  SendRequestToSecurityKeyAuthHandler(fake_ipc_client_1, kConnectionId1,
                                      "aaaaaaaaaa");

  // Send a security key request using the second IPC connection.
  SendRequestToSecurityKeyAuthHandler(fake_ipc_client_2, kConnectionId2,
                                      "bbbbbbbbbb");

  // Send a security key response using the first IPC connection.
  SendResponseViaSecurityKeyAuthHandler(fake_ipc_client_1, kConnectionId1,
                                        "cccccccccc");

  // Send a security key response using the second IPC connection.
  SendResponseViaSecurityKeyAuthHandler(fake_ipc_client_2, kConnectionId2,
                                        "dddddddddd");

  // Close the IPC connections.
  CloseSecurityKeySessionIpcConnection(fake_ipc_client_1, kConnectionId1);
  CloseSecurityKeySessionIpcConnection(fake_ipc_client_2, kConnectionId2);
}

TEST_F(SecurityKeyAuthHandlerWinTest, HandleSequentialSecurityKeyRequests) {
  // Create fake clients to connect to the auth handler.
  FakeSecurityKeyIpcClient fake_ipc_client_1(
      base::BindRepeating(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                          base::Unretained(this)));

  EstablishIpcConnection(fake_ipc_client_1, kConnectionId1);

  // Send a security key request using the first IPC connection.
  SendRequestToSecurityKeyAuthHandler(fake_ipc_client_1, kConnectionId1,
                                      "aaaaaaaaaa");

  // Send a security key response using the first IPC connection.
  SendResponseViaSecurityKeyAuthHandler(fake_ipc_client_1, kConnectionId1,
                                        "cccccccccc");

  // Close the IPC connection.
  CloseSecurityKeySessionIpcConnection(fake_ipc_client_1, kConnectionId1);

  // Now connect with a second client.
  FakeSecurityKeyIpcClient fake_ipc_client_2(
      base::BindRepeating(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                          base::Unretained(this)));
  EstablishIpcConnection(fake_ipc_client_2, kConnectionId2);

  // Send a security key request using the second IPC connection.
  SendRequestToSecurityKeyAuthHandler(fake_ipc_client_2, kConnectionId2,
                                      "bbbbbbbbbb");

  // Send a security key response using the second IPC connection.
  SendResponseViaSecurityKeyAuthHandler(fake_ipc_client_2, kConnectionId2,
                                        "dddddddddd");

  // Close the IPC connection.
  CloseSecurityKeySessionIpcConnection(fake_ipc_client_2, kConnectionId2);
}

TEST_F(SecurityKeyAuthHandlerWinTest, HandleSecurityKeyErrorResponse) {
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());

  // Create a fake client and connect to the auth handler.
  FakeSecurityKeyIpcClient fake_ipc_client(
      base::BindRepeating(&SecurityKeyAuthHandlerWinTest::OperationComplete,
                          base::Unretained(this)));
  EstablishIpcConnection(fake_ipc_client, kConnectionId1);

  // Send a security key request using the fake IPC client.
  SendRequestToSecurityKeyAuthHandler(fake_ipc_client, kConnectionId1,
                                      "0123456789");

  // Simulate a security key error from the client.
  auth_handler_->SendErrorAndCloseConnection(kConnectionId1);
  // Wait for the ipc server connection to be torn down.
  // Once for security key response, and once for IPC disconnection.
  WaitForOperationComplete(2);

  // Verify the connection was cleaned up.
  ASSERT_FALSE(fake_ipc_client.ipc_connected());
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(kConnectionId1));
  ASSERT_EQ(0u, auth_handler_->GetActiveConnectionCountForTest());

  // Attempt to connect again after the error.
  EstablishIpcConnection(fake_ipc_client, kConnectionId2);
}

}  // namespace remoting
