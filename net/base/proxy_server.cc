// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/proxy_server.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "net/base/url_util.h"
#include "net/http/http_util.h"

namespace net {

namespace {

// Parses the proxy type from a PAC string, to a ProxyServer::Scheme.
// This mapping is case-insensitive. If no type could be matched
// returns SCHEME_INVALID.
ProxyServer::Scheme GetSchemeFromPacTypeInternal(base::StringPiece type) {
  if (base::LowerCaseEqualsASCII(type, "proxy"))
    return ProxyServer::SCHEME_HTTP;
  if (base::LowerCaseEqualsASCII(type, "socks")) {
    // Default to v4 for compatibility. This is because the SOCKS4 vs SOCKS5
    // notation didn't originally exist, so if a client returns SOCKS they
    // really meant SOCKS4.
    return ProxyServer::SCHEME_SOCKS4;
  }
  if (base::LowerCaseEqualsASCII(type, "socks4"))
    return ProxyServer::SCHEME_SOCKS4;
  if (base::LowerCaseEqualsASCII(type, "socks5"))
    return ProxyServer::SCHEME_SOCKS5;
  if (base::LowerCaseEqualsASCII(type, "direct"))
    return ProxyServer::SCHEME_DIRECT;
  if (base::LowerCaseEqualsASCII(type, "https"))
    return ProxyServer::SCHEME_HTTPS;
  if (base::LowerCaseEqualsASCII(type, "quic"))
    return ProxyServer::SCHEME_QUIC;

  return ProxyServer::SCHEME_INVALID;
}

// Parses the proxy scheme from a URL-like representation, to a
// ProxyServer::Scheme. This corresponds with the values used in
// ProxyServer::ToURI(). If no type could be matched, returns SCHEME_INVALID.
ProxyServer::Scheme GetSchemeFromURIInternal(base::StringPiece type) {
  if (base::LowerCaseEqualsASCII(type, "http"))
    return ProxyServer::SCHEME_HTTP;
  if (base::LowerCaseEqualsASCII(type, "socks4"))
    return ProxyServer::SCHEME_SOCKS4;
  if (base::LowerCaseEqualsASCII(type, "socks"))
    return ProxyServer::SCHEME_SOCKS5;
  if (base::LowerCaseEqualsASCII(type, "socks5"))
    return ProxyServer::SCHEME_SOCKS5;
  if (base::LowerCaseEqualsASCII(type, "direct"))
    return ProxyServer::SCHEME_DIRECT;
  if (base::LowerCaseEqualsASCII(type, "https"))
    return ProxyServer::SCHEME_HTTPS;
  if (base::LowerCaseEqualsASCII(type, "quic"))
    return ProxyServer::SCHEME_QUIC;
  return ProxyServer::SCHEME_INVALID;
}

}  // namespace

ProxyServer::ProxyServer(Scheme scheme, const HostPortPair& host_port_pair)
      : scheme_(scheme), host_port_pair_(host_port_pair) {
  if (scheme_ == SCHEME_DIRECT || scheme_ == SCHEME_INVALID) {
    // |host_port_pair| isn't relevant for these special schemes, so none should
    // have been specified. It is important for this to be consistent since we
    // do raw field comparisons in the equality and comparison functions.
    DCHECK(host_port_pair.Equals(HostPortPair()));
    host_port_pair_ = HostPortPair();
  }
}

const HostPortPair& ProxyServer::host_port_pair() const {
  // Doesn't make sense to call this if the URI scheme doesn't
  // have concept of a host.
  DCHECK(is_valid());
  DCHECK(!is_direct());
  return host_port_pair_;
}

// static
ProxyServer ProxyServer::FromURI(base::StringPiece uri, Scheme default_scheme) {
  // We will default to |default_scheme| if no scheme specifier was given.
  Scheme scheme = default_scheme;

  // Trim the leading/trailing whitespace.
  uri = HttpUtil::TrimLWS(uri);

  // Check for [<scheme> "://"]
  size_t colon = uri.find(':');
  if (colon != base::StringPiece::npos && uri.size() - colon >= 3 &&
      uri[colon + 1] == '/' && uri[colon + 2] == '/') {
    scheme = GetSchemeFromURIInternal(uri.substr(0, colon));
    uri = uri.substr(colon + 3);  // Skip past the "://"
  }

  // Now parse the <host>[":"<port>].
  return FromSchemeHostAndPort(scheme, uri);
}

std::string ProxyServer::ToURI() const {
  switch (scheme_) {
    case SCHEME_DIRECT:
      return "direct://";
    case SCHEME_HTTP:
      // Leave off "http://" since it is our default scheme.
      return host_port_pair().ToString();
    case SCHEME_SOCKS4:
      return std::string("socks4://") + host_port_pair().ToString();
    case SCHEME_SOCKS5:
      return std::string("socks5://") + host_port_pair().ToString();
    case SCHEME_HTTPS:
      return std::string("https://") + host_port_pair().ToString();
    case SCHEME_QUIC:
      return std::string("quic://") + host_port_pair().ToString();
    default:
      // Got called with an invalid scheme.
      NOTREACHED();
      return std::string();
  }
}

// static
ProxyServer ProxyServer::FromPacString(base::StringPiece pac_string) {
  // Trim the leading/trailing whitespace.
  pac_string = HttpUtil::TrimLWS(pac_string);

  // Input should match:
  // "DIRECT" | ( <type> 1*(LWS) <host-and-port> )

  // Start by finding the first space (if any).
  size_t space = 0;
  for (; space < pac_string.size(); space++) {
    if (HttpUtil::IsLWS(pac_string[space])) {
      break;
    }
  }

  // Everything to the left of the space is the scheme.
  Scheme scheme = GetSchemeFromPacTypeInternal(pac_string.substr(0, space));

  // And everything to the right of the space is the
  // <host>[":" <port>].
  return FromSchemeHostAndPort(scheme, pac_string.substr(space));
}

std::string ProxyServer::ToPacString() const {
  switch (scheme_) {
    case SCHEME_DIRECT:
      return "DIRECT";
    case SCHEME_HTTP:
      return std::string("PROXY ") + host_port_pair().ToString();
    case SCHEME_SOCKS4:
      // For compatibility send SOCKS instead of SOCKS4.
      return std::string("SOCKS ") + host_port_pair().ToString();
    case SCHEME_SOCKS5:
      return std::string("SOCKS5 ") + host_port_pair().ToString();
    case SCHEME_HTTPS:
      return std::string("HTTPS ") + host_port_pair().ToString();
    case SCHEME_QUIC:
      return std::string("QUIC ") + host_port_pair().ToString();
    default:
      // Got called with an invalid scheme.
      NOTREACHED();
      return std::string();
  }
}

// static
int ProxyServer::GetDefaultPortForScheme(Scheme scheme) {
  switch (scheme) {
    case SCHEME_HTTP:
      return 80;
    case SCHEME_SOCKS4:
    case SCHEME_SOCKS5:
      return 1080;
    case SCHEME_HTTPS:
    case SCHEME_QUIC:
      return 443;
    case SCHEME_INVALID:
    case SCHEME_DIRECT:
      break;
  }
  return -1;
}

// static
ProxyServer::Scheme ProxyServer::GetSchemeFromURI(const std::string& scheme) {
  return GetSchemeFromURIInternal(scheme);
}

size_t ProxyServer::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(host_port_pair_);
}

// static
ProxyServer ProxyServer::FromSchemeHostAndPort(
    Scheme scheme,
    base::StringPiece host_and_port) {
  // Trim leading/trailing space.
  host_and_port = HttpUtil::TrimLWS(host_and_port);

  if (scheme == SCHEME_DIRECT && !host_and_port.empty())
    return ProxyServer();  // Invalid -- DIRECT cannot have a host/port.

  HostPortPair host_port_pair;

  if (scheme != SCHEME_INVALID && scheme != SCHEME_DIRECT) {
    std::string host;
    int port = -1;
    // If the scheme has a host/port, parse it.
    bool ok = ParseHostAndPort(host_and_port, &host, &port);
    if (!ok)
      return ProxyServer();  // Invalid -- failed parsing <host>[":"<port>]

    // Choose a default port number if none was given.
    if (port == -1)
      port = GetDefaultPortForScheme(scheme);

    host_port_pair = HostPortPair(host, static_cast<uint16_t>(port));
  }

  return ProxyServer(scheme, host_port_pair);
}

}  // namespace net
