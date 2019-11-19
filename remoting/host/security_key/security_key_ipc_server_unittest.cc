// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_ipc_server.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/security_key/fake_security_key_ipc_client.h"
#include "remoting/host/security_key/security_key_ipc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace {
const int kTestConnectionId = 42;
const int kInitialConnectTimeoutMs = 250;
const int kConnectionTimeoutErrorDeltaMs = 250;
const int kLargeResponseTimeoutMs = 500;
const int kLargeMessageSizeBytes = 256 * 1024;
}  // namespace

namespace remoting {

class SecurityKeyIpcServerTest : public testing::Test,
                                 public ClientSessionDetails {
 public:
  SecurityKeyIpcServerTest();
  ~SecurityKeyIpcServerTest() override;

  // Passed to the object used for testing to be called back to signal
  // completion of an IPC channel state change or reception of an IPC message.
  void OperationComplete();

  // Used as a callback to signal receipt of a security key request message.
  void SendRequestToClient(int connection_id, const std::string& data);

 protected:
  // testing::Test interface.
  void TearDown() override;

  // Returns a unique IPC channel name which prevents conflicts when running
  // tests concurrently.
  mojo::NamedPlatformChannel::ServerName GetUniqueTestChannelName();

  // Waits until the current |run_loop_| instance is signaled, then resets it.
  void WaitForOperationComplete();

  // Waits until all tasks have been run on the current message loop.
  void RunPendingTasks();

  // ClientSessionControl overrides:
  ClientSessionControl* session_control() override { return nullptr; }
  uint32_t desktop_session_id() const override { return peer_session_id_; }

  // IPC tests require a valid MessageLoop to run.
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  // Used to allow |message_loop_| to run during tests.  The instance is reset
  // after each stage of the tests has been completed.
  std::unique_ptr<base::RunLoop> run_loop_;

  // The object under test.
  std::unique_ptr<SecurityKeyIpcServer> security_key_ipc_server_;

  // Used to validate the object under test uses the correct ID when
  // communicating over the IPC channel.
  int last_connection_id_received_ = -1;

  // Stores the contents of the last IPC message received for validation.
  std::string last_message_received_;

  uint32_t peer_session_id_ = UINT32_MAX;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityKeyIpcServerTest);
};

SecurityKeyIpcServerTest::SecurityKeyIpcServerTest()
    : run_loop_(new base::RunLoop()) {
#if defined(OS_WIN)
  EXPECT_TRUE(ProcessIdToSessionId(
      GetCurrentProcessId(), reinterpret_cast<DWORD*>(&peer_session_id_)));
#endif  // defined(OS_WIN)

  security_key_ipc_server_ = remoting::SecurityKeyIpcServer::Create(
      kTestConnectionId, this,
      base::TimeDelta::FromMilliseconds(kInitialConnectTimeoutMs),
      base::Bind(&SecurityKeyIpcServerTest::SendRequestToClient,
                 base::Unretained(this)),
      base::DoNothing(),
      base::Bind(&SecurityKeyIpcServerTest::OperationComplete,
                 base::Unretained(this)));
}

SecurityKeyIpcServerTest::~SecurityKeyIpcServerTest() = default;

void SecurityKeyIpcServerTest::OperationComplete() {
  run_loop_->Quit();
}

void SecurityKeyIpcServerTest::WaitForOperationComplete() {
  run_loop_->Run();
  run_loop_.reset(new base::RunLoop());
}

void SecurityKeyIpcServerTest::RunPendingTasks() {
  // Run until there are no pending work items in the queue.
  base::RunLoop().RunUntilIdle();
}

void SecurityKeyIpcServerTest::TearDown() {
  RunPendingTasks();
}

void SecurityKeyIpcServerTest::SendRequestToClient(int connection_id,
                                                   const std::string& data) {
  last_connection_id_received_ = connection_id;
  last_message_received_ = data;
  OperationComplete();
}

