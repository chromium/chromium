// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_FUZZED_SOCKET_FACTORY_H_
#define NET_SOCKET_FUZZED_SOCKET_FACTORY_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "net/socket/client_socket_factory.h"

class FuzzedDataProvider;

namespace net {

// A socket factory that creates FuzzedSockets that share the same
// FuzzedDataProvider. To behave consistently, the read operations on all
// sockets must be the same, and in the same order (both on each socket, and
// between sockets).
//
// Currently doesn't support SSL sockets - just returns sockets that
// synchronously fail to connect when trying to create either type of socket.
// TODO(mmenke): Add support for ssl sockets.
// TODO(mmenke): add fuzzing for generation of valid cryptographically signed
// messages.
class FuzzedSocketFactory : public ClientSocketFactory {
 public:
  // |data_provider| must outlive the FuzzedSocketFactory, and all sockets it
  // creates. Other objects can also continue to consume |data_provider|, as
  // long as their calls into it are made on the CLientSocketFactory's thread
  // and the calls are deterministic.
  explicit FuzzedSocketFactory(FuzzedDataProvider* data_provider);

  FuzzedSocketFactory(const FuzzedSocketFactory&) = delete;
  FuzzedSocketFactory& operator=(const FuzzedSocketFactory&) = delete;

  ~FuzzedSocketFactory() override;

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

  // Sets whether Connect()ions on returned sockets can be asynchronously
  // delayed or outright fail. Defaults to true.
  void set_fuzz_connect_result(bool v) { fuzz_connect_result_ = v; }

 private:
  raw_ptr<FuzzedDataProvider> data_provider_;
  bool fuzz_connect_result_ = true;
};

}  // namespace net

#endif  // NET_SOCKET_FUZZED_SOCKET_FACTORY_H_
