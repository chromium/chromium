// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/fake_security_key_ipc_client.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "remoting/host/chromoting_messages.h"

namespace remoting {

FakeSecurityKeyIpcClient::FakeSecurityKeyIpcClient(
    const base::Closure& channel_event_callback)
    : channel_event_callback_(channel_event_callback) {
  DCHECK(!channel_event_callback_.is_null());
}

FakeSecurityKeyIpcClient::~FakeSecurityKeyIpcClient() = default;

base::WeakPtr<FakeSecurityKeyIpcClient> FakeSecurityKeyIpcClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool FakeSecurityKeyIpcClient::CheckForSecurityKeyIpcServerChannel() {
  return check_for_ipc_channel_return_value_;
}

void FakeSecurityKeyIpcClient::EstablishIpcConnection(
    const ConnectedCallback& connected_callback,
    const base::Closure& connection_error_callback) {
  if (establish_ipc_connection_should_succeed_) {
    connected_callback.Run(/*connection_usable=*/true);
  } else {
    connection_error_callback.Run();
  }
}

bool FakeSecurityKeyIpcClient::SendSecurityKeyRequest(
    const std::string& request_payload,
    const ResponseCallback& response_callback) {
  if (send_security_request_should_succeed_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(response_callback, security_key_response_payload_));
  }

  return send_security_request_should_succeed_;
}

void FakeSecurityKeyIpcClient::CloseIpcConnection() {
  client_channel_.reset();
  mojo_connection_.reset();
  channel_event_callback_.Run();
}

bool FakeSecurityKeyIpcClient::ConnectViaIpc(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  mojo::PlatformChannelEndpoint endpoint =
      mojo::NamedPlatformChannel::ConnectToServer(server_name);
  if (!endpoint.is_valid())
    return false;

  mojo_connection_ = std::make_unique<mojo::IsolatedConnection>();
  client_channel_ = IPC::Channel::CreateClient(
      mojo_connection_->Connect(std::move(endpoint)).release(), this,
      base::ThreadTaskRunnerHandle::Get());
  return client_channel_->Connect();
}

void FakeSecurityKeyIpcClient::SendSecurityKeyRequestViaIpc(
    const std::string& request_payload) {
  client_channel_->Send(
      new ChromotingRemoteSecurityKeyToNetworkMsg_Request(request_payload));
}

bool FakeSecurityKeyIpcClient::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(FakeSecurityKeyIpcClient, message)
    IPC_MESSAGE_HANDLER(ChromotingNetworkToRemoteSecurityKeyMsg_Response,
                        OnSecurityKeyResponse)
    IPC_MESSAGE_HANDLER(ChromotingNetworkToRemoteSecurityKeyMsg_ConnectionReady,
                        OnConnectionReady)
    IPC_MESSAGE_HANDLER(ChromotingNetworkToRemoteSecurityKeyMsg_InvalidSession,
                        OnInvalidSession)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  CHECK(handled) << "Received unexpected IPC type: " << message.type();
  return handled;
}

void FakeSecurityKeyIpcClient::OnConnectionReady() {
  connection_ready_ = true;
  channel_event_callback_.Run();
}

void FakeSecurityKeyIpcClient::OnInvalidSession() {
  invalid_session_error_ = true;
  channel_event_callback_.Run();
}

void FakeSecurityKeyIpcClient::OnChannelConnected(int32_t peer_pid) {
  ipc_channel_connected_ = true;

  // We don't always want to fire this event as only a subset of tests care
  // about the channel being connected.  Tests that do care can register for it.
  if (on_channel_connected_callback_) {
    on_channel_connected_callback_.Run();
  }
}

void FakeSecurityKeyIpcClient::OnChannelError() {
  ipc_channel_connected_ = false;
  channel_event_callback_.Run();
}

void FakeSecurityKeyIpcClient::OnSecurityKeyResponse(
    const std::string& request_data) {
  last_message_received_ = request_data;
  channel_event_callback_.Run();
}

}  // namespace remoting
