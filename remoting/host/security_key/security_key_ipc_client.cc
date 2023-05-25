// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_ipc_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "remoting/base/logging.h"
#include "remoting/host/chromoting_host_services_client.h"
#include "remoting/host/security_key/security_key_ipc_constants.h"

#if BUILDFLAG(IS_WIN)
#include <Windows.h>
#endif

namespace remoting {

SecurityKeyIpcClient::SecurityKeyIpcClient()
    : SecurityKeyIpcClient(std::make_unique<ChromotingHostServicesClient>()) {}

SecurityKeyIpcClient::SecurityKeyIpcClient(
    std::unique_ptr<ChromotingHostServicesProvider> service_provider)
    : service_provider_(std::move(service_provider)) {}

SecurityKeyIpcClient::~SecurityKeyIpcClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool SecurityKeyIpcClient::CheckForSecurityKeyIpcServerChannel() {
  DCHECK(thread_checker_.CalledOnValidThread());

  return service_provider_->GetSessionServices() != nullptr;
}

void SecurityKeyIpcClient::EstablishIpcConnection(
    ConnectedCallback connected_callback,
    base::OnceClosure connection_error_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(connected_callback);
  DCHECK(connection_error_callback);
  DCHECK(!security_key_forwarder_.is_bound());

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

  if (!security_key_forwarder_.is_bound() ||
      !security_key_forwarder_.is_connected()) {
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
  HOST_LOG << "IPC connection closed.";
  security_key_forwarder_.reset();
}

void SecurityKeyIpcClient::OnQueryVersionResult(uint32_t unused_version) {
  DCHECK(thread_checker_.CalledOnValidThread());

  HOST_LOG << "IPC channel connected.";
  std::move(connected_callback_).Run();
}

void SecurityKeyIpcClient::OnChannelError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  LOG(ERROR) << "IPC channel error.";
  security_key_forwarder_.reset();
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

  if (!CheckForSecurityKeyIpcServerChannel()) {
    LOG(ERROR) << "Invalid channel handle.";
    OnChannelError();
    return;
  }
  service_provider_->GetSessionServices()->BindSecurityKeyForwarder(
      security_key_forwarder_.BindNewPipeAndPassReceiver());
  security_key_forwarder_.set_disconnect_handler(base::BindOnce(
      &SecurityKeyIpcClient::OnChannelError, base::Unretained(this)));
  // This is to determine if the peer binding is successful. If the connection
  // is disconnected before OnQueryVersionResult() is called, it means the
  // server has rejected the binding request.
  security_key_forwarder_.QueryVersion(base::BindOnce(
      &SecurityKeyIpcClient::OnQueryVersionResult, base::Unretained(this)));
}

}  // namespace remoting
