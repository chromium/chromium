// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/socket_factory.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "mojo/public/cpp/bindings/type_converter.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/tcp_server_socket.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/simple_host_resolver.h"
#include "services/network/restricted_udp_socket.h"
#include "services/network/tls_client_socket.h"
#include "services/network/udp_socket.h"

namespace network {

SocketFactory::SocketFactory(net::NetLog* net_log,
                             net::URLRequestContext* url_request_context)
    : net_log_(net_log),
      client_socket_factory_(nullptr),
      tls_socket_factory_(url_request_context) {
  if (url_request_context->GetNetworkSessionContext()) {
    client_socket_factory_ =
        url_request_context->GetNetworkSessionContext()->client_socket_factory;
  }
  if (!client_socket_factory_) {
    client_socket_factory_ = net::ClientSocketFactory::GetDefaultFactory();
  }
}

SocketFactory::~SocketFactory() = default;

void SocketFactory::CreateUDPSocket(
    mojo::PendingReceiver<mojom::UDPSocket> receiver,
    mojo::PendingRemote<mojom::UDPSocketListener> listener) {
  udp_socket_receivers_.Add(
      std::make_unique<UDPSocket>(std::move(listener), net_log_),
      std::move(receiver));
}

void SocketFactory::CreateRestrictedUDPSocket(
    const net::IPEndPoint& addr,
    mojom::RestrictedUDPSocketMode mode,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojom::RestrictedUDPSocketParamsPtr params,
    mojo::PendingReceiver<mojom::RestrictedUDPSocket> receiver,
    mojo::PendingRemote<mojom::UDPSocketListener> listener,
    std::unique_ptr<SimpleHostResolver> resolver,
    mojom::NetworkContext::CreateRestrictedUDPSocketCallback callback) {
  auto udp_socket = std::make_unique<UDPSocket>(std::move(listener), net_log_);
  switch (mode) {
    case mojom::RestrictedUDPSocketMode::BOUND:
      udp_socket->Bind(addr, /*options=*/
                       params ? std::move(params->socket_options) : nullptr,
                       std::move(callback));
      break;
    case mojom::RestrictedUDPSocketMode::CONNECTED:
      udp_socket->Connect(addr, /*options=*/
                          params ? std::move(params->socket_options) : nullptr,
                          std::move(callback));
      break;
  }
  auto restricted_udp_socket = std::make_unique<RestrictedUDPSocket>(
      std::move(udp_socket), traffic_annotation, std::move(resolver));
#if BUILDFLAG(IS_CHROMEOS)
  if (params && params->connection_tracker) {
    restricted_udp_socket->AttachConnectionTracker(
        std::move(params->connection_tracker));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  restricted_udp_socket_receivers_.Add(std::move(restricted_udp_socket),
                                       std::move(receiver));
}

void SocketFactory::CreateTCPServerSocket(
    const net::IPEndPoint& local_addr,
    mojom::TCPServerSocketOptionsPtr options,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<mojom::TCPServerSocket> receiver,
    mojom::NetworkContext::CreateTCPServerSocketCallback callback) {
#if BUILDFLAG(IS_WIN)
  if (socket_broker_) {
    socket_broker_->CreateTcpSocket(
        local_addr.GetFamily(),
        base::BindOnce(&SocketFactory::DidCompleteCreate,
                       weak_ptr_factory_.GetWeakPtr(), local_addr,
                       std::move(options), traffic_annotation,
                       std::move(receiver), std::move(callback)));
    return;
  }
#endif
  auto socket =
      std::make_unique<TCPServerSocket>(this, net_log_, traffic_annotation);
  CreateTCPServerSocketHelper(std::move(socket), local_addr, std::move(options),
                              traffic_annotation, std::move(receiver),
                              std::move(callback));
}

#if BUILDFLAG(IS_WIN)
void SocketFactory::DidCompleteCreate(
    const net::IPEndPoint& local_addr,
    mojom::TCPServerSocketOptionsPtr options,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<mojom::TCPServerSocket> receiver,
    mojom::NetworkContext::CreateTCPServerSocketCallback callback,
    network::TransferableSocket socket,
    int result) {
  if (result != net::OK) {
    std::move(callback).Run(result, std::nullopt);
    return;
  }
  auto tcp_socket =
      std::make_unique<net::TCPServerSocket>(net_log_, net::NetLogSource());
  tcp_socket->AdoptSocket(socket.TakeSocket());

  auto tcp_server_socket = std::make_unique<TCPServerSocket>(
      std::move(tcp_socket), 0, this, traffic_annotation);

  CreateTCPServerSocketHelper(std::move(tcp_server_socket), local_addr,
                              std::move(options), traffic_annotation,
                              std::move(receiver), std::move(callback));
}

void SocketFactory::BindSocketBroker(
    mojo::PendingRemote<mojom::SocketBroker> pending_remote) {
  socket_broker_.Bind(std::move(pending_remote));
}
#endif

void SocketFactory::CreateTCPServerSocketHelper(
    std::unique_ptr<TCPServerSocket> socket,
    const net::IPEndPoint& local_addr,
    mojom::TCPServerSocketOptionsPtr options,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<mojom::TCPServerSocket> receiver,
    mojom::NetworkContext::CreateTCPServerSocketCallback callback) {
#if BUILDFLAG(IS_CHROMEOS)
  if (options->connection_tracker) {
    socket->AttachConnectionTracker(std::move(options->connection_tracker));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  std::optional<bool> ipv6_only;
  switch (options->ipv6_only) {
    case mojom::OptionalBool::kTrue:
      ipv6_only = true;
      break;
    case mojom::OptionalBool::kFalse:
      ipv6_only = false;
      break;
    case mojom::OptionalBool::kUnset:
      break;
  }
  base::expected<net::IPEndPoint, int32_t> result =
      socket->Listen(local_addr, options->backlog, ipv6_only);
  if (!result.has_value()) {
    std::move(callback).Run(result.error(), std::nullopt);
    return;
  }
  tcp_server_socket_receivers_.Add(std::move(socket), std::move(receiver));
  std::move(callback).Run(net::OK, result.value());
}

void SocketFactory::CreateTCPConnectedSocket(
    const std::optional<net::IPEndPoint>& local_addr,
    const net::AddressList& remote_addr_list,
    mojom::TCPConnectedSocketOptionsPtr tcp_connected_socket_options,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<mojom::TCPConnectedSocket> receiver,
    mojo::PendingRemote<mojom::SocketObserver> observer,
    mojom::NetworkContext::CreateTCPConnectedSocketCallback callback) {
  auto socket = std::make_unique<TCPConnectedSocket>(
      std::move(observer), net_log_, &tls_socket_factory_,
      client_socket_factory_, traffic_annotation);
  TCPConnectedSocket* socket_raw = socket.get();
  tcp_connected_socket_receiver_.Add(std::move(socket), std::move(receiver));
  socket_raw->Connect(local_addr, remote_addr_list,
                      std::move(tcp_connected_socket_options),
                      std::move(callback));
}

void SocketFactory::CreateTCPBoundSocket(
    const net::IPEndPoint& local_addr,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<mojom::TCPBoundSocket> receiver,
    mojom::NetworkContext::CreateTCPBoundSocketCallback callback) {
  auto socket =
      std::make_unique<TCPBoundSocket>(this, net_log_, traffic_annotation);
  net::IPEndPoint local_addr_out;
  int result = socket->Bind(local_addr, &local_addr_out);
  if (result != net::OK) {
    std::move(callback).Run(result, std::nullopt);
    return;
  }
  TCPBoundSocket* socket_ptr = socket.get();
  socket_ptr->set_id(
      tcp_bound_socket_receivers_.Add(std::move(socket), std::move(receiver)));
  std::move(callback).Run(result, local_addr_out);
}

void SocketFactory::DestroyBoundSocket(mojo::ReceiverId bound_socket_id) {
  tcp_bound_socket_receivers_.Remove(bound_socket_id);
}

void SocketFactory::OnBoundSocketListening(
    mojo::ReceiverId bound_socket_id,
    std::unique_ptr<TCPServerSocket> server_socket,
    mojo::PendingReceiver<mojom::TCPServerSocket> server_socket_receiver) {
  tcp_server_socket_receivers_.Add(std::move(server_socket),
                                   std::move(server_socket_receiver));
  tcp_bound_socket_receivers_.Remove(bound_socket_id);
}

void SocketFactory::OnBoundSocketConnected(
    mojo::ReceiverId bound_socket_id,
    std::unique_ptr<TCPConnectedSocket> connected_socket,
    mojo::PendingReceiver<mojom::TCPConnectedSocket>
        connected_socket_receiver) {
  tcp_connected_socket_receiver_.Add(std::move(connected_socket),
                                     std::move(connected_socket_receiver));
  tcp_bound_socket_receivers_.Remove(bound_socket_id);
}

void SocketFactory::OnAccept(
    std::unique_ptr<TCPConnectedSocket> socket,
    mojo::PendingReceiver<mojom::TCPConnectedSocket> receiver) {
  tcp_connected_socket_receiver_.Add(std::move(socket), std::move(receiver));
}

}  // namespace network
