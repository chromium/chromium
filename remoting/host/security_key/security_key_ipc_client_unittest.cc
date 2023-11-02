// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_ipc_client.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "remoting/host/security_key/fake_security_key_ipc_server.h"
#include "remoting/host/security_key/security_key_ipc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace {
const int kTestConnectionId = 1;
const char kNonexistentIpcChannelName[] = "Nonexistent_IPC_Channel";
const char kValidIpcChannelName[] = "SecurityKeyIpcClientTest";
const int kLargeMessageSizeBytes = 256 * 1024;
}  // namespace

namespace remoting {

class SecurityKeyIpcClientTest : public testing::Test {
 public:
  SecurityKeyIpcClientTest();

  SecurityKeyIpcClientTest(const SecurityKeyIpcClientTest&) = delete;
  SecurityKeyIpcClientTest& operator=(const SecurityKeyIpcClientTest&) = delete;

  ~SecurityKeyIpcClientTest() override;

  // Passed to the object used for testing to be called back to signal
  // completion of an IPC channel state change or reception of an IPC message.
  void OperationComplete(bool failed);

  // Callback used to signal when the IPC channel is ready for messages.
  void ConnectionStateHandler();

  // Used as a callback given to the object under test, expected to be called
  // back when a security key request is received by it.
  void SendMessageToClient(int connection_id, const std::string& data);

  // Used as a callback given to the object under test, expected to be called
  // back when a security key response is sent.
  void ClientMessageReceived(const std::string& response_payload);

 protected:
  // testing::Test interface.
  void SetUp() override;

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

  // Creates a unique IPC channel name to use for testing.
  mojo::NamedPlatformChannel::ServerName GenerateUniqueTestChannelName();

  // IPC tests require a valid MessageLoop to run.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  // Used to allow |message_loop_| to run during tests.  The instance is reset
  // after each stage of the tests has been completed.
  std::unique_ptr<base::RunLoop> run_loop_;

  // The object under test.
  SecurityKeyIpcClient security_key_ipc_client_;

  // Used to send/receive security key IPC messages for testing.
  FakeSecurityKeyIpcServer fake_ipc_server_;

  // Stores the current session ID on supported platforms.
  uint32_t session_id_ = 0;

  // Tracks the success/failure of the last async operation.
  bool operation_failed_ = false;

  // Tracks whether the IPC channel connection has been established.
  bool connection_established_ = false;

  // Used to drive invalid session behavior for testing the client.
  bool simulate_invalid_session_ = false;

  // Used to validate the object under test uses the correct ID when
  // communicating over the IPC channel.
  int last_connection_id_received_ = -1;