mojo::NamedPlatformChannel::ServerName
SecurityKeyIpcServerTest::GetUniqueTestChannelName() {
  std::string name = GetChannelNamePathPrefixForTest() +
                     "Super_Awesome_Test_Channel." +
                     IPC::Channel::GenerateUniqueRandomChannelID();
  return mojo::NamedPlatformChannel::ServerNameFromUTF8(name);
}

TEST_F(SecurityKeyIpcServerTest, HandleSingleSecurityKeyRequest) {
  mojo::NamedPlatformChannel::ServerName server_name =
      GetUniqueTestChannelName();
  ASSERT_TRUE(security_key_ipc_server_->CreateChannel(
      server_name,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));

  // Create a fake client and connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client(base::Bind(
      &SecurityKeyIpcServerTest::OperationComplete, base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(server_name));
  WaitForOperationComplete();

  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());
  ASSERT_FALSE(fake_ipc_client.invalid_session_error());
  ASSERT_TRUE(fake_ipc_client.connection_ready());

  // Send a request from the IPC client to the IPC server.
  std::string request_data("Blergh!");
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data);
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(kTestConnectionId, last_connection_id_received_);
  ASSERT_EQ(request_data, last_message_received_);

  // Send a response from the IPC server to the IPC client.
  std::string response_data("Blargh!");
  ASSERT_TRUE(security_key_ipc_server_->SendResponse(response_data));
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(response_data, fake_ipc_client.last_message_received());

  // Typically the client will be the one to close the connection.
  fake_ipc_client.CloseIpcConnection();
}

TEST_F(SecurityKeyIpcServerTest, HandleLargeSecurityKeyRequest) {
  mojo::NamedPlatformChannel::ServerName server_name =
      GetUniqueTestChannelName();
  ASSERT_TRUE(security_key_ipc_server_->CreateChannel(
      server_name,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));

  // Create a fake client and connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client(base::Bind(
      &SecurityKeyIpcServerTest::OperationComplete, base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(server_name));
  WaitForOperationComplete();

  ASSERT_FALSE(fake_ipc_client.invalid_session_error());
  ASSERT_TRUE(fake_ipc_client.connection_ready());
  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());

  // Send a request from the IPC client to the IPC server.
  std::string request_data(kLargeMessageSizeBytes, 'Y');
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data);
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(kTestConnectionId, last_connection_id_received_);
  ASSERT_EQ(request_data, last_message_received_);

  // Send a response from the IPC server to the IPC client.
  std::string response_data(kLargeMessageSizeBytes, 'Z');
  ASSERT_TRUE(security_key_ipc_server_->SendResponse(response_data));
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(response_data, fake_ipc_client.last_message_received());

  // Typically the client will be the one to close the connection.
  fake_ipc_client.CloseIpcConnection();
}

TEST_F(SecurityKeyIpcServerTest, HandleReallyLargeSecurityKeyRequest) {
  mojo::NamedPlatformChannel::ServerName server_name =
      GetUniqueTestChannelName();
  ASSERT_TRUE(security_key_ipc_server_->CreateChannel(
      server_name,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));

  // Create a fake client and connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client(base::Bind(
      &SecurityKeyIpcServerTest::OperationComplete, base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(server_name));
  WaitForOperationComplete();

  ASSERT_FALSE(fake_ipc_client.invalid_session_error());
  ASSERT_TRUE(fake_ipc_client.connection_ready());
  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());

  // Send a request from the IPC client to the IPC server.
  std::string request_data(kLargeMessageSizeBytes * 2, 'Y');
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data);
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(kTestConnectionId, last_connection_id_received_);
  ASSERT_EQ(request_data, last_message_received_);

  // Send a response from the IPC server to the IPC client.
  std::string response_data(kLargeMessageSizeBytes * 2, 'Z');
  ASSERT_TRUE(security_key_ipc_server_->SendResponse(response_data));
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(response_data, fake_ipc_client.last_message_received());

  // Typically the client will be the one to close the connection.
  fake_ipc_client.CloseIpcConnection();
}

