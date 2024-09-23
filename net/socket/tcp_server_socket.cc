// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_server_socket.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "net/base/net_errors.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/tcp_client_socket.h"

namespace net {

TCPServerSocket::TCPServerSocket(NetLog* net_log, const NetLogSource& source)
    : TCPServerSocket(
          TCPSocket::Create(nullptr /* socket_performance_watcher */,
                            net_log,
                            source)) {}

TCPServerSocket::TCPServerSocket(std::unique_ptr<TCPSocket> socket)
    : socket_(std::move(socket)) {}

int TCPServerSocket::AdoptSocket(SocketDescriptor socket) {
  adopted_opened_socket_ = true;
  return socket_->AdoptUnconnectedSocket(socket);
}

TCPServerSocket::~TCPServerSocket() = default;

int TCPServerSocket::Listen(const IPEndPoint& address,
                            int backlog,
                            std::optional<bool> ipv6_only) {
  int result = OK;
  if (!adopted_opened_socket_) {
    result = socket_->Open(address.GetFamily());
    if (result != OK) {
      return result;
    }
  }

  if (ipv6_only.has_value()) {
    CHECK_EQ(address.address(), net::IPAddress::IPv6AllZeros());
    result = socket_->SetIPv6Only(*ipv6_only);
    if (result != OK) {
      socket_->Close();
      return result;
    }
  }

  result = socket_->SetDefaultOptionsForServer();
  if (result != OK) {
    socket_->Close();
    return result;
  }

  result = socket_->Bind(address);
  if (result != OK) {
    socket_->Close();
    return result;
  }

  result = socket_->Listen(backlog);
  if (result != OK) {
    socket_->Close();
    return result;
  }

  return OK;
}

int TCPServerSocket::GetLocalAddress(IPEndPoint* address) const {
  return socket_->GetLocalAddress(address);
}

int TCPServerSocket::Accept(std::unique_ptr<StreamSocket>* socket,
                            CompletionOnceCallback callback) {
  return Accept(socket, std::move(callback), nullptr);
}

int TCPServerSocket::Accept(std::unique_ptr<StreamSocket>* socket,
                            CompletionOnceCallback callback,
                            IPEndPoint* peer_address) {
  DCHECK(socket);
  DCHECK(!callback.is_null());

  if (pending_accept_) {
    NOTREACHED_IN_MIGRATION();
    return ERR_UNEXPECTED;
  }

  // It is safe to use base::Unretained(this). |socket_| is owned by this class,
  // and the callback won't be run after |socket_| is destroyed.
  CompletionOnceCallback accept_callback = base::BindOnce(
      &TCPServerSocket::OnAcceptCompleted, base::Unretained(this), socket,
      peer_address, std::move(callback));
  int result = socket_->Accept(&accepted_socket_, &accepted_address_,
                               std::move(accept_callback));
  if (result != ERR_IO_PENDING) {
    // |accept_callback| won't be called so we need to run
    // ConvertAcceptedSocket() ourselves in order to do the conversion from
    // |accepted_socket_| to |socket|.
    result = ConvertAcceptedSocket(result, socket, peer_address);
  } else {
    pending_accept_ = true;
  }

  return result;
}

void TCPServerSocket::DetachFromThread() {
  socket_->DetachFromThread();
}

int TCPServerSocket::ConvertAcceptedSocket(
    int result,
    std::unique_ptr<StreamSocket>* output_accepted_socket,
    IPEndPoint* output_accepted_address) {
  // Make sure the TCPSocket object is destroyed in any case.
  std::unique_ptr<TCPSocket> temp_accepted_socket(std::move(accepted_socket_));
  if (result != OK)
    return result;

  if (output_accepted_address)
    *output_accepted_address = accepted_address_;

  *output_accepted_socket = std::make_unique<TCPClientSocket>(
      std::move(temp_accepted_socket), accepted_address_);

  return OK;
}

void TCPServerSocket::OnAcceptCompleted(
    std::unique_ptr<StreamSocket>* output_accepted_socket,
    IPEndPoint* output_accepted_address,
    CompletionOnceCallback forward_callback,
    int result) {
  result = ConvertAcceptedSocket(result, output_accepted_socket,
                                 output_accepted_address);
  pending_accept_ = false;
  std::move(forward_callback).Run(result);
}

}  // namespace net
