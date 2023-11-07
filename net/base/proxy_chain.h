// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PROXY_CHAIN_H_
#define NET_BASE_PROXY_CHAIN_H_

#include <stdint.h>

#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/proxy_server.h"

namespace net {

// ProxyChain represents a chain of ProxyServers. A chain with multiple proxy
// servers means that a single connection will go through all of the proxies in
// order, using a tunnel through the first proxy to connect to the second, etc.
// A "direct" connection is a chain of length zero.
//
// TODO(crbug.com/1491092): Initial implementations of proxy chaining may, in
// fact, not tunnel through the last proxy in the ProxyChain if the destination
// is http.
class NET_EXPORT ProxyChain {
 public:
  // Constructs an invalid ProxyChain.
  ProxyChain();

  ProxyChain(const ProxyChain& other);      // Copy constructor
  ProxyChain(ProxyChain&& other) noexcept;  // Move constructor

  ProxyChain& operator=(const ProxyChain& other);  // Copy assignment operator
  ProxyChain& operator=(
      ProxyChain&& other) noexcept;  // Move assignment operator

  ProxyChain(ProxyServer::Scheme scheme, const HostPortPair& host_port_pair);

  explicit ProxyChain(std::vector<ProxyServer> proxy_server_list);
  explicit ProxyChain(ProxyServer proxy_server);

  ~ProxyChain();  // Destructor declaration

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
                                          std::string_view host,
                                          std::string_view port_str) {
    return ProxyChain(
        ProxyServer::FromSchemeHostAndPort(scheme, host, port_str));
  }
  static ProxyChain FromSchemeHostAndPort(ProxyServer::Scheme scheme,
                                          std::string_view host,
                                          std::optional<uint16_t> port) {
    return ProxyChain(ProxyServer::FromSchemeHostAndPort(scheme, host, port));
  }
  // Create a "direct" proxy chain, which includes no proxy servers.
  static ProxyChain Direct() { return ProxyChain(ProxyServer::Direct()); }

  // Get ProxyServer at index in chain. This is not valid for direct or invalid
  // proxy chains.
  const ProxyServer& GetProxyServer(size_t chain_index) const;

  // Get the ProxyServers in this chain. This must not be called on invalid
  // proxy chains. An empty vector is returned for direct proxy chains.
  const std::vector<ProxyServer>& proxy_servers() const;

  // Get the ProxyServers in this chain, or `nullopt` if the chain is not valid.
  const std::optional<std::vector<ProxyServer>>& proxy_servers_if_valid()
      const {
    return proxy_server_list_;
  }

  // Get the single ProxyServer equivalent to this chain. This must not be
  // called for multi-proxy chains.
  // TODO(crbug.com/1491092): Remove this method.
  const ProxyServer& proxy_server() const;

  // Returns number of proxy servers in chain.
  size_t length() const {
    return proxy_server_list_.has_value() ? proxy_server_list_.value().size()
                                          : 0;
  }

  // Returns true if this chain contains more than one proxy.
  bool is_multi_proxy() const {
    return proxy_server_list_.has_value()
               ? proxy_server_list_.value().size() > 1
               : false;
  }

  // Returns true if this chain contains exactly one proxy.
  bool is_single_proxy() const {
    return proxy_server_list_.has_value()
               ? proxy_server_list_.value().size() == 1
               : false;
  }

  // Returns true if this is a direct (equivalently, zero-proxy) chain.
  bool is_direct() const {
    return proxy_server_list_.has_value() ? proxy_server_list_.value().empty()
                                          : false;
  }

  // Returns true if a proxy server list is available .
  bool IsValid() const { return proxy_server_list_.has_value(); }

  bool operator==(const ProxyChain& other) const {
    return proxy_server_list_ == other.proxy_server_list_;
  }

  bool operator!=(const ProxyChain& other) const { return !(*this == other); }

  // Comparator function so this can be placed in a std::map.
  bool operator<(const ProxyChain& other) const {
    return proxy_server_list_ < other.proxy_server_list_;
  }

  std::string ToDebugString() const;

 private:
  std::optional<std::vector<ProxyServer>> proxy_server_list_;

  // Returns true if this chain is valid.
  bool IsValidInternal() const;
};

NET_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                            const ProxyChain& proxy_chain);

// A HostPortProxyPair holds a host/port destination and a ProxyChain describing
// how that destination is reached.
typedef std::pair<HostPortPair, ProxyChain> HostPortProxyPair;

}  // namespace net

#endif  // NET_BASE_PROXY_CHAIN_H_
