// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_services_client.h"

#include "base/bind.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/isolated_connection.h"
#include "remoting/host/ipc_constants.h"
#include "remoting/host/mojom/chromoting_host_services.mojom.h"

namespace remoting {

ChromotingHostServicesClient::ChromotingHostServicesClient()
    : server_name_(GetChromotingHostServicesServerName()) {}

ChromotingHostServicesClient::~ChromotingHostServicesClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

mojom::ChromotingHostServices* ChromotingHostServicesClient::Get() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!const_cast<ChromotingHostServicesClient*>(this)->EnsureConnection()) {
    return nullptr;
  }

  return remote_.get();
}

bool ChromotingHostServicesClient::EnsureConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (remote_.is_bound()) {
    return true;
  }

  auto endpoint = mojo::NamedPlatformChannel::ConnectToServer(server_name_);
  if (!endpoint.is_valid()) {
    LOG(WARNING) << "Cannot connect to IPC through server name " << server_name_
                 << ". Endpoint is invalid.";
    return false;
  }
  connection_ = std::make_unique<mojo::IsolatedConnection>();
  mojo::PendingRemote<mojom::ChromotingHostServices> pending_remote(
      connection_->Connect(std::move(endpoint)), /* version= */ 0);
  if (!pending_remote.is_valid()) {
    LOG(WARNING) << "Invalid message pipe.";
    connection_.reset();
    return false;
  }
  remote_.Bind(std::move(pending_remote));
  remote_.set_disconnect_handler(base::BindOnce(
      &ChromotingHostServicesClient::OnDisconnected, base::Unretained(this)));
  return true;
}

void ChromotingHostServicesClient::OnDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  remote_.reset();
  connection_.reset();
}

}  // namespace remoting
