// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_DELAYED_CLIENT_SOCKET_FACTORY_H_
#define NET_SOCKET_DELAYED_CLIENT_SOCKET_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/net_export.h"
#include "net/base/network_handle.h"
#include "net/socket/bandwidth_throttle.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/delayed_socket_config.h"

namespace net {

// A ClientSocketFactory decorator that wraps transport sockets in
// DelayedStreamSocket to inject socket-level network delay. SSL sockets are
// created normally — the delay is below them (on the transport socket that
// SSL wraps), so TLS handshake round trips naturally experience the delay.
//
// Owns shared BandwidthThrottle instances for coordinated bandwidth limiting
// across all sockets created by this factory.
class NET_EXPORT DelayedClientSocketFactory : public ClientSocketFactory {
 public:
  // Wraps |inner_factory|, delegating socket creation to it and then
  // wrapping the resulting sockets with delay. |inner_factory| is not
  // owned and must outlive this factory. Creates shared BandwidthThrottle
  // instances based on config throughput values.
  DelayedClientSocketFactory(ClientSocketFactory* inner_factory,
                             const DelayedSocketConfig& config);

  DelayedClientSocketFactory(const DelayedClientSocketFactory&) = delete;
  DelayedClientSocketFactory& operator=(const DelayedClientSocketFactory&) =
      delete;

  ~DelayedClientSocketFactory() override;

  // Updates the delay config. New sockets will use the new config.
  // Existing sockets are unaffected.
  void SetConfig(const DelayedSocketConfig& config);

  const DelayedSocketConfig& config() const { return config_; }

  // ClientSocketFactory implementation:
  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      handles::NetworkHandle target_network,
      NetLog* net_log,
      const NetLogSource& source) override;

  std::unique_ptr<TransportClientSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      handles::NetworkHandle target_network,
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
  void RecreateThrottles();

  raw_ptr<ClientSocketFactory> inner_factory_;
  DelayedSocketConfig config_;

  // Shared bandwidth throttles. Null if throughput is unlimited.
  scoped_refptr<BandwidthThrottle> download_throttle_;
  scoped_refptr<BandwidthThrottle> upload_throttle_;
};

}  // namespace net

#endif  // NET_SOCKET_DELAYED_CLIENT_SOCKET_FACTORY_H_
