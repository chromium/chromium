// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_BROKERED_CLIENT_SOCKET_FACTORY_H_
#define SERVICES_NETWORK_BROKERED_CLIENT_SOCKET_FACTORY_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_socket.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/transport_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class AddressList;
class DatagramClientSocket;
class HostPortPair;
class NetLog;
struct NetLogSource;
class SSLClientContext;
class SSLClientSocket;
struct SSLConfig;
class NetworkQualityEstimator;

}  // namespace net

namespace network {

// A ClientSocketFactory to create brokered sockets.
class COMPONENT_EXPORT(NETWORK_SERVICE) BrokeredClientSocketFactory
    : public net::ClientSocketFactory {
 public:
  BrokeredClientSocketFactory();
  ~BrokeredClientSocketFactory() override;

  BrokeredClientSocketFactory(const BrokeredClientSocketFactory&) = delete;
  BrokeredClientSocketFactory& operator=(const BrokeredClientSocketFactory&) =
      delete;

  // ClientSocketFactory:
  std::unique_ptr<net::DatagramClientSocket> CreateDatagramClientSocket(
      net::DatagramSocket::BindType bind_type,
      net::NetLog* net_log,
      const net::NetLogSource& source) override;
  std::unique_ptr<net::TransportClientSocket> CreateTransportClientSocket(
      const net::AddressList& addresses,
      std::unique_ptr<net::SocketPerformanceWatcher> socket_performance_watcher,
      net::NetworkQualityEstimator* network_quality_estimator,
      net::NetLog* net_log,
      const net::NetLogSource& source) override;
  std::unique_ptr<net::SSLClientSocket> CreateSSLClientSocket(
      net::SSLClientContext* context,
      std::unique_ptr<net::StreamSocket> stream_socket,
      const net::HostPortPair& host_and_port,
      const net::SSLConfig& ssl_config) override;
};

}  // namespace network

#endif  // SERVICES_NETWORK_BROKERED_CLIENT_SOCKET_FACTORY_H_
