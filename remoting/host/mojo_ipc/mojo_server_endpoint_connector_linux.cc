// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojo_ipc/mojo_server_endpoint_connector_linux.h"

#include <sys/socket.h>

#include <memory>

#include "base/check.h"
#include "base/logging.h"
#include "base/task/current_thread.h"
#include "mojo/public/cpp/platform/socket_utils_posix.h"

namespace remoting {

MojoServerEndpointConnectorLinux::MojoServerEndpointConnectorLinux(
    Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

MojoServerEndpointConnectorLinux::~MojoServerEndpointConnectorLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoServerEndpointConnectorLinux::Connect(
    mojo::PlatformChannelServerEndpoint server_endpoint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(server_endpoint.is_valid());
  DCHECK(!pending_server_endpoint_.is_valid());

  read_watcher_ =
      std::make_unique<base::MessagePumpForIO::FdWatchController>(FROM_HERE);
  pending_server_endpoint_ = std::move(server_endpoint);
  base::CurrentIOThread::Get()->WatchFileDescriptor(
      pending_server_endpoint_.platform_handle().GetFD().get(), false,
      base::MessagePumpForIO::WATCH_READ, read_watcher_.get(), this);
}

void MojoServerEndpointConnectorLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(pending_server_endpoint_.platform_handle().GetFD().get(), fd);

  base::ScopedFD socket;
  bool success = mojo::AcceptSocketConnection(fd, &socket);
  read_watcher_.reset();
  pending_server_endpoint_.reset();
  if (!success) {
    LOG(ERROR) << "AcceptSocketConnection failed.";
    delegate_->OnServerEndpointConnectionFailed();
    return;
  }
  if (!socket.is_valid()) {
    LOG(ERROR) << "Socket is invalid.";
    delegate_->OnServerEndpointConnectionFailed();
    return;
  }

  ucred unix_peer_identity;
  socklen_t len = sizeof(unix_peer_identity);
  if (getsockopt(socket.get(), SOL_SOCKET, SO_PEERCRED, &unix_peer_identity,
                 &len) != 0) {
    PLOG(ERROR) << "getsockopt failed.";
    delegate_->OnServerEndpointConnectionFailed();
    return;
  }

  mojo::PlatformChannelEndpoint endpoint(
      mojo::PlatformHandle(std::move(socket)));
  if (!endpoint.is_valid()) {
    LOG(ERROR) << "Endpoint is invalid.";
    delegate_->OnServerEndpointConnectionFailed();
    return;
  }
  auto connection = std::make_unique<mojo::IsolatedConnection>();
  auto message_pipe = connection->Connect(std::move(endpoint));
  delegate_->OnServerEndpointConnected(
      std::move(connection), std::move(message_pipe), unix_peer_identity.pid);
}

void MojoServerEndpointConnectorLinux::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

// static
std::unique_ptr<MojoServerEndpointConnector>
MojoServerEndpointConnector::Create(Delegate* delegate) {
  return std::make_unique<MojoServerEndpointConnectorLinux>(delegate);
}

}  // namespace remoting
