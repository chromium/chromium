// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_auth_handler_mojo.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/security_key/security_key_ipc_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kConnectionId1 = 1;
const int kConnectionId2 = 2;
}  // namespace

namespace remoting {

class SecurityKeyAuthHandlerMojoTest : public testing::Test {
 public:
  SecurityKeyAuthHandlerMojoTest();

  SecurityKeyAuthHandlerMojoTest(const SecurityKeyAuthHandlerMojoTest&) =
      delete;
  SecurityKeyAuthHandlerMojoTest& operator=(
      const SecurityKeyAuthHandlerMojoTest&) = delete;

  ~SecurityKeyAuthHandlerMojoTest() override;

 protected:
  // Uses |remote| to connect to the auth handler via mojo pipe, validates
  // internal state of the object under test.
  void EstablishIpcConnection(mojo::Remote<mojom::SecurityKeyForwarder>& remote,
                              int expected_connection_id);

  // Sends a security key response message using |remote| and validates the
  // state of the object under test.
  void SendRequestToSecurityKeyAuthHandler(
      mojo::Remote<mojom::SecurityKeyForwarder>& remote,
      int connection_id,
      const std::string& request_payload);

  // Sends a security key response message to |remote| and validates the state
  // of the object under test.
  void SendResponseViaSecurityKeyAuthHandler(
      mojo::Remote<mojom::SecurityKeyForwarder>& remote,
      int connection_id,
      const std::string& response_payload);

  // Closes a security key session IPC connection and validates state.
  void CloseSecurityKeySessionIpcConnection(
      mojo::Remote<mojom::SecurityKeyForwarder>& remote,
      int connection_id);

  // IPC tests require a valid MessageLoop to run.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  // Used to wait for security key requests from the auth handler.
  base::test::TestFuture<int, const std::string&> request_future_{
      base::test::TestFutureMode::kQueue};

  // Used to wait for security key responses from the mojo remote.
  base::test::TestFuture<const std::string&> response_future_{
      base::test::TestFutureMode::kQueue};

  // Used to wait for disconnection signals.
  base::test::TestFuture<void> disconnect_future_{
      base::test::TestFutureMode::kQueue};

  // The object under test.
  std::unique_ptr<SecurityKeyAuthHandler> auth_handler_;

