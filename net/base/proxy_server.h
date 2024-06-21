// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PROXY_SERVER_H_
#define NET_BASE_PROXY_SERVER_H_

#include <stdint.h>

#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <tuple>

#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"

namespace base {
class Pickle;
class PickleIterator;
}  // namespace base

namespace net {

// ProxyServer encodes the {type, host, port} of a proxy server.
// ProxyServer is immutable.
class NET_EXPORT ProxyServer {
 public:
  // The type of proxy. These are defined as bit flags so they can be ORed
  // together to pass as the |scheme_bit_field| argument to
  // ProxyList::RemoveProxiesWithoutScheme().
  enum Scheme {
    SCHEME_INVALID = 1 << 0,
    // SCHEME_DIRECT (value = 1 << 1) is no longer used or supported.
    SCHEME_HTTP = 1 << 2,
    SCHEME_SOCKS4 = 1 << 3,
    SCHEME_SOCKS5 = 1 << 4,
    SCHEME_HTTPS = 1 << 5,
    // A QUIC proxy is an HTTP proxy in which QUIC is used as the transport,
    // instead of TCP.
    SCHEME_QUIC = 1 << 6,
  };

  // Default copy-constructor and assignment operator are OK!

  // Constructs an invalid ProxyServer.
  ProxyServer() = default;

  ProxyServer(Scheme scheme, const HostPortPair& host_port_pair);

  // Creates a ProxyServer, validating and canonicalizing input. Port is
  // optional and, if not provided, will be replaced with the default port for
  // the given scheme. Accepts IPv6 literal `host`s with surrounding brackets
  // (URL format) or without (HostPortPair format). On invalid input, result
  // will be a `SCHEME_INVALID` ProxyServer.
  //
  // Must not be called with `SCHEME_INVALID`. Use `ProxyServer()` to create an
  // invalid ProxyServer.
  static ProxyServer FromSchemeHostAndPort(Scheme scheme,
                                           std::string_view host,
                                           std::string_view port_str);
  static ProxyServer FromSchemeHostAndPort(Scheme scheme,
                                           std::string_view host,
                                           std::optional<uint16_t> port);

  static ProxyServer CreateFromPickle(base::PickleIterator* pickle_iter);

  void Persist(base::Pickle* pickle) const;

  // In URL format (with brackets around IPv6 literals). Must not call for
  // invalid ProxyServers.
  std::string GetHost() const;

  // Must not call for invalid ProxyServers.
  uint16_t GetPort() const;

  bool is_valid() const { return scheme_ != SCHEME_INVALID; }

  // Gets the proxy's scheme (i.e. SOCKS4, SOCKS5, HTTP)
  Scheme scheme() const { return scheme_; }

  // Returns true if this ProxyServer is an HTTP proxy.
  bool is_http() const { return scheme_ == SCHEME_HTTP; }

  // Returns true if this ProxyServer is an HTTPS proxy. Note this
  // does not include proxies matched by |is_quic()|.
  //
  // Generally one should test the more general concept of
  // |is_secure_http_like()| to account for |is_quic()|.
  bool is_https() const { return scheme_ == SCHEME_HTTPS; }

  // Returns true if this ProxyServer is a SOCKS proxy.
  bool is_socks() const {
    return scheme_ == SCHEME_SOCKS4 || scheme_ == SCHEME_SOCKS5;
  }

  // Returns true if this ProxyServer is a QUIC proxy.
  bool is_quic() const { return scheme_ == SCHEME_QUIC; }

  // Returns true if the ProxyServer's scheme is HTTP compatible (uses HTTP
  // headers, has a CONNECT method for establishing tunnels).
  bool is_http_like() const { return is_http() || is_https() || is_quic(); }

  // Returns true if the proxy server has HTTP semantics, AND
  // the channel between the client and proxy server is secure.
  bool is_secure_http_like() const { return is_https() || is_quic(); }

  const HostPortPair& host_port_pair() const;

  // Returns the default port number for a proxy server with the specified
  // scheme. Returns -1 if unknown.
  static int GetDefaultPortForScheme(Scheme scheme);

  bool operator==(const ProxyServer& other) const {
    return scheme_ == other.scheme_ &&
           host_port_pair_.Equals(other.host_port_pair_);
  }

  bool operator!=(const ProxyServer& other) const { return !(*this == other); }

  // Comparator function so this can be placed in a std::map.
  bool operator<(const ProxyServer& other) const {
    return std::tie(scheme_, host_port_pair_) <
           std::tie(other.scheme_, other.host_port_pair_);
  }

 private:
  Scheme scheme_ = SCHEME_INVALID;
  HostPortPair host_port_pair_;
};

NET_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                            const ProxyServer& proxy_server);

}  // namespace net

#endif  // NET_BASE_PROXY_SERVER_H_
