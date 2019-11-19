// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test methods and classes common to transport_client_socket_pool_unittest.cc
// and websocket_transport_client_socket_pool_unittest.cc. If you find you need
// to use these for another purpose, consider moving them to socket_test_util.h.

#ifndef NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_TEST_UTIL_H_
#define NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/queue.h"
#include "base/macros.h"
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
  enum ClientSocketType {
    // Connects successfully, synchronously.
    MOCK_CLIENT_SOCKET,
    // Fails to connect, synchronously.
    MOCK_FAILING_CLIENT_SOCKET,
    // Connects successfully, asynchronously.
    MOCK_PENDING_CLIENT_SOCKET,
    // Fails to connect, asynchronously.
    MOCK_PENDING_FAILING_CLIENT_SOCKET,
    // A delayed socket will pause before connecting through the message loop.
    MOCK_DELAYED_CLIENT_SOCKET,
    // A delayed socket that fails.
    MOCK_DELAYED_FAILING_CLIENT_SOCKET,
    // A stalled socket that never connects at all.
    MOCK_STALLED_CLIENT_SOCKET,
    // A stalled socket that never connects at all, but returns a failing
    // ConnectionAttempt in |GetConnectionAttempts|.
    MOCK_STALLED_FAILING_CLIENT_SOCKET,
    // A socket that can be triggered to connect explicitly, asynchronously.
    MOCK_TRIGGERABLE_CLIENT_SOCKET,
  };

  explicit MockTransportClientSocketFactory(NetLog* net_log);
  ~MockTransportClientSocketFactory() override;

  std::unique_ptr<DatagramClientSocket> CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      NetLog* net_log,
      const NetLogSource& source) override;

  std::unique_ptr<TransportClientSocket> CreateTransportClientSocket(
      const AddressList& addresses,
      std::unique_ptr<
          SocketPerformanceWatcher> /* socket_performance_watcher */,
      NetLog* /* net_log */,
      const NetLogSource& /* source */) override;

  std::unique_ptr<SSLClientSocket> CreateSSLClientSocket(
      SSLClientContext* context,
      std::unique_ptr<StreamSocket> nested_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config) override;

  std::unique_ptr<ProxyClientSocket> CreateProxyClientSocket(
      std::unique_ptr<StreamSocket> stream_socket,
      const std::string& user_agent,
      const HostPortPair& endpoint,
      const ProxyServer& proxy_server,
      HttpAuthController* http_auth_controller,
      bool tunnel,
      bool using_spdy,
      NextProto negotiated_protocol,
      ProxyDelegate* proxy_delegate,
      const NetworkTrafficAnnotationTag& traffic_annotation) override;

  int allocation_count() const { return allocation_count_; }

  // Set the default ClientSocketType.
  void set_default_client_socket_type(ClientSocketType type) {
    client_socket_type_ = type;
  }

  // Set a list of ClientSocketTypes to be used.
  void set_client_socket_types(ClientSocketType* type_list, int num_types);

  void set_delay(base::TimeDelta delay) { delay_ = delay; }

  // If one or more MOCK_TRIGGERABLE_CLIENT_SOCKETs has already been created,
  // then returns a Closure that can be called to cause the first
  // not-yet-connected one to connect. If no MOCK_TRIGGERABLE_CLIENT_SOCKETs
  // have been created yet, wait for one to be created before returning the
  // Closure. This method should be called the same number of times as
  // MOCK_TRIGGERABLE_CLIENT_SOCKETs are created in the test.
  base::Closure WaitForTriggerableSocketCreation();

 private:
  NetLog* net_log_;
  int allocation_count_;
  ClientSocketType client_socket_type_;
  ClientSocketType* client_socket_types_;
  int client_socket_index_;
  int client_socket_index_max_;
  base::TimeDelta delay_;
  base::queue<base::Closure> triggerable_sockets_;
  base::Closure run_loop_quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(MockTransportClientSocketFactory);
};

}  // namespace net

#endif  // NET_SOCKET_TRANSPORT_CLIENT_SOCKET_POOL_TEST_UTIL_H_
