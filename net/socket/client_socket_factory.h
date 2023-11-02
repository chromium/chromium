// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CLIENT_SOCKET_FACTORY_H_
#define NET_SOCKET_CLIENT_SOCKET_FACTORY_H_

#include <memory>
#include <string>

#include "net/base/net_export.h"
#include "net/http/proxy_client_socket.h"
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

// An interface used to instantiate StreamSocket objects.  Used to facilitate
// testing code with mock socket implementations.
class NET_EXPORT ClientSocketFactory {
 public:
  virtual ~ClientSocketFactory() = default;

  // |source| is the NetLogSource for the entity trying to create the socket,
  // if it has one.
  virtual std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) = 0;

  // |network_quality_estimator| is optional. If not specified, the network
  // quality will not be considered when determining TCP connect handshake
  // timeouts, or when histogramming the handshake duration.
  virtual std::unique_ptr<TransportClientSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetworkQualityEstimator* network_quality_estimator,
      NetLog* net_log,
      const NetLogSource& source) = 0;

  // It is allowed to pass in a StreamSocket that is not obtained from a
  // socket pool. The caller could create a StreamSocket directly.
  virtual std::unique_ptr<SSLClientSocket> CreateSSLClientSocket(
      SSLClientContext* context,
      std::unique_ptr<StreamSocket> stream_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config) = 0;

  // Returns the default ClientSocketFactory.
  static ClientSocketFactory* GetDefaultFactory();
};

}  // namespace net

#endif  // NET_SOCKET_CLIENT_SOCKET_FACTORY_H_
