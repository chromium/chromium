// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PROXY_STRING_UTIL_H_
#define NET_BASE_PROXY_STRING_UTIL_H_

#include <string>
#include <string_view>

#include "net/base/net_export.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"

namespace net {

// Converts a PAC result element (commonly called a PAC string) to/from a
// ProxyServer / ProxyChain. Note that this only deals with a single proxy
// element separated out from the complete semicolon-delimited PAC result
// string.
//
// Note that PAC strings cannot currently specify multi-proxy chains.
//
// PAC result elements have the format:
// <scheme>" "<host>[":"<port>]
//
// Where <scheme> may be one of (case-insensitive):
// "DIRECT"
// "PROXY"
// "HTTPS"
// "SOCKS4"
// "SOCKS5"
// "SOCKS" (canonicalizes to "SOCKS4")
// "QUIC"
//
// If <port> is omitted, it will be assumed as the default port for the
// chosen scheme (via ProxyServer::GetDefaultPortForScheme()).
//
// Returns an invalid ProxyServer / ProxyChain if parsing fails.
//
// Examples:
//   "PROXY foopy:19"   {scheme=HTTP, host="foopy", port=19}
//   "DIRECT"           {scheme=DIRECT}
//   "SOCKS5 foopy"     {scheme=SOCKS5, host="foopy", port=1080}
//   "HTTPS foopy:123"  {scheme=HTTPS, host="foopy", port=123}
//   "QUIC foopy:123"   {scheme=QUIC, host="foopy", port=123}
//   "BLAH xxx:xx"      INVALID
NET_EXPORT ProxyChain
PacResultElementToProxyChain(std::string_view pac_result_element);
// TODO(crbug.com/40284947): Remove method once all calls are updated to use
// PacResultElementToProxyChain.
NET_EXPORT ProxyServer
PacResultElementToProxyServer(std::string_view pac_result_element);
NET_EXPORT std::string ProxyServerToPacResultElement(
    const ProxyServer& proxy_server);

// Converts a non-standard URI string to/from a ProxyChain.
//
// The non-standard URI strings have the format:
//   [<scheme>"://"]<server>[":"<port>]
//
// Where <scheme> may be one of:
// "http"
// "socks4"
// "socks5
// "socks" (equivalent to "socks5")
// "direct"
// "https"
// "quic"
//
// Both <scheme> and <port> are optional. If <scheme> is omitted, it will be
// assumed as |default_scheme|. If <port> is omitted, it will be assumed as
// the default port for the chosen scheme (via
// ProxyServer::GetDefaultPortForScheme()).
//
// If parsing fails the returned proxy will have scheme
// `ProxyServer::SCHEME_INVALID`.
//
// Examples (for `default_pac_scheme` = `kHttp` ):
//   "foopy"            {scheme=HTTP, host="foopy", port=80}
//   "socks://foopy"    {scheme=SOCKS5, host="foopy", port=1080}
//   "socks4://foopy"   {scheme=SOCKS4, host="foopy", port=1080}
//   "socks5://foopy"   {scheme=SOCKS5, host="foopy", port=1080}
//   "http://foopy:17"  {scheme=HTTP, host="foopy", port=17}
//   "https://foopy:17" {scheme=HTTPS, host="foopy", port=17}
//   "quic://foopy:17"  {scheme=QUIC, host="foopy", port=17}
//   "direct://"        {scheme=DIRECT}
//   "foopy:X"          INVALID -- bad port.
NET_EXPORT ProxyChain ProxyUriToProxyChain(std::string_view uri,
                                           ProxyServer::Scheme default_scheme,
                                           bool is_quic_allowed = false);

// Converts a bracketed string of non-standard uris to a multi-proxy
// `net::ProxyChain`.
//
// The `uris` parameter may contain 1 or more non-standard URIs but not 0 which
// would result in an invalid `ProxyChain()`.
//
// If brackets are omitted from the `uris` string, it MUST be a single
// non-standard URI. Otherwise, an invalid `ProxyChain()` will be returned.
//
//
// The bracketed non-standard URIs strings have the format:
//   [x y z] where individual non-standard uris are space delimited and
//   encompassed within brackets.
//   ex. [https://foopy:17 https://hoopy:17]
//
// Each non-standard URI string follows the format described in the
// documentation for the `ProxyUriToProxyChain` function.
NET_EXPORT ProxyChain
MultiProxyUrisToProxyChain(std::string_view uris,
                           ProxyServer::Scheme default_scheme,
                           bool is_quic_allowed = false);
NET_EXPORT ProxyServer ProxyUriToProxyServer(std::string_view uri,
                                             ProxyServer::Scheme default_scheme,
                                             bool is_quic_allowed = false);
NET_EXPORT std::string ProxyServerToProxyUri(const ProxyServer& proxy_server);
NET_EXPORT ProxyServer
ProxySchemeHostAndPortToProxyServer(ProxyServer::Scheme scheme,
                                    std::string_view host_and_port);

// Parses the proxy scheme from the non-standard URI scheme string
// representation used in `ProxyUriToProxyServer()` and
// `ProxyServerToProxyUri()`. If no type could be matched, returns
// SCHEME_INVALID.
NET_EXPORT ProxyServer::Scheme GetSchemeFromUriScheme(
    std::string_view scheme,
    bool is_quic_allowed = false);

}  // namespace net

#endif  // NET_BASE_PROXY_STRING_UTIL_H_
