// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_ipc_client.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"
#include "remoting/host/security_key/security_key_ipc_constants.h"

#if BUILDFLAG(IS_WIN)
#include <Windows.h>
#endif

namespace remoting {

SecurityKeyIpcClient::SecurityKeyIpcClient()
    : named_channel_handle_(remoting::GetSecurityKeyIpcChannel()) {}

SecurityKeyIpcClient::~SecurityKeyIpcClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool SecurityKeyIpcClient::CheckForSecurityKeyIpcServerChannel() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!channel_handle_.is_valid()) {
    channel_handle_ =
        mojo::NamedPlatformChannel::ConnectToServer(named_channel_handle_);
  }
  return channel_handle_.is_valid();
}

void SecurityKeyIpcClient::EstablishIpcConnection(
    ConnectedCallback connected_callback,
    base::OnceClosure connection_error_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(connected_callback);
  DCHECK(connection_error_callback);
  DCHECK(!ipc_channel_);

  connected_callback_ = std::move(connected_callback);
  connection_error_callback_ = std::move(connection_error_callback);

  ConnectToIpcChannel();
}

bool SecurityKeyIpcClient::SendSecurityKeyRequest(
    const std::string& request_payload,
    ResponseCallback response_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!request_payload.empty());
  DCHECK(response_callback);

  if (!ipc_channel_) {
    LOG(ERROR) << "Request made before IPC connection was established.";
    return false;
  }

  if (response_callback_) {
    LOG(ERROR)
        << "Request made while waiting for a response to a previous request.";
    return false;
  }

  response_callback_ = std::move(response_callback);
  security_key_forwarder_->OnSecurityKeyRequest(
      request_payload,
      base::BindOnce(&SecurityKeyIpcClient::OnSecurityKeyResponse,
                     base::Unretained(this)));

  return true;
}

void SecurityKeyIpcClient::CloseIpcConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ipc_channel_.reset();
}

void SecurityKeyIpcClient::SetIpcChannelHandleForTest(
    const mojo::NamedPlatformChannel::ServerName& server_name) {
  named_channel_handle_ = server_name;
}

void SecurityKeyIpcClient::SetExpectedIpcServerSessionIdForTest(
    uint32_t expected_session_id) {
  expected_ipc_server_session_id_ = expected_session_id;
}

bool SecurityKeyIpcClient::OnMessageReceived(const IPC::Message& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(false) << "Unexpected call to OnMessageReceived: " << message.type();
  return false;
}

void SecurityKeyIpcClient::OnChannelConnected(int32_t peer_pid) {
  DCHECK(thread_checker_.CalledOnValidThread());

#if BUILDFLAG(IS_WIN)
  DWORD peer_session_id;
  if (!ProcessIdToSessionId(peer_pid, &peer_session_id)) {
    PLOG(ERROR) << "ProcessIdToSessionId failed";
    std::move(connection_error_callback_).Run();
    return;
  }

  if (peer_session_id != expected_ipc_server_session_id_) {
    LOG(ERROR)
        << "Cannot establish connection with IPC server running in session: "
        << peer_session_id;
    std::move(connection_error_callback_).Run();
    return;
  }
#endif  // BUILDFLAG(IS_WIN)

  std::move(connected_callback_).Run();
}

void SecurityKeyIpcClient::OnChannelError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  security_key_forwarder_.reset();
  ipc_channel_.reset();
  if (connection_error_callback_) {
    std::move(connection_error_callback_).Run();
  }
}

void SecurityKeyIpcClient::OnSecurityKeyResponse(
    const std::string& response_data) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!response_data.empty()) {
    std::move(response_callback_).Run(response_data);
  } else {
    LOG(ERROR) << "Invalid response received";
    if (connection_error_callback_) {
      std::move(connection_error_callback_).Run();
    }
  }
}

void SecurityKeyIpcClient::ConnectToIpcChannel() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Verify that any existing IPC connection has been closed.
  CloseIpcConnection();

  if (!channel_handle_.is_valid() && !CheckForSecurityKeyIpcServerChannel()) {
    LOG(ERROR) << "Invalid channel handle.";
    OnChannelError();
    return;
  }

  ipc_channel_ = IPC::Channel::CreateClient(
      mojo_connection_.Connect(std::move(channel_handle_)).release(), this,
      base::SingleThreadTaskRunner::GetCurrentDefault());

  if (!ipc_channel_->Connect()) {
    LOG(ERROR) << "Failed to connect IPC Channel.";
    OnChannelError();
    return;
  }

  auto* associated_interface_support =
      ipc_channel_->GetAssociatedInterfaceSupport();

  associated_interface_support->GetRemoteAssociatedInterface(
      security_key_forwarder_.BindNewEndpointAndPassReceiver());
}

}  // namespace remoting
