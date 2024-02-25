// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKETS_UDP_TEST_UDP_ECHO_SERVER_H_
#define EXTENSIONS_BROWSER_API_SOCKETS_UDP_TEST_UDP_ECHO_SERVER_H_

#include <memory>

#include "base/threading/thread.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/udp_socket_test_util.h"

namespace net {
class HostPortPair;
}  // namespace net

namespace extensions {

// Test UDP server that echos back everything it receives.
class TestUdpEchoServer {
 public:
  TestUdpEchoServer();

  // Destroying the server shuts it down.
  ~TestUdpEchoServer();

  TestUdpEchoServer(const TestUdpEchoServer&) = delete;
  TestUdpEchoServer& operator=(const TestUdpEchoServer&) = delete;

  // Starts the echo server, and returns an error on failure. Sets
  // |host_port_pair| to the the host and port the server is listening on.
  // |host_port_pair| must not be null. Spins the current message loop while
  // waiting for the server to start.
  [[nodiscard]] bool Start(network::mojom::NetworkContext* network_context,
                           net::HostPortPair* host_port_pair);

 private:
  // Class that does all the work. Created on the test server's thread, but
  // otherwise lives an IO thread, where it is also destroyed.
  class Core;

  std::unique_ptr<Core> core_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKETS_UDP_TEST_UDP_ECHO_SERVER_H_