TEST_F(SecurityKeyIpcServerTest, HandleMultipleSecurityKeyRequests) {
  mojo::NamedPlatformChannel::ServerName server_name =
      GetUniqueTestChannelName();
  ASSERT_TRUE(security_key_ipc_server_->CreateChannel(
      server_name,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));

  // Create a fake client and connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client(base::Bind(
      &SecurityKeyIpcServerTest::OperationComplete, base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(server_name));
  WaitForOperationComplete();

  ASSERT_FALSE(fake_ipc_client.invalid_session_error());
  ASSERT_TRUE(fake_ipc_client.connection_ready());
  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());

  // Send a request from the IPC client to the IPC server.
  std::string request_data_1("Blergh!");
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data_1);
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(kTestConnectionId, last_connection_id_received_);
  ASSERT_EQ(request_data_1, last_message_received_);

  // Send a response from the IPC server to the IPC client.
  std::string response_data_1("Blargh!");
  ASSERT_TRUE(security_key_ipc_server_->SendResponse(response_data_1));
  WaitForOperationComplete();

  // Verify the response was received.
  ASSERT_EQ(response_data_1, fake_ipc_client.last_message_received());

  // Send a request from the IPC client to the IPC server.
  std::string request_data_2("Bleh!");
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data_2);
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(kTestConnectionId, last_connection_id_received_);
  ASSERT_EQ(request_data_2, last_message_received_);

  // Send a response from the IPC server to the IPC client.
  std::string response_data_2("Meh!");
  ASSERT_TRUE(security_key_ipc_server_->SendResponse(response_data_2));
  WaitForOperationComplete();

  // Verify the response was received.
  ASSERT_EQ(response_data_2, fake_ipc_client.last_message_received());

  // Typically the client will be the one to close the connection.
  fake_ipc_client.CloseIpcConnection();
}

TEST_F(SecurityKeyIpcServerTest, InitialIpcConnectionTimeout_ConnectOnly) {
  // Create a channel, then wait for the done callback to be called indicating
  // the connection was closed.  This test simulates the IPC Server being
  // created, the client connecting to the OS channel, but never communicating
  // over the channel.
  mojo::NamedPlatformChannel::ServerName server_name =
      GetUniqueTestChannelName();
  ASSERT_TRUE(security_key_ipc_server_->CreateChannel(
      server_name,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));
  base::Time start_time(base::Time::NowFromSystemTime());
  mojo::PlatformChannelEndpoint client_endpoint =
      mojo::NamedPlatformChannel::ConnectToServer(server_name);
  WaitForOperationComplete();
  base::TimeDelta elapsed_time = base::Time::NowFromSystemTime() - start_time;

  ASSERT_NEAR(elapsed_time.InMilliseconds(), kInitialConnectTimeoutMs,
              kConnectionTimeoutErrorDeltaMs);
}

TEST_F(SecurityKeyIpcServerTest,
       InitialIpcConnectionTimeout_ConnectAndEstablishMojoConnection) {
  // Create a channel, then wait for the done callback to be called indicating
  // the connection was closed.  This test simulates the IPC Server being
  // created, the client establishing a mojo connection, but never constructing
  // an IPC::Channel over it.
  mojo::NamedPlatformChannel::ServerName server_name =
      GetUniqueTestChannelName();
  ASSERT_TRUE(security_key_ipc_server_->CreateChannel(
      server_name,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));
  base::Time start_time(base::Time::NowFromSystemTime());
  mojo::IsolatedConnection mojo_connection;
  mojo::ScopedMessagePipeHandle client_pipe = mojo_connection.Connect(
      mojo::NamedPlatformChannel::ConnectToServer(server_name));
  WaitForOperationComplete();
  base::TimeDelta elapsed_time = base::Time::NowFromSystemTime() - start_time;

  ASSERT_NEAR(elapsed_time.InMilliseconds(), kInitialConnectTimeoutMs,
              kConnectionTimeoutErrorDeltaMs);
}

