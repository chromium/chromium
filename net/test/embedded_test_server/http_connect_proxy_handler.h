// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECT_PROXY_HANDLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECT_PROXY_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "net/base/host_port_pair.h"

namespace net::test_server {

class HttpConnection;
struct HttpRequest;

// Helper for use by the EmbeddedTestServer to act as an HTTP proxy. Only
// supports HTTP/1.x CONNECT requests.
class HttpConnectProxyHandler {
 public:
  // Regardless of the requested destination, all requests are proxied to
  // `dest_port` on 127.0.0.1. If provided, connections to destinations other
  // than `expected_dest` result in test expectation failures.
  HttpConnectProxyHandler(uint16_t dest_port,
                          std::optional<HostPortPair> expected_dest);
  ~HttpConnectProxyHandler();

  // If `request` is not a CONNECT request, returns false. Otherwise, takes
  // ownership of the socket owned by `connection`, validates the destination,
  // connects to the real destination location, writes an HTTP success message
  // to the original socket, and starts tunnelling data between the two sockets.
  //
  // Logs on connection failures, but does not fail the test.
  //
  // Only supports CONNECT requests.  Non-CONNECT requests can be simulated
  // without a proxy, if needed.
  bool HandleProxyRequest(HttpConnection& connection,
                          const HttpRequest& request);

 private:
  class ConnectTunnel;

  void DeleteTunnel(ConnectTunnel* tunnel);

  const uint16_t dest_port_;
  const std::optional<HostPortPair> expected_dest_;

  std::set<std::unique_ptr<ConnectTunnel>, base::UniquePtrComparator>
      connect_tunnels_;
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECT_PROXY_HANDLER_H_
