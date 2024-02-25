// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/brokered_client_socket_factory.h"

#include "build/build_config.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/udp_client_socket.h"
#include "services/network/broker_helper_win.h"
#include "services/network/brokered_tcp_client_socket.h"
#include "services/network/brokered_udp_client_socket.h"

namespace net {

class AddressList;
class HostPortPair;
class NetLog;
struct NetLogSource;
class SSLClientContext;
class SSLClientSocket;
struct SSLConfig;
class NetworkQualityEstimator;

}  // namespace net

namespace network {

BrokeredClientSocketFactory::BrokeredClientSocketFactory(
    mojo::PendingRemote<mojom::SocketBroker> pending_remote)
    : socket_broker_(std::move(pending_remote)) {}
BrokeredClientSocketFactory::~BrokeredClientSocketFactory() = default;

std::unique_ptr<net::DatagramClientSocket>
BrokeredClientSocketFactory::CreateDatagramClientSocket(
    net::DatagramSocket::BindType bind_type,
    net::NetLog* net_log,
    const net::NetLogSource& source) {
  return std::make_unique<BrokeredUdpClientSocket>(bind_type, net_log, source,
                                                   this);
}

std::unique_ptr<net::TransportClientSocket>
BrokeredClientSocketFactory::CreateTransportClientSocket(
    const net::AddressList& addresses,
    std::unique_ptr<net::SocketPerformanceWatcher> socket_performance_watcher,
    net::NetworkQualityEstimator* network_quality_estimator,
    net::NetLog* net_log,
    const net::NetLogSource& source) {
  if (ShouldBroker(addresses)) {
    return std::make_unique<BrokeredTcpClientSocket>(
        addresses, std::move(socket_performance_watcher),
        network_quality_estimator, net_log, source, this);
  }

  return std::make_unique<net::TCPClientSocket>(
      addresses, std::move(socket_performance_watcher),
      network_quality_estimator, net_log, source);
}

std::unique_ptr<net::SSLClientSocket>
BrokeredClientSocketFactory::CreateSSLClientSocket(
    net::SSLClientContext* context,
    std::unique_ptr<net::StreamSocket> stream_socket,
    const net::HostPortPair& host_and_port,
    const net::SSLConfig& ssl_config) {
  // TODO(liza): Call into the broker rather than directly to net.
  return context->CreateSSLClientSocket(std::move(stream_socket), host_and_port,
                                        ssl_config);
}

void BrokeredClientSocketFactory::BrokerCreateTcpSocket(
    net::AddressFamily address_family,
    mojom::SocketBroker::CreateTcpSocketCallback callback) {
  socket_broker_->CreateTcpSocket(address_family, std::move(callback));
}

void BrokeredClientSocketFactory::BrokerCreateUdpSocket(
    net::AddressFamily address_family,
    mojom::SocketBroker::CreateUdpSocketCallback callback) {
  socket_broker_->CreateUdpSocket(address_family, std::move(callback));
}

bool BrokeredClientSocketFactory::ShouldBroker(
    const net::AddressList& addresses) const {
  for (const auto& address : addresses) {
    if (broker_helper_.ShouldBroker(address.address()))
      return true;
  }
  return false;
}

}  // namespace network
