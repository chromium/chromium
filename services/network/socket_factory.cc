// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/socket_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "mojo/public/cpp/bindings/type_converter.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_config_service.h"
#include "net/url_request/url_request_context.h"
#include "services/network/public/cpp/simple_host_resolver.h"
#include "services/network/restricted_udp_socket.h"
#include "services/network/tls_client_socket.h"
#include "services/network/udp_socket.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
    mojom::UDPSocketOptionsPtr options,
    mojo::PendingReceiver<mojom::RestrictedUDPSocket> receiver,
    mojo::PendingRemote<mojom::UDPSocketListener> listener,
    std::unique_ptr<SimpleHostResolver> resolver,
    mojom::NetworkContext::CreateRestrictedUDPSocketCallback callback) {
  auto udp_socket = std::make_unique<UDPSocket>(std::move(listener), net_log_);
  switch (mode) {
    case mojom::RestrictedUDPSocketMode::BOUND:
      udp_socket->Bind(addr, std::move(options), std::move(callback));
      break;
    case mojom::RestrictedUDPSocketMode::CONNECTED:
      udp_socket->Connect(addr, std::move(options), std::move(callback));
      break;
  }
  restricted_udp_socket_receivers_.Add(
      std::make_unique<RestrictedUDPSocket>(
          std::move(udp_socket), traffic_annotation, std::move(resolver)),
      std::move(receiver));
}

void SocketFactory::CreateTCPServerSocket(
    const net::IPEndPoint& local_addr,
    int backlog,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<mojom::TCPServerSocket> receiver,
    mojom::NetworkContext::CreateTCPServerSocketCallback callback) {
  auto socket =
      std::make_unique<TCPServerSocket>(this, net_log_, traffic_annotation);
  net::IPEndPoint local_addr_out;
  int result = socket->Listen(local_addr, backlog, &local_addr_out);
  if (result != net::OK) {
    std::move(callback).Run(result, absl::nullopt);
    return;
  }
  tcp_server_socket_receivers_.Add(std::move(socket), std::move(receiver));
  std::move(callback).Run(result, local_addr_out);
}

void SocketFactory::CreateTCPConnectedSocket(
    const absl::optional<net::IPEndPoint>& local_addr,
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
    std::move(callback).Run(result, absl::nullopt);
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
