// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/tls_client_socket.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"

namespace network {

TLSClientSocket::TLSClientSocket(
    mojo::PendingRemote<mojom::SocketObserver> observer,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : observer_(std::move(observer)), traffic_annotation_(traffic_annotation) {}

TLSClientSocket::~TLSClientSocket() {}

void TLSClientSocket::Connect(
    const net::HostPortPair& host_port_pair,
    const net::SSLConfig& ssl_config,
    std::unique_ptr<net::StreamSocket> tcp_socket,
    net::SSLClientContext* ssl_client_context,
    net::ClientSocketFactory* socket_factory,
    mojom::TCPConnectedSocket::UpgradeToTLSCallback callback,
    bool send_ssl_info) {
  connect_callback_ = std::move(callback);
  send_ssl_info_ = send_ssl_info;
  socket_ = socket_factory->CreateSSLClientSocket(
      ssl_client_context, std::move(tcp_socket), host_port_pair, ssl_config);
  int result = socket_->Connect(base::BindRepeating(
      &TLSClientSocket::OnTLSConnectCompleted, base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    OnTLSConnectCompleted(result);
}

void TLSClientSocket::OnTLSConnectCompleted(int result) {
  DCHECK(!connect_callback_.is_null());

  if (result != net::OK) {
    socket_ = nullptr;
    std::move(connect_callback_)
        .Run(result, mojo::ScopedDataPipeConsumerHandle(),
             mojo::ScopedDataPipeProducerHandle(), base::nullopt);
    return;
  }
  mojo::DataPipe send_pipe;
  mojo::DataPipe receive_pipe;
  socket_data_pump_ = std::make_unique<SocketDataPump>(
      socket_.get(), this /*delegate*/, std::move(receive_pipe.producer_handle),
      std::move(send_pipe.consumer_handle), traffic_annotation_);
  base::Optional<net::SSLInfo> ssl_info;
  if (send_ssl_info_) {
    net::SSLInfo local;
    socket_->GetSSLInfo(&local);
    ssl_info = std::move(local);
  }
  std::move(connect_callback_)
      .Run(net::OK, std::move(receive_pipe.consumer_handle),
           std::move(send_pipe.producer_handle), std::move(ssl_info));
}

void TLSClientSocket::OnNetworkReadError(int net_error) {
  if (observer_)
    observer_->OnReadError(net_error);
}

void TLSClientSocket::OnNetworkWriteError(int net_error) {
  if (observer_)
    observer_->OnWriteError(net_error);
}

void TLSClientSocket::OnShutdown() {
  // Do nothing.
}

}  // namespace network
