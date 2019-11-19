// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/tcp_server_socket.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/socket/tcp_server_socket.h"
#include "services/network/tcp_connected_socket.h"

namespace network {

TCPServerSocket::TCPServerSocket(
    Delegate* delegate,
    net::NetLog* net_log,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : TCPServerSocket(
          std::make_unique<net::TCPServerSocket>(net_log, net::NetLogSource()),
          0 /*backlog*/,
          delegate,
          traffic_annotation) {}

TCPServerSocket::TCPServerSocket(
    std::unique_ptr<net::ServerSocket> server_socket,
    int backlog,
    Delegate* delegate,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : delegate_(delegate),
      socket_(std::move(server_socket)),
      backlog_(backlog),
      traffic_annotation_(traffic_annotation) {}

TCPServerSocket::~TCPServerSocket() {}

int TCPServerSocket::Listen(const net::IPEndPoint& local_addr,
                            int backlog,
                            net::IPEndPoint* local_addr_out) {
  if (backlog == 0) {
    // SocketPosix::Listen and TCPSocketWin::Listen DCHECKs on backlog > 0.
    return net::ERR_INVALID_ARGUMENT;
  }
  backlog_ = backlog;
  int net_error = socket_->Listen(local_addr, backlog);
  if (net_error == net::OK)
    net_error = socket_->GetLocalAddress(local_addr_out);
  return net_error;
}

void TCPServerSocket::Accept(
    mojo::PendingRemote<mojom::SocketObserver> observer,
    AcceptCallback callback) {
  if (pending_accepts_queue_.size() >= static_cast<size_t>(backlog_)) {
    std::move(callback).Run(net::ERR_INSUFFICIENT_RESOURCES, base::nullopt,
                            mojo::NullRemote(),
                            mojo::ScopedDataPipeConsumerHandle(),
                            mojo::ScopedDataPipeProducerHandle());
    return;
  }

  pending_accepts_queue_.push_back(std::make_unique<PendingAccept>(
      std::move(callback), std::move(observer)));
  if (pending_accepts_queue_.size() == 1)
    ProcessNextAccept();
}

void TCPServerSocket::SetSocketForTest(
    std::unique_ptr<net::ServerSocket> socket) {
  socket_ = std::move(socket);
}

TCPServerSocket::PendingAccept::PendingAccept(
    AcceptCallback callback,
    mojo::PendingRemote<mojom::SocketObserver> observer)
    : callback(std::move(callback)), observer(std::move(observer)) {}

TCPServerSocket::PendingAccept::~PendingAccept() {}

void TCPServerSocket::OnAcceptCompleted(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  DCHECK(!pending_accepts_queue_.empty());

  auto pending_accept = std::move(pending_accepts_queue_.front());
  pending_accepts_queue_.erase(pending_accepts_queue_.begin());

  net::IPEndPoint peer_addr;
  if (result == net::OK) {
    DCHECK(accepted_socket_);
    result = accepted_socket_->GetPeerAddress(&peer_addr);
  }
  if (result == net::OK) {
    mojo::DataPipe send_pipe;
    mojo::DataPipe receive_pipe;
    mojo::PendingRemote<mojom::TCPConnectedSocket> socket;
    auto connected_socket = std::make_unique<TCPConnectedSocket>(
        std::move(pending_accept->observer),
        base::WrapUnique(static_cast<net::TransportClientSocket*>(
            accepted_socket_.release())),
        std::move(receive_pipe.producer_handle),
        std::move(send_pipe.consumer_handle), traffic_annotation_);
    delegate_->OnAccept(std::move(connected_socket),
                        socket.InitWithNewPipeAndPassReceiver());
    std::move(pending_accept->callback)
        .Run(result, peer_addr, std::move(socket),
             std::move(receive_pipe.consumer_handle),
             std::move(send_pipe.producer_handle));
  } else {
    std::move(pending_accept->callback)
        .Run(result, base::nullopt, mojo::NullRemote(),
             mojo::ScopedDataPipeConsumerHandle(),
             mojo::ScopedDataPipeProducerHandle());
  }
  ProcessNextAccept();
}

void TCPServerSocket::ProcessNextAccept() {
  if (pending_accepts_queue_.empty())
    return;
  int result =
      socket_->Accept(&accepted_socket_,
                      base::BindRepeating(&TCPServerSocket::OnAcceptCompleted,
                                          base::Unretained(this)));
  if (result == net::ERR_IO_PENDING)
    return;
  OnAcceptCompleted(result);
}

}  // namespace network
