// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test methods and classes common to transport_client_socket_pool_unittest.cc
// and websocket_transport_client_socket_pool_unittest.cc. If you find you need
// to use these for another purpose, consider moving them to socket_test_util.h.

#ifndef NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_TEST_UTIL_H_
#define NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_TEST_UTIL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/stream_socket.h"

namespace net {

class ClientSocketHandle;
class IPEndPoint;
class NetLog;

// Make sure |handle| sets load times correctly when it has been assigned a
// reused socket. Uses gtest expectations.
void TestLoadTimingInfoConnectedReused(const ClientSocketHandle& handle);

// Make sure |handle| sets load times correctly when it has been assigned a
// fresh socket.  Also runs TestLoadTimingInfoConnectedReused, since the owner
// of a connection where |is_reused| is false may consider the connection
// reused. Uses gtest expectations.
void TestLoadTimingInfoConnectedNotReused(const ClientSocketHandle& handle);

// Set |address| to 1.1.1.1:80
void SetIPv4Address(IPEndPoint* address);

// Set |address| to [1:abcd::3:4:ff]:80
void SetIPv6Address(IPEndPoint* address);

// A ClientSocketFactory that produces sockets with the specified connection
// behaviours.
class MockTransportClientSocketFactory : public ClientSocketFactory {
 public:
  // The type of socket to create.
  enum class Type {
    // An unexpected socket. Causes a test failure if run.
    kUnexpected,
    // Connects successfully, synchronously.
    kSynchronous,
    // Fails to connect, synchronously.
    kFailing,
    // Connects successfully, asynchronously.
    kPending,
    // Fails to connect, asynchronously.
    kPendingFailing,
    // A delayed socket will pause before connecting through the message loop.
    kDelayed,
    // A delayed socket that fails.
    kDelayedFailing,
    // A stalled socket that never connects at all.
    kStalled,
    // A socket that can be triggered to connect explicitly, asynchronously.
    kTriggerable,
  };

  // A rule describing a mock `TransportClientSocket` to create.
  struct Rule {
    explicit Rule(Type type,
                  std::optional<std::vector<IPEndPoint>> expected_addresses =
                      std::nullopt,
                  Error connect_error = ERR_CONNECTION_FAILED);
    ~Rule();
    Rule(const Rule&);
    Rule& operator=(const Rule&);

    Type type;
    // If specified, the addresses that should be passed into
    // `CreateTransportClientSocket`.
    std::optional<std::vector<IPEndPoint>> expected_addresses;
    // The error to use if `type` specifies a failing connection. Ignored
    // otherwise.
    Error connect_error;
  };

  explicit MockTransportClientSocketFactory(NetLog* net_log);

  MockTransportClientSocketFactory(const MockTransportClientSocketFactory&) =
      delete;
  MockTransportClientSocketFactory& operator=(
      const MockTransportClientSocketFactory&) = delete;

  ~MockTransportClientSocketFactory() override;

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override;

  std::unique_ptr<TransportClientSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      std::unique_ptr<
          SocketPerformanceWatcher> /* socket_performance_watcher */,
      NetworkQualityEstimator* /* network_quality_estimator */,
      NetLog* /* net_log */,
      const NetLogSource& /* source */) override;

  std::unique_ptr<SSLClientSocket> CreateSSLClientSocket(
      SSLClientContext* context,
      std::unique_ptr<StreamSocket> nested_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config) override;

  int allocation_count() const { return allocation_count_; }

  // Set the default type for `CreateTransportClientSocket` calls, if all rules
  // (see `SetRules`) are consumed.
  void set_default_client_socket_type(Type type) { client_socket_type_ = type; }

  // Configures a list of rules for `CreateTransportClientSocket`. `rules` must
  // outlive the `MockTransportClientSocketFactory`. If
  // `CreateTransportClientSocket` is called more than `rules.size()` times,
  // excess calls will be treated as test failures, but this can be changed by
  // calling `set_default_client_socket_type` after this method.
  void SetRules(base::span<const Rule> rules);

  void set_delay(base::TimeDelta delay) { delay_ = delay; }

  // If one or more `kTriggerable` socket has already been created, then returns
  // a `OnceClosure` that can be called to cause the first not-yet-connected one
  // to connect. If no `kTriggerable` sockets have been created yet, wait for
  // one to be created before returning the `OnceClosure`. This method should be
  // called the same number of times as `kTriggerable` sockets are created in
  // the test.
  base::OnceClosure WaitForTriggerableSocketCreation();

 private:
  raw_ptr<NetLog> net_log_;
  int allocation_count_ = 0;
  Type client_socket_type_ = Type::kSynchronous;
  base::raw_span<const Rule, DanglingUntriaged> rules_;
  base::TimeDelta delay_;
  base::queue<base::OnceClosure> triggerable_sockets_;
  base::OnceClosure run_loop_quit_closure_;
};

}  // namespace net

#endif  // NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_TEST_UTIL_H_
