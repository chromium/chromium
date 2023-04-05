// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/fake_security_key_ipc_client.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

FakeSecurityKeyIpcClient::FakeSecurityKeyIpcClient(
    const base::RepeatingClosure& connection_event_callback)
    : SecurityKeyIpcClient(/* service_provider */ nullptr),
      connection_event_callback_(connection_event_callback) {
  DCHECK(!connection_event_callback_.is_null());
}

FakeSecurityKeyIpcClient::~FakeSecurityKeyIpcClient() = default;

base::WeakPtr<FakeSecurityKeyIpcClient> FakeSecurityKeyIpcClient::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool FakeSecurityKeyIpcClient::CheckForSecurityKeyIpcServerChannel() {
  return check_for_ipc_channel_return_value_;
}

void FakeSecurityKeyIpcClient::EstablishIpcConnection(
    ConnectedCallback connected_callback,
    base::OnceClosure connection_error_callback) {
  if (establish_ipc_connection_should_succeed_) {
    std::move(connected_callback).Run();
  } else {
    std::move(connection_error_callback).Run();
  }
}

bool FakeSecurityKeyIpcClient::SendSecurityKeyRequest(
    const std::string& request_payload,
    ResponseCallback response_callback) {
  if (send_security_request_should_succeed_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(response_callback, security_key_response_payload_));
  }

  return send_security_request_should_succeed_;
}

void FakeSecurityKeyIpcClient::CloseIpcConnection() {
  ipc_connected_ = false;
  security_key_forwarder_.reset();
  connection_event_callback_.Run();
}

mojo::PendingReceiver<mojom::SecurityKeyForwarder>
FakeSecurityKeyIpcClient::BindNewPipeAndPassReceiver() {
  auto pending_receiver = security_key_forwarder_.BindNewPipeAndPassReceiver();
  security_key_forwarder_.set_disconnect_handler(base::BindOnce(
      &FakeSecurityKeyIpcClient::CloseIpcConnection, base::Unretained(this)));
  // This is to determine if the peer binding is successful. If the connection
  // is disconnected before OnQueryVersionResult() is called, it means the
  // server has rejected the binding request.
  security_key_forwarder_.QueryVersion(base::BindOnce(
      &FakeSecurityKeyIpcClient::OnQueryVersionResult, base::Unretained(this)));
  return pending_receiver;
}

void FakeSecurityKeyIpcClient::SendSecurityKeyRequestViaIpc(
    const std::string& request_payload) {
  security_key_forwarder_->OnSecurityKeyRequest(
      request_payload,
      base::BindOnce(&FakeSecurityKeyIpcClient::OnSecurityKeyResponse,
                     base::Unretained(this)));
}

void FakeSecurityKeyIpcClient::OnQueryVersionResult(uint32_t unused_version) {
  ipc_connected_ = true;
  connection_ready_ = true;
  connection_event_callback_.Run();
}

void FakeSecurityKeyIpcClient::OnSecurityKeyResponse(
    const std::string& request_data) {
  last_message_received_ = request_data;
  connection_event_callback_.Run();
}

}  // namespace remoting