// Flaky on mac, https://crbug.com/936583
#if defined(OS_MACOSX)
#define MAYBE_NoSecurityKeyRequestTimeout DISABLED_NoSecurityKeyRequestTimeout
#else
#define MAYBE_NoSecurityKeyRequestTimeout NoSecurityKeyRequestTimeout
#endif
TEST_F(SecurityKeyIpcServerTest, MAYBE_NoSecurityKeyRequestTimeout) {
  // Create a channel and connect to it via IPC but do not send a request.
  // The channel should be closed and cleaned up if the IPC client does not
  // issue a request within the specified timeout period.
  mojo::NamedPlatformChannel::ServerName server_name =
      GetUniqueTestChannelName();
  ASSERT_TRUE(security_key_ipc_server_->CreateChannel(
      server_name,
      /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));

  // Create a fake client and connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client(base::Bind(
      &SecurityKeyIpcServerTest::OperationComplete, base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(server_name));
  WaitForOperationComplete();

  ASSERT_FALSE(fake_ipc_client.invalid_session_error());
  ASSERT_TRUE(fake_ipc_client.connection_ready());
  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());

  // Now that a connection has been established, we wait for the timeout.
  base::Time start_time(base::Time::NowFromSystemTime());
  WaitForOperationComplete();
  base::TimeDelta elapsed_time = base::Time::NowFromSystemTime() - start_time;

  ASSERT_NEAR(elapsed_time.InMilliseconds(), kInitialConnectTimeoutMs,
              kConnectionTimeoutErrorDeltaMs);
}

TEST_F(SecurityKeyIpcServerTest, SecurityKeyResponseTimeout) {
  // Create a channel, connect to it via IPC, and issue a request, but do
  // not send a response.  This simulates a client-side timeout.
  base::TimeDelta request_timeout(base::TimeDelta::FromMilliseconds(50));
  mojo::NamedPlatformChannel::ServerName server_name =
      GetUniqueTestChannelName();
  ASSERT_TRUE(
      security_key_ipc_server_->CreateChannel(server_name, request_timeout));

  // Create a fake client and connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client(base::Bind(
      &SecurityKeyIpcServerTest::OperationComplete, base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(server_name));
  WaitForOperationComplete();

  ASSERT_FALSE(fake_ipc_client.invalid_session_error());
  ASSERT_TRUE(fake_ipc_client.connection_ready());
  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());

  // Now that a connection has been established, we issue a request and
  // then wait for the timeout.
  std::string request_data("I can haz Auth?");
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data);
  WaitForOperationComplete();

  // Leave the request hanging until it times out...
  base::Time start_time(base::Time::NowFromSystemTime());
  WaitForOperationComplete();
  base::TimeDelta elapsed_time = base::Time::NowFromSystemTime() - start_time;

  ASSERT_LT(elapsed_time.InMilliseconds(), kLargeResponseTimeoutMs);
}

TEST_F(SecurityKeyIpcServerTest, SendResponseTimeout) {
  // Create a channel, connect to it via IPC, issue a request, and send
  // a response, but do not close the channel after that.  The connection
  // should be terminated after the initial timeout period has elapsed.
  base::TimeDelta request_timeout(base::TimeDelta::FromMilliseconds(500));
  mojo::NamedPlatformChannel::ServerName server_name =
      GetUniqueTestChannelName();
  ASSERT_TRUE(
      security_key_ipc_server_->CreateChannel(server_name, request_timeout));

  // Create a fake client and connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client(base::Bind(
      &SecurityKeyIpcServerTest::OperationComplete, base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(server_name));
  WaitForOperationComplete();

  ASSERT_FALSE(fake_ipc_client.invalid_session_error());
  ASSERT_TRUE(fake_ipc_client.connection_ready());
  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());

  // Issue a request.
  std::string request_data("Auth me yo!");
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data);
  WaitForOperationComplete();

  // Send a response from the IPC server to the IPC client.
  std::string response_data("OK, the secret code is 1-2-3-4-5");
  ASSERT_TRUE(security_key_ipc_server_->SendResponse(response_data));
  WaitForOperationComplete();

  // Now wait for the timeout period for the connection to be torn down.
  base::Time start_time(base::Time::NowFromSystemTime());
  WaitForOperationComplete();
  base::TimeDelta elapsed_time = base::Time::NowFromSystemTime() - start_time;

  ASSERT_NEAR(elapsed_time.InMilliseconds(), request_timeout.InMilliseconds(),
              kConnectionTimeoutErrorDeltaMs);
}

