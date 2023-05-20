// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_BROKERED_CLIENT_SOCKET_FACTORY_H_
#define SERVICES_NETWORK_BROKERED_CLIENT_SOCKET_FACTORY_H_

#include "base/component_export.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/datagram_socket.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/transport_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/socket_broker.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "services/network/broker_helper_win.h"
#endif

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
  explicit BrokeredClientSocketFactory(
      mojo::PendingRemote<mojom::SocketBroker> pending_remote);
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

  // Sends an IPC to the SocketBroker to create a new TCP socket.
  void BrokerCreateTcpSocket(
      net::AddressFamily address_family,
      mojom::SocketBroker::CreateTcpSocketCallback callback);

  // Sends an IPC to the SocketBroker to create a new UDP socket.
  void BrokerCreateUdpSocket(
      net::AddressFamily address_family,
      mojom::SocketBroker::CreateUdpSocketCallback callback);

  // Whether or not a socket for `addresses` should be brokered or not. Virtual
  // for testing.
  virtual bool ShouldBroker(const net::AddressList& addresses) const;

 private:
  mojo::Remote<mojom::SocketBroker> socket_broker_;
#if BUILDFLAG(IS_WIN)
  BrokerHelperWin broker_helper_;
#endif
};

}  // namespace network

#endif  // SERVICES_NETWORK_BROKERED_CLIENT_SOCKET_FACTORY_H_
