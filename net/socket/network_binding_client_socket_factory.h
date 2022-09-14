// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_NETWORK_BINDING_CLIENT_SOCKET_FACTORY_H_
#define NET_SOCKET_NETWORK_BINDING_CLIENT_SOCKET_FACTORY_H_

#include "net/base/network_handle.h"
#include "net/socket/client_socket_factory.h"

namespace net {

// A ClientSocketFactory to create sockets bound to `network`.
class NetworkBindingClientSocketFactory : public ClientSocketFactory {
 public:
  explicit NetworkBindingClientSocketFactory(handles::NetworkHandle network);

  NetworkBindingClientSocketFactory(const NetworkBindingClientSocketFactory&) =
      delete;
  NetworkBindingClientSocketFactory& operator=(
      const NetworkBindingClientSocketFactory&) = delete;

  ~NetworkBindingClientSocketFactory() override = default;

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override;

  std::unique_ptr<TransportClientSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetworkQualityEstimator* network_quality_estimator,
      NetLog* net_log,
      const NetLogSource& source) override;

  std::unique_ptr<SSLClientSocket> CreateSSLClientSocket(
      SSLClientContext* context,
      std::unique_ptr<StreamSocket> stream_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config) override;

 private:
  handles::NetworkHandle network_;
};

}  // namespace net

#endif  // NET_SOCKET_NETWORK_BINDING_CLIENT_SOCKET_FACTORY_H_