 private:
  testing::NiceMock<MockClientSessionDetails> mock_client_session_details_;
};

SecurityKeyAuthHandlerMojoTest::SecurityKeyAuthHandlerMojoTest() {
  auth_handler_ = std::make_unique<SecurityKeyAuthHandlerMojo>(
      &mock_client_session_details_);
  auth_handler_->SetSendMessageCallback(request_future_.GetRepeatingCallback(),
                                        this);
}

SecurityKeyAuthHandlerMojoTest::~SecurityKeyAuthHandlerMojoTest() = default;

void SecurityKeyAuthHandlerMojoTest::EstablishIpcConnection(
    mojo::Remote<mojom::SecurityKeyForwarder>& remote,
    int expected_connection_id) {
  size_t expected_connection_count =
      auth_handler_->GetActiveConnectionCountForTest() + 1;

  ASSERT_FALSE(auth_handler_->IsValidConnectionId(expected_connection_id));

  auth_handler_->BindSecurityKeyForwarder(remote.BindNewPipeAndPassReceiver());
  remote.set_disconnect_handler(disconnect_future_.GetRepeatingCallback());

  // Ensure the connection is established before continuing.
  base::test::TestFuture<uint32_t> version_future;
  remote.QueryVersion(version_future.GetCallback());
  ASSERT_TRUE(version_future.Wait());

  ASSERT_TRUE(remote.is_connected());

  // Verify the internal state of the SecurityKeyAuthHandler is correct.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(expected_connection_id));
  ASSERT_EQ(auth_handler_->GetActiveConnectionCountForTest(),
            expected_connection_count);
}

void SecurityKeyAuthHandlerMojoTest::SendRequestToSecurityKeyAuthHandler(
    mojo::Remote<mojom::SecurityKeyForwarder>& remote,
    int connection_id,
    const std::string& request_payload) {
  size_t expected_connection_count =
      auth_handler_->GetActiveConnectionCountForTest();

  // Send a security key request using the mojo remote.
  remote->OnSecurityKeyRequest(request_payload,
                               response_future_.GetRepeatingCallback());

  // Wait for the auth handler to receive the request.
  auto [received_id, received_payload] = request_future_.Take();
  ASSERT_EQ(received_id, connection_id);
  ASSERT_EQ(received_payload, request_payload);

  // Verify the internal state of the SecurityKeyAuthHandler is still correct.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(connection_id));
  ASSERT_EQ(auth_handler_->GetActiveConnectionCountForTest(),
            expected_connection_count);
}

void SecurityKeyAuthHandlerMojoTest::SendResponseViaSecurityKeyAuthHandler(
    mojo::Remote<mojom::SecurityKeyForwarder>& remote,
    int connection_id,
    const std::string& response_payload) {
  size_t expected_connection_count =
      auth_handler_->GetActiveConnectionCountForTest();

  // Send a security key response using the new IPC connection.
  auth_handler_->SendClientResponse(connection_id, response_payload);

  // Verify the security key response was received by the mojo remote.
  ASSERT_EQ(response_future_.Take(), response_payload);

  // Verify the internal state of the SecurityKeyAuthHandler is still correct.
  ASSERT_TRUE(auth_handler_->IsValidConnectionId(connection_id));
  ASSERT_EQ(auth_handler_->GetActiveConnectionCountForTest(),
            expected_connection_count);
}

void SecurityKeyAuthHandlerMojoTest::CloseSecurityKeySessionIpcConnection(
    mojo::Remote<mojom::SecurityKeyForwarder>& remote,
    int connection_id) {
  size_t expected_connection_count =
      auth_handler_->GetActiveConnectionCountForTest() - 1;

  remote.reset();

  // Make sure that all pending async work has been completed before checking
  // the validity of |expected_connection_id| from |auth_handler_|.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return auth_handler_->GetActiveConnectionCountForTest() ==
           expected_connection_count;
  }));

  // Verify the internal state has been updated.
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(connection_id));
  ASSERT_EQ(auth_handler_->GetActiveConnectionCountForTest(),
            expected_connection_count);
}

TEST_F(SecurityKeyAuthHandlerMojoTest, HandleSingleSecurityKeyRequest) {
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(kConnectionId1));

  // Create a mojo remote and connect to the auth handler.
  mojo::Remote<mojom::SecurityKeyForwarder> remote;
  EstablishIpcConnection(remote, kConnectionId1);

  // Send a security key request using the mojo remote.
  SendRequestToSecurityKeyAuthHandler(remote, kConnectionId1, "0123456789");

  // Send a security key response using the mojo remote.
  SendResponseViaSecurityKeyAuthHandler(remote, kConnectionId1, "9876543210");

  CloseSecurityKeySessionIpcConnection(remote, kConnectionId1);
}

TEST_F(SecurityKeyAuthHandlerMojoTest, HandleConcurrentSecurityKeyRequests) {
  // Create mojo remotes and connect each to the auth handler.
  mojo::Remote<mojom::SecurityKeyForwarder> remote1;
  mojo::Remote<mojom::SecurityKeyForwarder> remote2;

  EstablishIpcConnection(remote1, kConnectionId1);
  EstablishIpcConnection(remote2, kConnectionId2);

  // Connect and send a security key request using the first IPC connection.
  SendRequestToSecurityKeyAuthHandler(remote1, kConnectionId1, "aaaaaaaaaa");

  // Send a security key request using the second IPC connection.
  SendRequestToSecurityKeyAuthHandler(remote2, kConnectionId2, "bbbbbbbbbb");

  // Send a security key response using the first IPC connection.
  SendResponseViaSecurityKeyAuthHandler(remote1, kConnectionId1, "cccccccccc");

  // Send a security key response using the second IPC connection.
  SendResponseViaSecurityKeyAuthHandler(remote2, kConnectionId2, "dddddddddd");

  // Close the IPC connections.
  CloseSecurityKeySessionIpcConnection(remote1, kConnectionId1);
  CloseSecurityKeySessionIpcConnection(remote2, kConnectionId2);
}

