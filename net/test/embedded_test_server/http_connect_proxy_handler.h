// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECT_PROXY_HANDLER_H_
#define NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECT_PROXY_HANDLER_H_

#include <memory>
#include <optional>
#include <set>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/unique_ptr_adapters.h"
#include "net/base/host_port_pair.h"

namespace net::test_server {

class HttpConnection;
struct HttpRequest;

// Helper for use by the EmbeddedTestServer to act as an HTTP proxy. Only
// supports HTTP/1.x CONNECT requests. Non-CONNECT requests can be simulated
// without a proxy, if needed.
class HttpConnectProxyHandler {
 public:
  // Only CONNECT requests to destinations in proxied_destinations will be
  // proxied. All others result in 502 errors. CONNECT requests are all
  // connected to 127.0.0.1:<port> where `port` is the destination port of the
  // requested destination.
  explicit HttpConnectProxyHandler(
      base::span<const HostPortPair> proxied_destinations);
  ~HttpConnectProxyHandler();

  // `request` must be a CONNECT request. Returns false if the request is not a
  // valid CONNECT request or is to a destination in `dest_ports`. In the former
  // case, adds a gtest failure as well.
  //
  // If true is returned, takes ownership of the underlying socket, and attempts
  // to establish an HTTP/1.x tunnel between that socket and the destination
  // port on localost.
  //
  // Write an error to the socket and logs on connection failures.
  bool HandleProxyRequest(HttpConnection& connection,
                          const HttpRequest& request);

 private:
  class ConnectTunnel;

  void DeleteTunnel(ConnectTunnel* tunnel);

  const base::flat_set<HostPortPair> proxied_destinations_;

  std::set<std::unique_ptr<ConnectTunnel>, base::UniquePtrComparator>
      connect_tunnels_;
};

}  // namespace net::test_server

#endif  // NET_TEST_EMBEDDED_TEST_SERVER_HTTP_CONNECT_PROXY_HANDLER_H_
