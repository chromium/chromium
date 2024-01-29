// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_resolving_socket_mojo.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "services/network/socket_data_pump.h"

namespace network {

ProxyResolvingSocketMojo::ProxyResolvingSocketMojo(
    std::unique_ptr<net::StreamSocket> socket,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingRemote<mojom::SocketObserver> observer,
    TLSSocketFactory* tls_socket_factory)
    : observer_(std::move(observer)),
      tls_socket_factory_(tls_socket_factory),
      socket_(std::move(socket)),
      traffic_annotation_(traffic_annotation) {}

ProxyResolvingSocketMojo::~ProxyResolvingSocketMojo() {
  if (connect_callback_) {
    // If |this| is destroyed when connect hasn't completed, tell the consumer
    // that request has been aborted.
    std::move(connect_callback_)
        .Run(net::ERR_ABORTED, std::nullopt, std::nullopt,
             mojo::ScopedDataPipeConsumerHandle(),
             mojo::ScopedDataPipeProducerHandle());
  }
}

void ProxyResolvingSocketMojo::Connect(
    mojom::ProxyResolvingSocketFactory::CreateProxyResolvingSocketCallback
        callback) {
  DCHECK(socket_);
  DCHECK(callback);
  DCHECK(!connect_callback_);

  connect_callback_ = std::move(callback);
  int result = socket_->Connect(base::BindOnce(
      &ProxyResolvingSocketMojo::OnConnectCompleted, base::Unretained(this)));
  if (result == net::ERR_IO_PENDING)
    return;
  OnConnectCompleted(result);
}

void ProxyResolvingSocketMojo::UpgradeToTLS(
    const net::HostPortPair& host_port_pair,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<mojom::TLSClientSocket> receiver,
    mojo::PendingRemote<mojom::SocketObserver> observer,
    mojom::ProxyResolvingSocket::UpgradeToTLSCallback callback) {
  // Wait for data pipes to be closed by the client before doing the upgrade.
  if (socket_data_pump_) {
    pending_upgrade_to_tls_callback_ = base::BindOnce(
        &ProxyResolvingSocketMojo::UpgradeToTLS, base::Unretained(this),
        host_port_pair, traffic_annotation, std::move(receiver),
        std::move(observer), std::move(callback));
    return;
  }
  tls_socket_factory_->UpgradeToTLS(
      this, host_port_pair, nullptr /* sockt_options */, traffic_annotation,
      std::move(receiver), std::move(observer),
      base::BindOnce(
          [](mojom::ProxyResolvingSocket::UpgradeToTLSCallback callback,
             int32_t net_error,
             mojo::ScopedDataPipeConsumerHandle receive_stream,
             mojo::ScopedDataPipeProducerHandle send_stream,
             const std::optional<net::SSLInfo>& ssl_info) {
            DCHECK(!ssl_info);
            std::move(callback).Run(net_error, std::move(receive_stream),
                                    std::move(send_stream));
          },
          std::move(callback)));
}

void ProxyResolvingSocketMojo::OnConnectCompleted(int result) {
  DCHECK(!connect_callback_.is_null());
  DCHECK(!socket_data_pump_);

  net::IPEndPoint local_addr;
  if (result == net::OK)
    result = socket_->GetLocalAddress(&local_addr);

  net::IPEndPoint peer_addr;
  // If |socket_| is connected through a proxy, GetPeerAddress returns
  // net::ERR_NAME_NOT_RESOLVED.
  bool get_peer_address_success =
      result == net::OK && (socket_->GetPeerAddress(&peer_addr) == net::OK);

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
      .Run(net::OK, local_addr,
           get_peer_address_success
               ? std::make_optional<net::IPEndPoint>(peer_addr)
               : std::nullopt,
           std::move(receive_consumer_handle), std::move(send_producer_handle));
}

void ProxyResolvingSocketMojo::OnNetworkReadError(int net_error) {
  if (observer_)
    observer_->OnReadError(net_error);
}

void ProxyResolvingSocketMojo::OnNetworkWriteError(int net_error) {
  if (observer_)
    observer_->OnWriteError(net_error);
}

void ProxyResolvingSocketMojo::OnShutdown() {
  socket_data_pump_ = nullptr;
  if (!pending_upgrade_to_tls_callback_.is_null())
    std::move(pending_upgrade_to_tls_callback_).Run();
}

const net::StreamSocket* ProxyResolvingSocketMojo::BorrowSocket() {
  return socket_.get();
}

std::unique_ptr<net::StreamSocket> ProxyResolvingSocketMojo::TakeSocket() {
  return std::move(socket_);
}

}  // namespace network