TEST_F(SecurityKeyAuthHandlerMojoTest, HandleSequentialSecurityKeyRequests) {
  // Create a mojo remote to connect to the auth handler.
  mojo::Remote<mojom::SecurityKeyForwarder> remote1;

  EstablishIpcConnection(remote1, kConnectionId1);

  // Send a security key request using the first IPC connection.
  SendRequestToSecurityKeyAuthHandler(remote1, kConnectionId1, "aaaaaaaaaa");

  // Send a security key response using the first IPC connection.
  SendResponseViaSecurityKeyAuthHandler(remote1, kConnectionId1, "cccccccccc");

  // Close the IPC connection.
  CloseSecurityKeySessionIpcConnection(remote1, kConnectionId1);

  // Now connect with a second client.
  mojo::Remote<mojom::SecurityKeyForwarder> remote2;
  EstablishIpcConnection(remote2, kConnectionId2);

  // Send a security key request using the second IPC connection.
  SendRequestToSecurityKeyAuthHandler(remote2, kConnectionId2, "bbbbbbbbbb");

  // Send a security key response using the second IPC connection.
  SendResponseViaSecurityKeyAuthHandler(remote2, kConnectionId2, "dddddddddd");

  // Close the IPC connection.
  CloseSecurityKeySessionIpcConnection(remote2, kConnectionId2);
}

TEST_F(SecurityKeyAuthHandlerMojoTest, HandleSecurityKeyErrorResponse) {
  ASSERT_EQ(auth_handler_->GetActiveConnectionCountForTest(), 0u);

  // Create a mojo remote and connect to the auth handler.
  mojo::Remote<mojom::SecurityKeyForwarder> remote;
  EstablishIpcConnection(remote, kConnectionId1);

  // Send a security key request using the mojo remote.
  SendRequestToSecurityKeyAuthHandler(remote, kConnectionId1, "0123456789");

  // Simulate a security key error from the client.
  auth_handler_->SendErrorAndCloseConnection(kConnectionId1);

  // Verify the error response was received.
  ASSERT_EQ(response_future_.Take(), kSecurityKeyConnectionError);

  // Wait for the IPC disconnection.
  ASSERT_TRUE(disconnect_future_.Wait());
  disconnect_future_.Clear();

  // Verify the connection was cleaned up.
  ASSERT_FALSE(remote.is_connected());
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(kConnectionId1));
  ASSERT_EQ(auth_handler_->GetActiveConnectionCountForTest(), 0u);

  // Attempt to connect again after the error.
  mojo::Remote<mojom::SecurityKeyForwarder> remote2;
  EstablishIpcConnection(remote2, kConnectionId2);
}

TEST_F(SecurityKeyAuthHandlerMojoTest,
       OnSecurityKeyRequest_SafeWhenCallbackNull) {
  ASSERT_EQ(auth_handler_->GetActiveConnectionCountForTest(), 0u);

  // 1. Establish the IPC connection.
  mojo::Remote<mojom::SecurityKeyForwarder> remote;
  EstablishIpcConnection(remote, kConnectionId1);

  // 2. Clear the callback.
  auth_handler_->ClearSendMessageCallback(this);

  // 3. Send a request.
  remote->OnSecurityKeyRequest("0123456789",
                               response_future_.GetRepeatingCallback());

  // 4. Verify that the request was dropped (request_future_ is NOT triggered).
  // Since we cannot easily wait for "nothing to happen", we wait for the IPC
  // disconnection which is triggered by the handler closing the connection!
  ASSERT_TRUE(disconnect_future_.Wait());
  disconnect_future_.Clear();

  // 5. Verify the connection was cleaned up.
  ASSERT_FALSE(remote.is_connected());
  ASSERT_FALSE(auth_handler_->IsValidConnectionId(kConnectionId1));
  ASSERT_EQ(auth_handler_->GetActiveConnectionCountForTest(), 0u);
}

}  // namespace remoting
