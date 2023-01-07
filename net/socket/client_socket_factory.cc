// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_factory.h"

#include <utility>

#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/udp_client_socket.h"

namespace net {

class X509Certificate;

namespace {

class DefaultClientSocketFactory : public ClientSocketFactory {
 public:
  DefaultClientSocketFactory() = default;

  // Note: This code never runs, as the factory is defined as a Leaky singleton.
  ~DefaultClientSocketFactory() override = default;

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override {
    return std::make_unique<UDPClientSocket>(bind_type, net_log, source);
  }

  std::unique_ptr<TransportClientSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
      NetworkQualityEstimator* network_quality_estimator,
      NetLog* net_log,
      const NetLogSource& source) override {
    return std::make_unique<TCPClientSocket>(
        addresses, std::move(socket_performance_watcher),
        network_quality_estimator, net_log, source);
  }

  std::unique_ptr<SSLClientSocket> CreateSSLClientSocket(
      SSLClientContext* context,
      std::unique_ptr<StreamSocket> stream_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config) override {
    return context->CreateSSLClientSocket(std::move(stream_socket),
                                          host_and_port, ssl_config);
  }
};

static base::LazyInstance<DefaultClientSocketFactory>::Leaky
    g_default_client_socket_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ClientSocketFactory* ClientSocketFactory::GetDefaultFactory() {
  return g_default_client_socket_factory.Pointer();
}

}  // namespace net
