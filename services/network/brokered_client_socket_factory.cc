// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "services/network/brokered_client_socket_factory.h"

#include "build/build_config.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "services/network/tcp_client_socket_brokered.h"

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

BrokeredClientSocketFactory::BrokeredClientSocketFactory() = default;
BrokeredClientSocketFactory::~BrokeredClientSocketFactory() = default;

std::unique_ptr<net::DatagramClientSocket>
BrokeredClientSocketFactory::CreateDatagramClientSocket(
    net::DatagramSocket::BindType bind_type,
    net::NetLog* net_log,
    const net::NetLogSource& source) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<net::TransportClientSocket>
BrokeredClientSocketFactory::CreateTransportClientSocket(
    const net::AddressList& addresses,
    std::unique_ptr<net::SocketPerformanceWatcher> socket_performance_watcher,
    net::NetworkQualityEstimator* network_quality_estimator,
    net::NetLog* net_log,
    const net::NetLogSource& source) {
  return std::make_unique<TCPClientSocketBrokered>(
      addresses, std::move(socket_performance_watcher),
      network_quality_estimator, net_log, source);
}

std::unique_ptr<net::SSLClientSocket>
BrokeredClientSocketFactory::CreateSSLClientSocket(
    net::SSLClientContext* context,
    std::unique_ptr<net::StreamSocket> stream_socket,
    const net::HostPortPair& host_and_port,
    const net::SSLConfig& ssl_config) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace network
