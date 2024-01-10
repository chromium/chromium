// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TLS_CLIENT_SOCKET_H_
#define SERVICES_NETWORK_TLS_CLIENT_SOCKET_H_

#include <memory>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_family.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/address_family.mojom.h"
#include "services/network/public/mojom/ip_endpoint.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "services/network/socket_data_pump.h"

namespace net {
class ClientSocketFactory;
class SSLClientContext;
class SSLClientSocket;
struct SSLConfig;
class StreamSocket;
}  // namespace net

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) TLSClientSocket
    : public mojom::TLSClientSocket,
      public SocketDataPump::Delegate {
 public:
  TLSClientSocket(mojo::PendingRemote<mojom::SocketObserver> observer,
                  const net::NetworkTrafficAnnotationTag& traffic_annotation);

  TLSClientSocket(const TLSClientSocket&) = delete;
  TLSClientSocket& operator=(const TLSClientSocket&) = delete;

  ~TLSClientSocket() override;

  void Connect(const net::HostPortPair& host_port_pair,
               const net::SSLConfig& ssl_config,
               std::unique_ptr<net::StreamSocket> tcp_socket,
               net::SSLClientContext* ssl_client_context,
               net::ClientSocketFactory* socket_factory,
               mojom::TCPConnectedSocket::UpgradeToTLSCallback callback,
               bool send_ssl_info);

 private:
  void OnTLSConnectCompleted(int result);

  // SocketDataPump::Delegate implementation.
  void OnNetworkReadError(int net_error) override;
  void OnNetworkWriteError(int net_error) override;
  void OnShutdown() override;

  const mojo::Remote<mojom::SocketObserver> observer_;
  // `socket_` must outlive `socket_data_pump_`.
  std::unique_ptr<net::SSLClientSocket> socket_;
  std::unique_ptr<SocketDataPump> socket_data_pump_;
  mojom::TCPConnectedSocket::UpgradeToTLSCallback connect_callback_;
  bool send_ssl_info_ = false;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TLS_CLIENT_SOCKET_H_
