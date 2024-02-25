// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/socket/tls_socket.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/socket/mojo_data_pump.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "url/url_canon.h"

namespace extensions {

const char kTLSSocketTypeInvalidError[] =
    "Cannot listen on a socket that is already connected.";

TLSSocket::TLSSocket(
    mojo::PendingRemote<network::mojom::TLSClientSocket> tls_socket,
    const net::IPEndPoint& local_addr,
    const net::IPEndPoint& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream,
    const std::string& owner_extension_id)
    : ResumableTCPSocket(nullptr, owner_extension_id),
      tls_socket_(std::move(tls_socket)),
      local_addr_(local_addr),
      peer_addr_(peer_addr),
      mojo_data_pump_(std::make_unique<MojoDataPump>(std::move(receive_stream),
                                                     std::move(send_stream))) {
  DCHECK(tls_socket_);
  is_connected_ = true;
}

TLSSocket::~TLSSocket() {
  Disconnect(true /* socket_destroying */);
}

void TLSSocket::Connect(const net::AddressList& address,
                        net::CompletionOnceCallback callback) {
  std::move(callback).Run(net::ERR_CONNECTION_FAILED);
}

void TLSSocket::Disconnect(bool socket_destroying) {
  is_connected_ = false;
  tls_socket_.reset();
  local_addr_ = std::nullopt;
  peer_addr_ = std::nullopt;
  mojo_data_pump_ = nullptr;
  // TODO(devlin): Should we do this for all callbacks?
  if (read_callback_) {
    std::move(read_callback_)
        .Run(net::ERR_CONNECTION_CLOSED, nullptr, socket_destroying);
  }
}

void TLSSocket::Read(int count, ReadCompletionCallback callback) {
  DCHECK(callback);
  DCHECK(!read_callback_);

  const bool socket_destroying = false;
  if (!mojo_data_pump_) {
    std::move(callback).Run(net::ERR_SOCKET_NOT_CONNECTED, nullptr,
                            socket_destroying);
    return;
  }
  if (mojo_data_pump_->HasPendingRead()) {
    std::move(callback).Run(net::ERR_IO_PENDING, nullptr, socket_destroying);
    return;
  }
  read_callback_ = std::move(callback);
  mojo_data_pump_->Read(count, base::BindOnce(&TLSSocket::OnReadComplete,
                                              base::Unretained(this)));
}

void TLSSocket::Listen(const std::string& address,
                       uint16_t port,
                       int backlog,
                       ListenCallback callback) {
  std::move(callback).Run(net::ERR_NOT_IMPLEMENTED, kTLSSocketTypeInvalidError);
}

bool TLSSocket::IsConnected() {
  return is_connected_;
}

bool TLSSocket::GetPeerAddress(net::IPEndPoint* address) {
  if (!IsConnected())
    return false;
  if (!peer_addr_)
    return false;
  *address = peer_addr_.value();
  return true;
}

bool TLSSocket::GetLocalAddress(net::IPEndPoint* address) {
  if (!IsConnected())
    return false;
  if (!local_addr_)
    return false;
  *address = local_addr_.value();
  return true;
}

Socket::SocketType TLSSocket::GetSocketType() const {
  return Socket::TYPE_TLS;
}

int TLSSocket::WriteImpl(net::IOBuffer* io_buffer,
                         int io_buffer_size,
                         net::CompletionOnceCallback callback) {
  if (!mojo_data_pump_)
    return net::ERR_SOCKET_NOT_CONNECTED;
  mojo_data_pump_->Write(
      io_buffer, io_buffer_size,
      base::BindOnce(&TLSSocket::OnWriteComplete, base::Unretained(this),
                     std::move(callback)));
  return net::ERR_IO_PENDING;
}

void TLSSocket::OnWriteComplete(net::CompletionOnceCallback callback,
                                int result) {
  if (result < 0) {
    // Write side has terminated. This can be an error or a graceful close.
    // TCPSocketEventDispatcher doesn't distinguish between the two.
    Disconnect(false /* socket_destroying */);
  }
  std::move(callback).Run(result);
}

void TLSSocket::OnReadComplete(int result,
                               scoped_refptr<net::IOBuffer> io_buffer) {
  DCHECK(read_callback_);
  DCHECK_GE(result, 0);

  // Use a local variable for |read_callback_|, because otherwise Disconnect()
  // will try to invoke |read_callback_| with a hardcoded result code.
  ReadCompletionCallback callback = std::move(read_callback_);
  if (result == 0) {
    // Read side has terminated. This can be an error or a graceful close.
    // TCPSocketEventDispatcher doesn't distinguish between the two. Treat them
    // as connection close.
    Disconnect(false /* socket_destroying */);
  }
  std::move(callback).Run(result, io_buffer, false /* socket_destroying */);
}

}  // namespace extensions