TEST_F(SecurityKeyIpcServerTest, CleanupPendingConnection) {
#if defined(OS_MACOSX)
  // Named servers when using ChannelMac are an exclusive resources, and it is
  // not possible to create an instance of a server endpoint while another one
  // exists. Creating the servers in a loop below will flakily fail because the
  // channel shutdown is a series of asynchronous tasks posted on the IO
  // thread, and there is not a way to synchronize it with the test main thread.
  return;
#endif  // defined(OS_MACOSX)

  // Test that servers correctly close pending OS connections on
  // |server_name|. If multiple servers do remain, the client may happen to
  // connect to the correct server, so create and delete many servers.
  mojo::NamedPlatformChannel::ServerName server_name =
      GetUniqueTestChannelName();
  for (int i = 0; i < 100; i++) {
    security_key_ipc_server_ = remoting::SecurityKeyIpcServer::Create(
        kTestConnectionId, this,
        base::TimeDelta::FromMilliseconds(kInitialConnectTimeoutMs),
        base::Bind(&SecurityKeyIpcServerTest::SendRequestToClient,
                   base::Unretained(this)),
        base::DoNothing(),
        base::Bind(&SecurityKeyIpcServerTest::OperationComplete,
                   base::Unretained(this)));
    ASSERT_TRUE(security_key_ipc_server_->CreateChannel(
        server_name,
        /*request_timeout=*/base::TimeDelta::FromMilliseconds(500)));
  }
  // The mojo system posts tasks as part of its cleanup, so run them all.
  base::RunLoop().RunUntilIdle();

  // Create a fake client and connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client(base::Bind(
      &SecurityKeyIpcServerTest::OperationComplete, base::Unretained(this)));
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(server_name));
  WaitForOperationComplete();

  ASSERT_FALSE(fake_ipc_client.invalid_session_error());
  ASSERT_TRUE(fake_ipc_client.connection_ready());
  ASSERT_TRUE(fake_ipc_client.ipc_channel_connected());

  // Send a request from the IPC client to the IPC server.
  std::string request_data("Blergh!");
  fake_ipc_client.SendSecurityKeyRequestViaIpc(request_data);
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(kTestConnectionId, last_connection_id_received_);
  ASSERT_EQ(request_data, last_message_received_);

  // Send a response from the IPC server to the IPC client.
  std::string response_data("Blargh!");
  ASSERT_TRUE(security_key_ipc_server_->SendResponse(response_data));
  WaitForOperationComplete();

  // Verify the request was received.
  ASSERT_EQ(response_data, fake_ipc_client.last_message_received());

  // Typically the client will be the one to close the connection.
  fake_ipc_client.CloseIpcConnection();
}

#if defined(OS_WIN)
TEST_F(SecurityKeyIpcServerTest, IpcConnectionFailsFromInvalidSession) {
  // Change the expected session ID to not match the current session.
  peer_session_id_++;

  base::TimeDelta request_timeout(base::TimeDelta::FromMilliseconds(500));
  mojo::NamedPlatformChannel::ServerName server_name =
      GetUniqueTestChannelName();
  ASSERT_TRUE(
      security_key_ipc_server_->CreateChannel(server_name, request_timeout));

  // Create a fake client and attempt to connect to the IPC server channel.
  FakeSecurityKeyIpcClient fake_ipc_client{base::DoNothing()};
  ASSERT_TRUE(fake_ipc_client.ConnectViaIpc(server_name));
  WaitForOperationComplete();

  // Verify the connection failed.
  ASSERT_TRUE(fake_ipc_client.invalid_session_error());
  ASSERT_FALSE(fake_ipc_client.connection_ready());

  RunPendingTasks();
  ASSERT_FALSE(fake_ipc_client.ipc_channel_connected());
}
#endif  // defined(OS_WIN)

}  // namespace remoting
