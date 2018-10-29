// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/tcp_bound_socket.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "net/socket/tcp_socket.h"
#include "services/network/socket_factory.h"
#include "services/network/tcp_connected_socket.h"

namespace network {

TCPBoundSocket::TCPBoundSocket(
    SocketFactory* socket_factory,
    net::NetLog* net_log,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : socket_factory_(socket_factory),
      socket_(std::make_unique<net::TCPSocket>(
          nullptr /*socket_performance_watcher*/,
          net_log,
          net::NetLogSource())),
      traffic_annotation_(traffic_annotation),
      weak_factory_(this) {}

TCPBoundSocket::~TCPBoundSocket() = default;

int TCPBoundSocket::Bind(const net::IPEndPoint& local_addr,
                         net::IPEndPoint* local_addr_out) {
  bind_address_ = local_addr;

  int result = socket_->Open(local_addr.GetFamily());
  if (result != net::OK)
    return result;

  // This is primarily intended for use with server sockets.
  result = socket_->SetDefaultOptionsForServer();
  if (result != net::OK)
    return result;

  result = socket_->Bind(local_addr);
  if (result != net::OK)
    return result;

  return socket_->GetLocalAddress(local_addr_out);
}

void TCPBoundSocket::Listen(uint32_t backlog,
                            mojom::TCPServerSocketRequest request,
                            ListenCallback callback) {
  DCHECK(socket_->IsValid());

  if (!socket_) {
    // Drop unexpected calls on the floor. Could destroy |this|, but as this is
    // currently only reachable from more trusted processes, doesn't seem too
    // useful.
    NOTREACHED();
    return;
  }

  int result = ListenInternal(backlog);

  // Succeed or fail, pass the result back to the caller.
  std::move(callback).Run(result);

  // Tear down everything on error.
  if (result != net::OK) {
    socket_factory_->DestroyBoundSocket(binding_id_);
    return;
  }

  // On success, also set up the TCPServerSocket.
  std::unique_ptr<TCPServerSocket> server_socket =
      std::make_unique<TCPServerSocket>(
          std::make_unique<net::TCPServerSocket>(std::move(socket_)), backlog,
          socket_factory_, traffic_annotation_);
  socket_factory_->OnBoundSocketListening(binding_id_, std::move(server_socket),
                                          std::move(request));
  // The above call will have destroyed |this|.
}

void TCPBoundSocket::Connect(
    const net::AddressList& remote_addr_list,
    mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
    mojom::TCPConnectedSocketRequest request,
    mojom::SocketObserverPtr observer,
    ConnectCallback callback) {
  DCHECK(socket_->IsValid());

  if (!socket_) {
    // Drop unexpected calls on the floor. Could destroy |this|, but as this is
    // currently only reachable from more trusted processes, doesn't seem too
    // useful.
    NOTREACHED();
    return;
  }

  DCHECK(!connect_callback_);
  DCHECK(!connected_socket_request_);
  DCHECK(!connecting_socket_);

  connected_socket_request_ = std::move(request);
  connect_callback_ = std::move(callback);

  // Create a TCPConnectedSocket and have it do the work of connecting and
  // configuring the socket. This saves a bit of code, and reduces the number of
  // tests this class needs, since it can rely on TCPConnectedSocket's tests for
  // a lot of cases.
  connecting_socket_ = std::make_unique<TCPConnectedSocket>(
      std::move(observer), socket_->net_log().net_log(),
      socket_factory_->tls_socket_factory(),
      nullptr /* client_socket_factory */, traffic_annotation_);
  connecting_socket_->ConnectWithSocket(
      net::TCPClientSocket::CreateFromBoundSocket(
          std::move(socket_), remote_addr_list, bind_address_),
      std::move(tcp_connected_socket_options),
      base::BindOnce(&TCPBoundSocket::OnConnectComplete,
                     base::Unretained(this)));
}

void TCPBoundSocket::OnConnectComplete(
    int result,
    const base::Optional<net::IPEndPoint>& local_addr,
    const base::Optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK(connecting_socket_);
  DCHECK(connect_callback_);

  std::move(connect_callback_)
      .Run(result, local_addr, peer_addr, std::move(receive_stream),
           std::move(send_stream));
  if (result != net::OK) {
    socket_factory_->DestroyBoundSocket(binding_id_);
    // The above call will have destroyed |this|.
    return;
  }

  socket_factory_->OnBoundSocketConnected(binding_id_,
                                          std::move(connecting_socket_),
                                          std::move(connected_socket_request_));
  // The above call will have destroyed |this|.
}

int TCPBoundSocket::ListenInternal(int backlog) {
  return socket_->Listen(backlog);
}

}  // namespace network
