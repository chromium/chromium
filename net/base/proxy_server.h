// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PROXY_SERVER_H_
#define NET_BASE_PROXY_SERVER_H_

#include "build/build_config.h"

#if defined(OS_APPLE)
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <string>
#include <tuple>

#include "base/strings/string_piece.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"

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
    SCHEME_DIRECT  = 1 << 1,
    SCHEME_HTTP    = 1 << 2,
    SCHEME_SOCKS4  = 1 << 3,
    SCHEME_SOCKS5  = 1 << 4,
    SCHEME_HTTPS   = 1 << 5,
    // A QUIC proxy is an HTTP proxy in which QUIC is used as the transport,
    // instead of TCP.
    SCHEME_QUIC    = 1 << 6,
  };

  // Default copy-constructor and assignment operator are OK!

  // Constructs an invalid ProxyServer.
  ProxyServer() {}

  ProxyServer(Scheme scheme, const HostPortPair& host_port_pair);

  bool is_valid() const { return scheme_ != SCHEME_INVALID; }

  // Gets the proxy's scheme (i.e. SOCKS4, SOCKS5, HTTP)
  Scheme scheme() const { return scheme_; }

  // Returns true if this ProxyServer is actually just a DIRECT connection.
  bool is_direct() const { return scheme_ == SCHEME_DIRECT; }

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

  // Parses from an input with format:
  //   [<scheme>"://"]<server>[":"<port>]
  //
  // Both <scheme> and <port> are optional. If <scheme> is omitted, it will be
  // assumed as |default_scheme|. If <port> is omitted, it will be assumed as
  // the default port for the chosen scheme (80 for "http", 1080 for "socks").
  //
  // If parsing fails the instance will be set to invalid.
  //
  // Examples (for |default_scheme| = SCHEME_HTTP ):
  //   "foopy"            {scheme=HTTP, host="foopy", port=80}
  //   "socks://foopy"    {scheme=SOCKS5, host="foopy", port=1080}
  //   "socks4://foopy"   {scheme=SOCKS4, host="foopy", port=1080}
  //   "socks5://foopy"   {scheme=SOCKS5, host="foopy", port=1080}
  //   "http://foopy:17"  {scheme=HTTP, host="foopy", port=17}
  //   "https://foopy:17" {scheme=HTTPS, host="foopy", port=17}
  //   "quic://foopy:17"  {scheme=QUIC, host="foopy", port=17}
  //   "direct://"        {scheme=DIRECT}
  //   "foopy:X"          INVALID -- bad port.
  static ProxyServer FromURI(base::StringPiece uri, Scheme default_scheme);

  // Formats as a URI string. This does the reverse of FromURI.
  std::string ToURI() const;

  // Parses from a PAC string result.
  //
  // If <port> is omitted, it will be assumed as the default port for the
  // chosen scheme (80 for "http", 1080 for "socks").
  //
  // If parsing fails the instance will be set to invalid.
  //
  // Examples:
  //   "PROXY foopy:19"   {scheme=HTTP, host="foopy", port=19}
  //   "DIRECT"           {scheme=DIRECT}
  //   "SOCKS5 foopy"     {scheme=SOCKS5, host="foopy", port=1080}
  //   "HTTPS foopy:123"  {scheme=HTTPS, host="foopy", port=123}
  //   "QUIC foopy:123"   {scheme=QUIC, host="foopy", port=123}
  //   "BLAH xxx:xx"      INVALID
  static ProxyServer FromPacString(base::StringPiece pac_string);

  // Returns a ProxyServer representing DIRECT connections.
  static ProxyServer Direct() {
    return ProxyServer(SCHEME_DIRECT, HostPortPair());
  }

#if defined(OS_APPLE)
  // Utility function to pull out a host/port pair from a dictionary and return
  // it as a ProxyServer object. Pass in a dictionary that has a  value for the
  // host key and optionally a value for the port key. In the error condition
  // where the host value is especially malformed, returns an invalid
  // ProxyServer.
  static ProxyServer FromDictionary(Scheme scheme,
                                    CFDictionaryRef dict,
                                    CFStringRef host_key,
                                    CFStringRef port_key);
#endif

  // Formats as a PAC result entry. This does the reverse of FromPacString().
  std::string ToPacString() const;

  // Returns the default port number for a proxy server with the specified
  // scheme. Returns -1 if unknown.
  static int GetDefaultPortForScheme(Scheme scheme);

  // Parses the proxy scheme from a URL-like representation, to a
  // ProxyServer::Scheme. This corresponds with the values used in
  // ProxyServer::ToURI(). If no type could be matched, returns SCHEME_INVALID.
  // |scheme| can be one of http, https, socks, socks4, socks5, direct.
  static Scheme GetSchemeFromURI(const std::string& scheme);

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

  // Returns the estimate of dynamically allocated memory in bytes.
  size_t EstimateMemoryUsage() const;

 private:
  // Creates a ProxyServer given a scheme, and host/port string. If parsing the
  // host/port string fails, the returned instance will be invalid.
  static ProxyServer FromSchemeHostAndPort(Scheme scheme,
                                           base::StringPiece host_and_port);

  Scheme scheme_ = SCHEME_INVALID;
  HostPortPair host_port_pair_;
};

typedef std::pair<HostPortPair, ProxyServer> HostPortProxyPair;

}  // namespace net

#endif  // NET_BASE_PROXY_SERVER_H_
