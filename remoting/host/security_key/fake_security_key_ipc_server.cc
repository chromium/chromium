// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/fake_security_key_ipc_server.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "remoting/host/security_key/security_key_auth_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

FakeSecurityKeyIpcServer::FakeSecurityKeyIpcServer(
    int connection_id,
    ClientSessionDetails* client_session_details,
    base::TimeDelta initial_connect_timeout,
    const SecurityKeyAuthHandler::SendMessageCallback& send_message_callback,
    base::OnceClosure connect_callback,
    base::OnceClosure channel_closed_callback)
    : connection_id_(connection_id),
      send_message_callback_(send_message_callback),
      connect_callback_(std::move(connect_callback)),
      channel_closed_callback_(std::move(channel_closed_callback)) {}

FakeSecurityKeyIpcServer::~FakeSecurityKeyIpcServer() = default;

void FakeSecurityKeyIpcServer::SendRequest(const std::string& message_data) {
  send_message_callback_.Run(connection_id_, message_data);
}

void FakeSecurityKeyIpcServer::CloseChannel() {
  ipc_channel_.reset();
  security_key_forwarder_.reset();
  mojo_connection_.reset();
  std::move(channel_closed_callback_).Run();
}

base::WeakPtr<FakeSecurityKeyIpcServer> FakeSecurityKeyIpcServer::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool FakeSecurityKeyIpcServer::OnMessageReceived(const IPC::Message& message) {
  ADD_FAILURE() << "Unexpected call to OnMessageReceived()";
  return false;
}

void FakeSecurityKeyIpcServer::OnChannelConnected(int32_t peer_pid) {
  if (simulate_invalid_session_) {
    CloseChannel();
  } else {
    std::move(connect_callback_).Run();
  }
}

void FakeSecurityKeyIpcServer::BindAssociatedInterface(
    mojo::ScopedInterfaceEndpointHandle handle) {
  EXPECT_FALSE(security_key_forwarder_.is_bound());

  mojo::PendingAssociatedReceiver<mojom::SecurityKeyForwarder> pending_receiver(
      std::move(handle));
  security_key_forwarder_.Bind(std::move(pending_receiver));
}

void FakeSecurityKeyIpcServer::OnSecurityKeyRequest(
    const std::string& request_data,
    OnSecurityKeyRequestCallback callback) {
  // If a second request is received before responding, then close the channel
  // to simulate the behavior in the real implementation.
  if (request_callback_) {
    CloseChannel();
    return;
  }

  request_callback_ = std::move(callback);
  send_message_callback_.Run(connection_id_, request_data);
}

bool FakeSecurityKeyIpcServer::CreateChannel(ChannelEndpoint endpoint,
                                             base::TimeDelta request_timeout) {
  mojo::ScopedMessagePipeHandle pipe;
  if (absl::holds_alternative<mojo::ScopedMessagePipeHandle>(endpoint)) {
    pipe = std::move(absl::get<mojo::ScopedMessagePipeHandle>(endpoint));
  } else {
    mojo::NamedPlatformChannel::Options options;
    options.server_name =
        absl::get<mojo::NamedPlatformChannel::ServerName>(endpoint);
#if BUILDFLAG(IS_WIN)
    options.enforce_uniqueness = false;
#endif
    mojo::NamedPlatformChannel channel(options);
    mojo_connection_ = std::make_unique<mojo::IsolatedConnection>();
    pipe = mojo_connection_->Connect(channel.TakeServerEndpoint());
  }

  ipc_channel_ = IPC::Channel::CreateServer(
      pipe.release(), this, base::SingleThreadTaskRunner::GetCurrentDefault());
  EXPECT_NE(nullptr, ipc_channel_);

  auto* associated_interface_support =
      ipc_channel_->GetAssociatedInterfaceSupport();
  if (!associated_interface_support) {
    ADD_FAILURE() << "Couldn't retrieve GetAssociatedInterfaceSupport helper.";
    ipc_channel_.reset();
    return false;
  }

  associated_interface_support->AddGenericAssociatedInterface(
      mojom::SecurityKeyForwarder::Name_,
      base::BindRepeating(&FakeSecurityKeyIpcServer::BindAssociatedInterface,
                          base::Unretained(this)));

  return ipc_channel_->Connect();
}

bool FakeSecurityKeyIpcServer::SendResponse(const std::string& message_data) {
  last_message_received_ = message_data;

  // This class works in two modes: one in which the test wants the IPC channel
  // to be created and used for notification and the second mode where the test
  // wants to notified of a response via a callback.  If a callback is set then
  // we use it, otherwise we will use the IPC connection to send a message.
  if (send_response_callback_) {
    send_response_callback_.Run();
    return true;
  }

  std::move(request_callback_).Run(message_data);
  return true;
}

FakeSecurityKeyIpcServerFactory::FakeSecurityKeyIpcServerFactory() {
  SecurityKeyIpcServer::SetFactoryForTest(this);
}

FakeSecurityKeyIpcServerFactory::~FakeSecurityKeyIpcServerFactory() {
  SecurityKeyIpcServer::SetFactoryForTest(nullptr);
}

std::unique_ptr<SecurityKeyIpcServer> FakeSecurityKeyIpcServerFactory::Create(
    int connection_id,
    ClientSessionDetails* client_session_details,
    base::TimeDelta initial_connect_timeout,
    const SecurityKeyAuthHandler::SendMessageCallback& send_message_callback,
    base::OnceClosure connect_callback,
    base::OnceClosure done_callback) {
  auto fake_ipc_server = std::make_unique<FakeSecurityKeyIpcServer>(
      connection_id, client_session_details, initial_connect_timeout,
      send_message_callback, std::move(connect_callback),
      std::move(done_callback));

  ipc_server_map_[connection_id] = fake_ipc_server->AsWeakPtr();

  return std::move(fake_ipc_server);
}

base::WeakPtr<FakeSecurityKeyIpcServer>
FakeSecurityKeyIpcServerFactory::GetIpcServerObject(int connection_id) {
  return ipc_server_map_[connection_id];
}

}  // namespace remoting
