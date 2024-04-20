// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/tcp_connected_socket.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/tls_client_socket.h"

namespace network {

namespace {

int ClampTCPBufferSize(int requested_buffer_size) {
  return std::clamp(requested_buffer_size, 0,
                    TCPConnectedSocket::kMaxBufferSize);
}

// Sets the initial options on a fresh socket. Assumes |socket| is currently
// configured using the default client socket options
// (TCPSocket::SetDefaultOptionsForClient()).
int ConfigureSocket(
    net::TransportClientSocket* socket,
    const mojom::TCPConnectedSocketOptions* tcp_connected_socket_options) {
  int send_buffer_size =
      ClampTCPBufferSize(tcp_connected_socket_options->send_buffer_size);
  if (send_buffer_size > 0) {
    int result = socket->SetSendBufferSize(send_buffer_size);
    DCHECK_NE(net::ERR_IO_PENDING, result);
    if (result != net::OK)
      return result;
  }

  int receive_buffer_size =
      ClampTCPBufferSize(tcp_connected_socket_options->receive_buffer_size);
  if (receive_buffer_size > 0) {
    int result = socket->SetReceiveBufferSize(receive_buffer_size);
    DCHECK_NE(net::ERR_IO_PENDING, result);
    if (result != net::OK)
      return result;
  }

  // No delay is set by default, so only update the setting if it's false.
  if (!tcp_connected_socket_options->no_delay) {
    // Unlike the above calls, TcpSocket::SetNoDelay() returns a bool rather
    // than a network error code.
    if (!socket->SetNoDelay(false))
      return net::ERR_FAILED;
  }

  const mojom::TCPKeepAliveOptionsPtr& keep_alive_options =
      tcp_connected_socket_options->keep_alive_options;
  if (keep_alive_options) {
    // TcpSocket::SetKeepAlive(...) returns a bool rather than a network error
    // code.
    if (!socket->SetKeepAlive(/*enable=*/keep_alive_options->enable,
                              /*delay_secs=*/keep_alive_options->delay)) {
      return net::ERR_FAILED;
    }
  }

  return net::OK;
}

}  // namespace

const int TCPConnectedSocket::kMaxBufferSize = 128 * 1024;

TCPConnectedSocket::TCPConnectedSocket(
    mojo::PendingRemote<mojom::SocketObserver> observer,
    net::NetLog* net_log,
    TLSSocketFactory* tls_socket_factory,
    net::ClientSocketFactory* client_socket_factory,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : observer_(std::move(observer)),
      net_log_(net_log),
      client_socket_factory_(client_socket_factory),
      tls_socket_factory_(tls_socket_factory),
      traffic_annotation_(traffic_annotation) {}

TCPConnectedSocket::TCPConnectedSocket(
    mojo::PendingRemote<mojom::SocketObserver> observer,
    std::unique_ptr<net::TransportClientSocket> socket,
    mojo::ScopedDataPipeProducerHandle receive_pipe_handle,
    mojo::ScopedDataPipeConsumerHandle send_pipe_handle,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : observer_(std::move(observer)),
      net_log_(nullptr),
      client_socket_factory_(nullptr),
      tls_socket_factory_(nullptr),
      socket_(std::move(socket)),
      traffic_annotation_(traffic_annotation) {
  socket_data_pump_ = std::make_unique<SocketDataPump>(
      socket_.get(), this /*delegate*/, std::move(receive_pipe_handle),
      std::move(send_pipe_handle), traffic_annotation);
}

TCPConnectedSocket::~TCPConnectedSocket() {
  if (connect_callback_) {
    // If |this| is destroyed when connect hasn't completed, tell the consumer
    // that request has been aborted.
    std::move(connect_callback_)
        .Run(net::ERR_ABORTED, std::nullopt, std::nullopt,
             mojo::ScopedDataPipeConsumerHandle(),
             mojo::ScopedDataPipeProducerHandle());
  }
}

void TCPConnectedSocket::Connect(
    const std::optional<net::IPEndPoint>& local_addr,
    const net::AddressList& remote_addr_list,
    mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
    mojom::NetworkContext::CreateTCPConnectedSocketCallback callback) {
  DCHECK(!socket_);
  DCHECK(callback);

  // TODO(crbug.com/40146880): Pass a non-null NetworkQualityEstimator.
  net::NetworkQualityEstimator* network_quality_estimator = nullptr;

  std::unique_ptr<net::TransportClientSocket> socket =
      client_socket_factory_->CreateTransportClientSocket(
          remote_addr_list, nullptr /*socket_performance_watcher*/,
          network_quality_estimator, net_log_, net::NetLogSource());

  if (local_addr) {
    int result = socket->Bind(local_addr.value());
    if (result != net::OK) {
      OnConnectCompleted(result);
      return;
    }
  }

  return ConnectWithSocket(std::move(socket),
                           std::move(tcp_connected_socket_options),
                           std::move(callback));
}

void TCPConnectedSocket::ConnectWithSocket(
    std::unique_ptr<net::TransportClientSocket> socket,
    mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
    mojom::NetworkContext::CreateTCPConnectedSocketCallback callback) {
  socket_ = std::move(socket);
  connect_callback_ = std::move(callback);

  if (tcp_connected_socket_options) {
    socket_options_ = std::move(tcp_connected_socket_options);
    socket_->SetBeforeConnectCallback(base::BindRepeating(
        &ConfigureSocket, socket_.get(), socket_options_.get()));
  }
  int result = socket_->Connect(base::BindOnce(
      &TCPConnectedSocket::OnConnectCompleted, base::Unretained(this)));

  if (result == net::ERR_IO_PENDING)
    return;

  OnConnectCompleted(result);
}

void TCPConnectedSocket::UpgradeToTLS(
    const net::HostPortPair& host_port_pair,
    mojom::TLSClientSocketOptionsPtr socket_options,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<mojom::TLSClientSocket> receiver,
    mojo::PendingRemote<mojom::SocketObserver> observer,
    mojom::TCPConnectedSocket::UpgradeToTLSCallback callback) {
  if (!tls_socket_factory_) {
    std::move(callback).Run(
        net::ERR_NOT_IMPLEMENTED, mojo::ScopedDataPipeConsumerHandle(),
        mojo::ScopedDataPipeProducerHandle(), std::nullopt /* ssl_info*/);
    return;
  }
  // Wait for data pipes to be closed by the client before doing the upgrade.
  if (socket_data_pump_) {
    pending_upgrade_to_tls_callback_ = base::BindOnce(
        &TCPConnectedSocket::UpgradeToTLS, base::Unretained(this),
        host_port_pair, std::move(socket_options), traffic_annotation,
        std::move(receiver), std::move(observer), std::move(callback));
    return;
  }
  tls_socket_factory_->UpgradeToTLS(
      this, host_port_pair, std::move(socket_options), traffic_annotation,
      std::move(receiver), std::move(observer), std::move(callback));
}

void TCPConnectedSocket::SetSendBufferSize(int send_buffer_size,
                                           SetSendBufferSizeCallback callback) {
  if (!socket_) {
    // Fail is this method was called after upgrading to TLS.
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }
  int result = socket_->SetSendBufferSize(ClampTCPBufferSize(send_buffer_size));
  std::move(callback).Run(result);
}

void TCPConnectedSocket::SetReceiveBufferSize(
    int send_buffer_size,
    SetSendBufferSizeCallback callback) {
  if (!socket_) {
    // Fail is this method was called after upgrading to TLS.
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }
  int result =
      socket_->SetReceiveBufferSize(ClampTCPBufferSize(send_buffer_size));
  std::move(callback).Run(result);
}

void TCPConnectedSocket::SetNoDelay(bool no_delay,
                                    SetNoDelayCallback callback) {
  if (!socket_) {
    std::move(callback).Run(false);
    return;
  }
  bool success = socket_->SetNoDelay(no_delay);
  std::move(callback).Run(success);
}

void TCPConnectedSocket::SetKeepAlive(bool enable,
                                      int32_t delay_secs,
                                      SetKeepAliveCallback callback) {
  if (!socket_) {
    std::move(callback).Run(false);
    return;
  }
  bool success = socket_->SetKeepAlive(enable, delay_secs);
  std::move(callback).Run(success);
}

void TCPConnectedSocket::OnConnectCompleted(int result) {
  DCHECK(!connect_callback_.is_null());
  DCHECK(!socket_data_pump_);

  net::IPEndPoint peer_addr, local_addr;
  if (result == net::OK)
    result = socket_->GetLocalAddress(&local_addr);
  if (result == net::OK)
    result = socket_->GetPeerAddress(&peer_addr);

  mojo::ScopedDataPipeProducerHandle send_producer_handle;
  mojo::ScopedDataPipeConsumerHandle send_consumer_handle;
  if (result == net::OK) {
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

  if (result != net::OK) {
    std::move(connect_callback_)
        .Run(result, std::nullopt, std::nullopt,
             mojo::ScopedDataPipeConsumerHandle(),
             mojo::ScopedDataPipeProducerHandle());
    return;
  }
  socket_data_pump_ = std::make_unique<SocketDataPump>(
      socket_.get(), this /*delegate*/, std::move(receive_producer_handle),
      std::move(send_consumer_handle), traffic_annotation_);
  std::move(connect_callback_)
      .Run(net::OK, local_addr, peer_addr, std::move(receive_consumer_handle),
           std::move(send_producer_handle));
}

void TCPConnectedSocket::OnNetworkReadError(int net_error) {
  if (observer_)
    observer_->OnReadError(net_error);
}

void TCPConnectedSocket::OnNetworkWriteError(int net_error) {
  if (observer_)
    observer_->OnWriteError(net_error);
}

void TCPConnectedSocket::OnShutdown() {
  socket_data_pump_ = nullptr;
  if (!pending_upgrade_to_tls_callback_.is_null())
    std::move(pending_upgrade_to_tls_callback_).Run();
}

const net::StreamSocket* TCPConnectedSocket::BorrowSocket() {
  return socket_.get();
}

std::unique_ptr<net::StreamSocket> TCPConnectedSocket::TakeSocket() {
  return std::move(socket_);
}

}  // namespace network
