// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PROXY_CHAIN_H_
#define NET_BASE_PROXY_CHAIN_H_

#include <stdint.h>

#include <ostream>
#include <string>
#include <tuple>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/proxy_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

// ProxyChain represents a chain of ProxyServers. A chain with multiple proxy
// servers means that a single connection will go through all of the proxies in
// order, using a tunnel through the first proxy to connect to the second, etc.
// A "direct" connection is a chain of length zero.
//
// TODO(crbug.com/1491092): Initial implementations of proxy chaining may, in
// fact, not tunnel through the last proxy in the ProxyChain if the destination
// is http.
//
// TODO(crbug.com/1491092): This does not currently support multi-proxy chains,
// so a ProxyChain is always inter-convertable to a ProxyServer.
class NET_EXPORT ProxyChain {
 public:
  // Default copy-constructor and assignment operator are OK!

  // Constructs an invalid ProxyChain.
  ProxyChain() = default;

  ProxyChain(ProxyServer::Scheme scheme, const HostPortPair& host_port_pair)
      : proxy_server_(scheme, host_port_pair) {}

  explicit ProxyChain(ProxyServer proxy_server) : proxy_server_(proxy_server) {}

  // Creates a single-proxy ProxyChain, validating and canonicalizing input.
  // Port is optional and, if not provided, will be replaced with the default
  // port for the given scheme. Accepts IPv6 literal `host`s with surrounding
  // brackets (URL format) or without (HostPortPair format). On invalid input,
  // result will be a `SCHEME_INVALID` ProxyChain.
  //
  // Must not be called with `SCHEME_INVALID` or `SCHEME_DIRECT`. Use
  // `ProxyChain()` or `Direct()` respectively to create an invalid or direct
  // ProxyChain.
  static ProxyChain FromSchemeHostAndPort(ProxyServer::Scheme scheme,
                                          base::StringPiece host,
                                          base::StringPiece port_str) {
    return ProxyChain(
        ProxyServer::FromSchemeHostAndPort(scheme, host, port_str));
  }
  static ProxyChain FromSchemeHostAndPort(ProxyServer::Scheme scheme,
                                          base::StringPiece host,
                                          absl::optional<uint16_t> port) {
    return ProxyChain(ProxyServer::FromSchemeHostAndPort(scheme, host, port));
  }

  // Create a "direct" proxy chain, which includes no proxy servers.
  static ProxyChain Direct() { return ProxyChain(ProxyServer::Direct()); }

  // Returns true if this chain contains more than one proxy.
  bool is_multi_proxy() const { return false; }

  // Returns true if this is a direct (equivalently, zero-proxy) chain.
  bool is_direct() const { return proxy_server_.is_direct(); }

  // Returns true if this chain is valid.
  bool IsValid() const;

  bool operator==(const ProxyChain& other) const {
    return proxy_server_ == other.proxy_server_;
  }

  bool operator!=(const ProxyChain& other) const { return !(*this == other); }

  // Comparator function so this can be placed in a std::map.
  bool operator<(const ProxyChain& other) const {
    return proxy_server_ < other.proxy_server_;
  }

  std::string ToDebugString() const;

  // Get the single ProxyServer equivalent to this chain. This must not be
  // called for multi-proxy chains.
  // TODO(crbug.com/1491092): Remove this method.
  const ProxyServer& proxy_server() const {
    CHECK(!is_multi_proxy());
    return proxy_server_;
  }

 private:
  ProxyServer proxy_server_;
};

NET_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                            const ProxyChain& proxy_chain);

}  // namespace net

#endif  // NET_BASE_PROXY_CHAIN_H_