  // Stores the contents of the last IPC message received for validation.
  std::string last_message_received_;
};

SecurityKeyIpcClientTest::SecurityKeyIpcClientTest()
    : run_loop_(new base::RunLoop()),
      fake_ipc_server_(
          kTestConnectionId,
          /*client_session_details=*/nullptr,
          /*initial_connect_timeout=*/base::Milliseconds(500),
          base::BindRepeating(&SecurityKeyIpcClientTest::SendMessageToClient,
                              base::Unretained(this)),
          base::BindOnce(&SecurityKeyIpcClientTest::ConnectionStateHandler,
                         base::Unretained(this)),
          base::BindOnce(&SecurityKeyIpcClientTest::OperationComplete,
                         base::Unretained(this),
                         /*failed=*/false)) {}

SecurityKeyIpcClientTest::~SecurityKeyIpcClientTest() = default;

void SecurityKeyIpcClientTest::SetUp() {
#if BUILDFLAG(IS_WIN)
  DWORD session_id = 0;
  // If we are on Windows, then we need to set the correct session ID or the
  // IPC connection will not be created successfully.
  ASSERT_TRUE(ProcessIdToSessionId(GetCurrentProcessId(), &session_id));
  session_id_ = session_id;
  security_key_ipc_client_.SetExpectedIpcServerSessionIdForTest(session_id_);
#endif  // BUILDFLAG(IS_WIN)
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

void SecurityKeyIpcClientTest::SendMessageToClient(int connection_id,
                                                   const std::string& data) {
  last_connection_id_received_ = connection_id;
  last_message_received_ = data;
  OperationComplete(/*failed=*/false);
}

void SecurityKeyIpcClientTest::ClientMessageReceived(
    const std::string& response_payload) {
  last_message_received_ = response_payload;
  OperationComplete(/*failed=*/false);
}

mojo::NamedPlatformChannel::ServerName
SecurityKeyIpcClientTest::GenerateUniqueTestChannelName() {
  return mojo::NamedPlatformChannel::ServerNameFromUTF8(
      GetChannelNamePathPrefixForTest() + kValidIpcChannelName +
      IPC::Channel::GenerateUniqueRandomChannelID());
}

void SecurityKeyIpcClientTest::EstablishConnection(bool expect_error) {
  if (simulate_invalid_session_) {
    fake_ipc_server_.set_simulate_invalid_session(true);
  }

  // Start up the security key forwarding session IPC channel first, that way
  // we can provide the channel using the fake SecurityKeyAuthHandler later on.
  mojo::NamedPlatformChannel::ServerName server_name =
      GenerateUniqueTestChannelName();
  security_key_ipc_client_.SetIpcChannelHandleForTest(server_name);
  ASSERT_TRUE(fake_ipc_server_.CreateChannel(
      server_name,
      /*request_timeout=*/base::Milliseconds(500)));

  ASSERT_TRUE(security_key_ipc_client_.CheckForSecurityKeyIpcServerChannel());

  // Establish the IPC channel so we can begin sending and receiving security
  // key messages.
  security_key_ipc_client_.EstablishIpcConnection(
      base::BindOnce(&SecurityKeyIpcClientTest::ConnectionStateHandler,
                     base::Unretained(this)),
      base::BindOnce(&SecurityKeyIpcClientTest::OperationComplete,
                     base::Unretained(this), /*failed=*/true));
  WaitForOperationComplete();

  ASSERT_TRUE(connection_established_);
  ASSERT_EQ(expect_error, operation_failed_);
}

void SecurityKeyIpcClientTest::SendRequestAndResponse(
    const std::string& request_data,
    const std::string& response_data) {
  ASSERT_TRUE(security_key_ipc_client_.SendSecurityKeyRequest(
      request_data,
      base::BindRepeating(&SecurityKeyIpcClientTest::ClientMessageReceived,
                          base::Unretained(this))));
  WaitForOperationComplete();

  ASSERT_FALSE(operation_failed_);
  ASSERT_EQ(kTestConnectionId, last_connection_id_received_);
  ASSERT_EQ(request_data, last_message_received_);

  ASSERT_TRUE(fake_ipc_server_.SendResponse(response_data));
  WaitForOperationComplete();

  ASSERT_FALSE(operation_failed_);
  ASSERT_EQ(response_data, last_message_received_);
}

TEST_F(SecurityKeyIpcClientTest, GenerateSingleSecurityKeyRequest) {
  EstablishConnection();

  SendRequestAndResponse("Auth me!", "You've been authed!");

  security_key_ipc_client_.CloseIpcConnection();
}

TEST_F(SecurityKeyIpcClientTest, GenerateLargeSecurityKeyRequest) {
  EstablishConnection();

  SendRequestAndResponse(std::string(kLargeMessageSizeBytes, 'Y'),
                         std::string(kLargeMessageSizeBytes, 'Z'));

  security_key_ipc_client_.CloseIpcConnection();
}

TEST_F(SecurityKeyIpcClientTest, GenerateReallyLargeSecurityKeyRequest) {
  EstablishConnection();

  SendRequestAndResponse(std::string(kLargeMessageSizeBytes * 2, 'Y'),
                         std::string(kLargeMessageSizeBytes * 2, 'Z'));

  security_key_ipc_client_.CloseIpcConnection();
}

TEST_F(SecurityKeyIpcClientTest, GenerateMultipleSecurityKeyRequest) {
  EstablishConnection();

  SendRequestAndResponse("Auth me 1!", "You've been authed once!");
  SendRequestAndResponse("Auth me 2!", "You've been authed twice!");
  SendRequestAndResponse("Auth me 3!", "You've been authed thrice!");

  security_key_ipc_client_.CloseIpcConnection();
}

TEST_F(SecurityKeyIpcClientTest, ServerClosesConnectionAfterRequestTimeout) {
  EstablishConnection();
  fake_ipc_server_.CloseChannel();
  WaitForOperationComplete();
  ASSERT_TRUE(operation_failed_);
}

TEST_F(SecurityKeyIpcClientTest,
       SecondSecurityKeyRequestBeforeFirstResponseReceived) {
  EstablishConnection();

  ASSERT_TRUE(security_key_ipc_client_.SendSecurityKeyRequest(
      "First Request",
      base::BindRepeating(&SecurityKeyIpcClientTest::ClientMessageReceived,
                          base::Unretained(this))));
  WaitForOperationComplete();
  ASSERT_FALSE(operation_failed_);

  ASSERT_FALSE(security_key_ipc_client_.SendSecurityKeyRequest(
      "Second Request",
      base::BindRepeating(&SecurityKeyIpcClientTest::ClientMessageReceived,
                          base::Unretained(this))));
}

TEST_F(SecurityKeyIpcClientTest, ReceiveSecurityKeyResponseWithEmptyPayload) {
  EstablishConnection();

  ASSERT_TRUE(security_key_ipc_client_.SendSecurityKeyRequest(
      "Valid request",
      base::BindRepeating(&SecurityKeyIpcClientTest::ClientMessageReceived,
                          base::Unretained(this))));
  WaitForOperationComplete();
  ASSERT_FALSE(operation_failed_);

  ASSERT_TRUE(fake_ipc_server_.SendResponse(""));
  WaitForOperationComplete();
  ASSERT_TRUE(operation_failed_);
}

TEST_F(SecurityKeyIpcClientTest, SendRequestBeforeEstablishingConnection) {
  // Sending a request will fail since the IPC connection has not been
  // established.
  ASSERT_FALSE(security_key_ipc_client_.SendSecurityKeyRequest(
      "Too soon!!",
      base::BindRepeating(&SecurityKeyIpcClientTest::ClientMessageReceived,
                          base::Unretained(this))));
}

TEST_F(SecurityKeyIpcClientTest, SendRequestBeforeReceivingResponse) {
  EstablishConnection();

  ASSERT_TRUE(security_key_ipc_client_.SendSecurityKeyRequest(
      "Auth me dude!",
      base::BindRepeating(&SecurityKeyIpcClientTest::ClientMessageReceived,
                          base::Unretained(this))));
  WaitForOperationComplete();
  ASSERT_FALSE(operation_failed_);

  ASSERT_FALSE(security_key_ipc_client_.SendSecurityKeyRequest(
      "Me too!",
      base::BindRepeating(&SecurityKeyIpcClientTest::ClientMessageReceived,
                          base::Unretained(this))));
}

TEST_F(SecurityKeyIpcClientTest, NonExistentIpcServerChannel) {
  security_key_ipc_client_.SetIpcChannelHandleForTest(
      mojo::NamedPlatformChannel::ServerNameFromUTF8(
          kNonexistentIpcChannelName));

  // Attempt to establish the connection (should fail since the IPC channel does
  // not exist).
  security_key_ipc_client_.EstablishIpcConnection(
      base::BindOnce(&SecurityKeyIpcClientTest::ConnectionStateHandler,
                     base::Unretained(this)),
      base::BindOnce(&SecurityKeyIpcClientTest::OperationComplete,
                     base::Unretained(this), /*failed=*/true));
  WaitForOperationComplete();
  ASSERT_TRUE(operation_failed_);
}

#if BUILDFLAG(IS_WIN)
TEST_F(SecurityKeyIpcClientTest, SecurityKeyIpcServerRunningInWrongSession) {
  // Set the expected session Id to a different session than we are running in.
  security_key_ipc_client_.SetExpectedIpcServerSessionIdForTest(session_id_ +
                                                                1);

  // Attempting to establish a connection should fail here since the IPC Server
  // is 'running' in a different session than expected.  The success callback
  // will be called by the IPC server since it thinks the connection is valid,
  // but we will have already started tearing down the connection since it
  // failed at the client end.
  EstablishConnection(/*expect_error=*/true);
}

TEST_F(SecurityKeyIpcClientTest, SecurityKeyIpcClientRunningInWrongSession) {
  // Attempting to establish a connection should fail here since the IPC Client
  // is 'running' in the non-remoted session.
  simulate_invalid_session_ = true;
  EstablishConnection(/*expect_error=*/true);
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace remoting
