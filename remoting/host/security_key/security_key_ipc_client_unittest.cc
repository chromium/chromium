// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_ipc_client.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/host/mojom/remote_security_key.mojom.h"
#include "remoting/host/security_key/security_key_ipc_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kLargeMessageSizeBytes = 256 * 1024;

using testing::_;
using testing::Return;
}  // namespace

namespace remoting {

class SecurityKeyIpcClientTest : public testing::Test,
                                 public mojom::SecurityKeyForwarder {
 public:
  SecurityKeyIpcClientTest();

  SecurityKeyIpcClientTest(const SecurityKeyIpcClientTest&) = delete;
  SecurityKeyIpcClientTest& operator=(const SecurityKeyIpcClientTest&) = delete;

  ~SecurityKeyIpcClientTest() override;

  // mojom::SecurityKeyForwarder interface.
  void OnSecurityKeyRequest(const std::string& request_data,
                            OnSecurityKeyRequestCallback callback) override;

  // Passed to the object used for testing to be called back to signal
  // completion of an IPC channel state change or reception of an IPC message.
  void OperationComplete(bool failed);

  // Callback used to signal when the IPC channel is ready for messages.
  void ConnectionStateHandler();

  // Used as a callback given to the object under test, expected to be called
  // back when a security key response is sent.
  void ClientMessageReceived(const std::string& response_payload);

 protected:
  // Waits until the current |run_loop_| instance is signaled, then resets it.
  void WaitForOperationComplete();

  // Sets up an active IPC connection between |security_key_ipc_client_|
  // and |fake_ipc_server_|. |expect_error| defines whether the the error
  // callback should be invoked during the connection process.
  void EstablishConnection(bool expect_error = false);

  // Sends a security key request from |security_key_ipc_client_| and
  // a response from |fake_ipc_server_| and verifies the payloads for both.
  void SendRequestAndResponse(const std::string& request_data,
                              const std::string& response_data);

  // IPC tests require a valid MessageLoop to run.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  // Used to allow |message_loop_| to run during tests.  The instance is reset
  // after each stage of the tests has been completed.
  std::unique_ptr<base::RunLoop> run_loop_;

  raw_ptr<MockChromotingHostServicesProvider, DanglingUntriaged> api_provider_;

  // The object under test.
  std::unique_ptr<SecurityKeyIpcClient> security_key_ipc_client_;

  MockChromotingSessionServices mock_api_;

  mojo::Receiver<mojom::SecurityKeyForwarder> receiver_{this};

  // Stores the current session ID on supported platforms.
  uint32_t session_id_ = 0;

  // Tracks the success/failure of the last async operation.
  bool operation_failed_ = false;

  // Tracks whether the IPC channel connection has been established.
  bool connection_established_ = false;

  // Used to drive invalid session behavior for testing the client.
  bool simulate_invalid_session_ = false;

  // Stores the contents of the last IPC message received for validation.
  std::string last_message_received_;

  OnSecurityKeyRequestCallback response_callback_;
};

SecurityKeyIpcClientTest::SecurityKeyIpcClientTest()
    : run_loop_(new base::RunLoop()) {
  auto api_provider = std::make_unique<MockChromotingHostServicesProvider>();
  api_provider_ = api_provider.get();
  security_key_ipc_client_ =
      base::WrapUnique(new SecurityKeyIpcClient(std::move(api_provider)));
}

SecurityKeyIpcClientTest::~SecurityKeyIpcClientTest() = default;

void SecurityKeyIpcClientTest::OnSecurityKeyRequest(
    const std::string& request_data,
    OnSecurityKeyRequestCallback callback) {
  last_message_received_ = request_data;
  response_callback_ = std::move(callback);
  OperationComplete(/*failed=*/false);
}

void SecurityKeyIpcClientTest::OperationComplete(bool failed) {
  operation_failed_ |= failed;
  run_loop_->Quit();
}

void SecurityKeyIpcClientTest::ConnectionStateHandler() {
  connection_established_ = true;
  OperationComplete(/*failed=*/false);
}

void SecurityKeyIpcClientTest::WaitForOperationComplete() {
  run_loop_->Run();
  run_loop_ = std::make_unique<base::RunLoop>();

  // Run until there are no pending work items in the queue.
  base::RunLoop().RunUntilIdle();
}

void SecurityKeyIpcClientTest::ClientMessageReceived(
    const std::string& response_payload) {
  last_message_received_ = response_payload;
  OperationComplete(/*failed=*/false);
}

void SecurityKeyIpcClientTest::EstablishConnection(bool expect_error) {
  EXPECT_CALL(*api_provider_, GetSessionServices())
      .WillRepeatedly(Return(&mock_api_));

  EXPECT_CALL(mock_api_, BindSecurityKeyForwarder(_))
      .WillOnce([&](mojo::PendingReceiver<mojom::SecurityKeyForwarder>
                        pending_receiver) {
        if (!simulate_invalid_session_) {
          receiver_.Bind(std::move(pending_receiver));
        }
        // Otherwise drop the receiver.
      });

  ASSERT_TRUE(security_key_ipc_client_->CheckForSecurityKeyIpcServerChannel());

  // Establish the IPC channel so we can begin sending and receiving security
  // key messages.
  security_key_ipc_client_->EstablishIpcConnection(
      base::BindOnce(&SecurityKeyIpcClientTest::ConnectionStateHandler,
                     base::Unretained(this)),
      base::BindOnce(&SecurityKeyIpcClientTest::OperationComplete,
                     base::Unretained(this), /*failed=*/true));
  WaitForOperationComplete();

  ASSERT_EQ(!expect_error, connection_established_);
  ASSERT_EQ(expect_error, operation_failed_);
}

void SecurityKeyIpcClientTest::SendRequestAndResponse(
    const std::string& request_data,
    const std::string& response_data) {
  ASSERT_TRUE(security_key_ipc_client_->SendSecurityKeyRequest(
      request_data,
      base::BindRepeating(&SecurityKeyIpcClientTest::ClientMessageReceived,
                          base::Unretained(this))));
  WaitForOperationComplete();

  ASSERT_FALSE(operation_failed_);
  ASSERT_EQ(request_data, last_message_received_);

  std::move(response_callback_).Run(response_data);
  WaitForOperationComplete();

  ASSERT_FALSE(operation_failed_);
  ASSERT_EQ(response_data, last_message_received_);
}

TEST_F(SecurityKeyIpcClientTest, GenerateSingleSecurityKeyRequest) {
  EstablishConnection();

  SendRequestAndResponse("Auth me!", "You've been authed!");

  security_key_ipc_client_->CloseIpcConnection();
}

TEST_F(SecurityKeyIpcClientTest, GenerateLargeSecurityKeyRequest) {
  EstablishConnection();

  SendRequestAndResponse(std::string(kLargeMessageSizeBytes, 'Y'),
                         std::string(kLargeMessageSizeBytes, 'Z'));

  security_key_ipc_client_->CloseIpcConnection();
}

TEST_F(SecurityKeyIpcClientTest, GenerateReallyLargeSecurityKeyRequest) {
  EstablishConnection();

  SendRequestAndResponse(std::string(kLargeMessageSizeBytes * 2, 'Y'),
                         std::string(kLargeMessageSizeBytes * 2, 'Z'));

  security_key_ipc_client_->CloseIpcConnection();
}

TEST_F(SecurityKeyIpcClientTest, GenerateMultipleSecurityKeyRequest) {
  EstablishConnection();

  SendRequestAndResponse("Auth me 1!", "You've been authed once!");
  SendRequestAndResponse("Auth me 2!", "You've been authed twice!");
  SendRequestAndResponse("Auth me 3!", "You've been authed thrice!");

  security_key_ipc_client_->CloseIpcConnection();
}

TEST_F(SecurityKeyIpcClientTest, ServerClosesConnectionAfterRequestTimeout) {
  EstablishConnection();
  receiver_.reset();
  WaitForOperationComplete();
  ASSERT_TRUE(operation_failed_);
}

TEST_F(SecurityKeyIpcClientTest,
       SecondSecurityKeyRequestBeforeFirstResponseReceived) {
  EstablishConnection();

  ASSERT_TRUE(security_key_ipc_client_->SendSecurityKeyRequest(
      "First Request",
      base::BindRepeating(&SecurityKeyIpcClientTest::ClientMessageReceived,
                          base::Unretained(this))));
  WaitForOperationComplete();
  ASSERT_FALSE(operation_failed_);

  ASSERT_FALSE(security_key_ipc_client_->SendSecurityKeyRequest(
      "Second Request",
      base::BindRepeating(&SecurityKeyIpcClientTest::ClientMessageReceived,
                          base::Unretained(this))));

  // This is the response callback for the first request. Mojo callbacks must be
  // run before they are dropped.
  std::move(response_callback_).Run("");
}

TEST_F(SecurityKeyIpcClientTest, ReceiveSecurityKeyResponseWithEmptyPayload) {
  EstablishConnection();

  ASSERT_TRUE(security_key_ipc_client_->SendSecurityKeyRequest(
      "Valid request",
      base::BindRepeating(&SecurityKeyIpcClientTest::ClientMessageReceived,
                          base::Unretained(this))));
  WaitForOperationComplete();
  ASSERT_FALSE(operation_failed_);

  std::move(response_callback_).Run("");
  WaitForOperationComplete();
  ASSERT_TRUE(operation_failed_);
}

TEST_F(SecurityKeyIpcClientTest, SendRequestBeforeEstablishingConnection) {
  // Sending a request will fail since the IPC connection has not been
  // established.
  ASSERT_FALSE(security_key_ipc_client_->SendSecurityKeyRequest(
      "Too soon!!",
      base::BindRepeating(&SecurityKeyIpcClientTest::ClientMessageReceived,
                          base::Unretained(this))));
}

TEST_F(SecurityKeyIpcClientTest, NonExistentIpcServerChannel) {
  EXPECT_CALL(*api_provider_, GetSessionServices())
      .WillRepeatedly(Return(nullptr));

  // Attempt to establish the connection (should fail since the IPC channel does
  // not exist).
  security_key_ipc_client_->EstablishIpcConnection(
      base::BindOnce(&SecurityKeyIpcClientTest::ConnectionStateHandler,
                     base::Unretained(this)),
      base::BindOnce(&SecurityKeyIpcClientTest::OperationComplete,
                     base::Unretained(this), /*failed=*/true));
  ASSERT_TRUE(operation_failed_);
}

TEST_F(SecurityKeyIpcClientTest, SecurityKeyIpcClientRunningInWrongSession) {
  // Attempting to establish a connection should fail here since the IPC Client
  // is 'running' in the non-remoted session.
  simulate_invalid_session_ = true;
  EstablishConnection(/*expect_error=*/true);
}

}  // namespace remoting
