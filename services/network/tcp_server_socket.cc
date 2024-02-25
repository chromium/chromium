// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/tcp_server_socket.h"

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
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

base::expected<net::IPEndPoint, int32_t> TCPServerSocket::Listen(
    const net::IPEndPoint& local_addr,
    int backlog,
    std::optional<bool> ipv6_only) {
  if (backlog == 0) {
    // SocketPosix::Listen and TCPSocketWin::Listen DCHECKs on backlog > 0.
    return base::unexpected(net::ERR_INVALID_ARGUMENT);
  }
  backlog_ = backlog;
  int net_error = socket_->Listen(local_addr, backlog, ipv6_only);
  net::IPEndPoint local_addr_out;
  if (net_error == net::OK) {
    net_error = socket_->GetLocalAddress(&local_addr_out);
  }
  if (net_error == net::OK) {
    return local_addr_out;
  }
  return base::unexpected(net_error);
}

void TCPServerSocket::Accept(
    mojo::PendingRemote<mojom::SocketObserver> observer,
    AcceptCallback callback) {
  if (pending_accepts_queue_.size() >= static_cast<size_t>(backlog_)) {
    std::move(callback).Run(net::ERR_INSUFFICIENT_RESOURCES, std::nullopt,
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

#if BUILDFLAG(IS_CHROMEOS)
void TCPServerSocket::AttachConnectionTracker(
    mojo::PendingRemote<mojom::SocketConnectionTracker> connection_tracker) {
  connection_tracker_ = std::move(connection_tracker);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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

  mojo::ScopedDataPipeProducerHandle send_producer_handle;
  mojo::ScopedDataPipeConsumerHandle send_consumer_handle;
  if (result == net::OK) {
    DCHECK(accepted_socket_);
    if (mojo::CreateDataPipe(nullptr, send_producer_handle,
                             send_consumer_handle) != MOJO_RESULT_OK) {
      result = net::ERR_FAILED;
    }
  }

  mojo::ScopedDataPipeProducerHandle receive_producer_handle;
  mojo::ScopedDataPipeConsumerHandle receive_consumer_handle;
  if (result == net::OK) {
    if (mojo::CreateDataPipe(nullptr, receive_producer_handle,
                             receive_consumer_handle) != MOJO_RESULT_OK) {
      result = net::ERR_FAILED;
    }
  }

  if (result == net::OK) {
    mojo::PendingRemote<mojom::TCPConnectedSocket> socket;
    auto connected_socket = std::make_unique<TCPConnectedSocket>(
        std::move(pending_accept->observer),
        base::WrapUnique(static_cast<net::TransportClientSocket*>(
            accepted_socket_.release())),
        std::move(receive_producer_handle), std::move(send_consumer_handle),
        traffic_annotation_);
    delegate_->OnAccept(std::move(connected_socket),
                        socket.InitWithNewPipeAndPassReceiver());
    std::move(pending_accept->callback)
        .Run(result, accepted_address_, std::move(socket),
             std::move(receive_consumer_handle),
             std::move(send_producer_handle));
  } else {
    std::move(pending_accept->callback)
        .Run(result, std::nullopt, mojo::NullRemote(),
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
                      base::BindOnce(&TCPServerSocket::OnAcceptCompleted,
                                     base::Unretained(this)),
                      &accepted_address_);
  if (result == net::ERR_IO_PENDING)
    return;
  OnAcceptCompleted(result);
}

}  // namespace network
