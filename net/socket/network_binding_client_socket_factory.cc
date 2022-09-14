// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/network_binding_client_socket_factory.h"

#include "net/socket/tcp_client_socket.h"
#include "net/socket/udp_client_socket.h"

namespace net {

NetworkBindingClientSocketFactory::NetworkBindingClientSocketFactory(
    handles::NetworkHandle network)
    : network_(network) {}

std::unique_ptr<DatagramClientSocket>
NetworkBindingClientSocketFactory::CreateDatagramClientSocket(
    DatagramSocket::BindType bind_type,
    NetLog* net_log,
    const NetLogSource& source) {
  return std::make_unique<UDPClientSocket>(bind_type, net_log, source,
                                           network_);
}

std::unique_ptr<TransportClientSocket>
NetworkBindingClientSocketFactory::CreateTransportClientSocket(
    const AddressList& addresses,
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetworkQualityEstimator* network_quality_estimator,
    NetLog* net_log,
    const NetLogSource& source) {
  return std::make_unique<TCPClientSocket>(
      addresses, std::move(socket_performance_watcher),
      network_quality_estimator, net_log, source, network_);
}

std::unique_ptr<SSLClientSocket>
NetworkBindingClientSocketFactory::CreateSSLClientSocket(
    SSLClientContext* context,
    std::unique_ptr<StreamSocket> stream_socket,
    const HostPortPair& host_and_port,
    const SSLConfig& ssl_config) {
  return ClientSocketFactory::GetDefaultFactory()->CreateSSLClientSocket(
      context, std::move(stream_socket), host_and_port, ssl_config);
}

}  // namespace net
