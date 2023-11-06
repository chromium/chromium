// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/proxy_string_util.h"

#include <string>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/base/proxy_server.h"
#include "net/base/url_util.h"
#include "net/http/http_util.h"
#include "url/third_party/mozilla/url_parse.h"

namespace net {

namespace {

// Parses the proxy type from a PAC string, to a ProxyServer::Scheme.
// This mapping is case-insensitive. If no type could be matched
// returns SCHEME_INVALID.
ProxyServer::Scheme GetSchemeFromPacTypeInternal(base::StringPiece type) {
  if (base::EqualsCaseInsensitiveASCII(type, "proxy"))
    return ProxyServer::SCHEME_HTTP;
  if (base::EqualsCaseInsensitiveASCII(type, "socks")) {
    // Default to v4 for compatibility. This is because the SOCKS4 vs SOCKS5
    // notation didn't originally exist, so if a client returns SOCKS they
    // really meant SOCKS4.
    return ProxyServer::SCHEME_SOCKS4;
  }
  if (base::EqualsCaseInsensitiveASCII(type, "socks4"))
    return ProxyServer::SCHEME_SOCKS4;
  if (base::EqualsCaseInsensitiveASCII(type, "socks5"))
    return ProxyServer::SCHEME_SOCKS5;
  if (base::EqualsCaseInsensitiveASCII(type, "direct"))
    return ProxyServer::SCHEME_DIRECT;
  if (base::EqualsCaseInsensitiveASCII(type, "https"))
    return ProxyServer::SCHEME_HTTPS;
  if (base::EqualsCaseInsensitiveASCII(type, "quic"))
    return ProxyServer::SCHEME_QUIC;

  return ProxyServer::SCHEME_INVALID;
}

ProxyServer FromSchemeHostAndPort(ProxyServer::Scheme scheme,
                                  base::StringPiece host_and_port) {
  // Trim leading/trailing space.
  host_and_port = HttpUtil::TrimLWS(host_and_port);

  if (scheme == ProxyServer::SCHEME_INVALID)
    return ProxyServer();

  if (scheme == ProxyServer::SCHEME_DIRECT) {
    if (!host_and_port.empty())
      return ProxyServer();  // Invalid -- DIRECT cannot have a host/port.
    return ProxyServer::Direct();
  }

  url::Component username_component;
  url::Component password_component;
  url::Component hostname_component;
  url::Component port_component;
  url::ParseAuthority(host_and_port.data(),
                      url::Component(0, host_and_port.size()),
                      &username_component, &password_component,
                      &hostname_component, &port_component);
  if (username_component.is_valid() || password_component.is_valid() ||
      hostname_component.is_empty()) {
    return ProxyServer();
  }

  base::StringPiece hostname =
      host_and_port.substr(hostname_component.begin, hostname_component.len);

  // Reject inputs like "foo:". /url parsing and canonicalization code generally
  // allows it and treats it the same as a URL without a specified port, but
  // Chrome has traditionally disallowed it in proxy specifications.
  if (port_component.is_valid() && port_component.is_empty())
    return ProxyServer();
  base::StringPiece port =
      port_component.is_nonempty()
          ? host_and_port.substr(port_component.begin, port_component.len)
          : "";

  return ProxyServer::FromSchemeHostAndPort(scheme, hostname, port);
}

std::string ConstructHostPortString(base::StringPiece hostname, uint16_t port) {
  DCHECK(!hostname.empty());
  DCHECK((hostname.front() == '[' && hostname.back() == ']') ||
         hostname.find(":") == base::StringPiece::npos);

  return base::StrCat({hostname, ":", base::NumberToString(port)});
}

}  // namespace

ProxyChain PacResultElementToProxyChain(base::StringPiece pac_result_element) {
  // TODO(https://crbug.com/1491092): Support parsing multi-hop proxy chains
  // from PAC scripts.
  return ProxyChain(PacResultElementToProxyServer(pac_result_element));
}

ProxyServer PacResultElementToProxyServer(
    base::StringPiece pac_result_element) {
  // Trim the leading/trailing whitespace.
  pac_result_element = HttpUtil::TrimLWS(pac_result_element);

  // Input should match:
  // "DIRECT" | ( <type> 1*(LWS) <host-and-port> )

  // Start by finding the first space (if any).
  size_t space = 0;
  for (; space < pac_result_element.size(); space++) {
    if (HttpUtil::IsLWS(pac_result_element[space])) {
      break;
    }
  }

  // Everything to the left of the space is the scheme.
  ProxyServer::Scheme scheme =
      GetSchemeFromPacTypeInternal(pac_result_element.substr(0, space));

  // And everything to the right of the space is the
  // <host>[":" <port>].
  return FromSchemeHostAndPort(scheme, pac_result_element.substr(space));
}

std::string ProxyChainToPacResultElement(const ProxyChain& proxy_chain) {
  // TODO(https://crbug.com/1491092): Support converting a multi-hop ProxyChain
  // to a PAC script format.
  CHECK(!proxy_chain.is_multi_proxy());
  return ProxyServerToPacResultElement(proxy_chain.proxy_server());
}

std::string ProxyServerToPacResultElement(const ProxyServer& proxy_server) {
  switch (proxy_server.scheme()) {
    case ProxyServer::SCHEME_DIRECT:
      return "DIRECT";
    case ProxyServer::SCHEME_HTTP:
      return std::string("PROXY ") +
             ConstructHostPortString(proxy_server.GetHost(),
                                     proxy_server.GetPort());
    case ProxyServer::SCHEME_SOCKS4:
      // For compatibility send SOCKS instead of SOCKS4.
      return std::string("SOCKS ") +
             ConstructHostPortString(proxy_server.GetHost(),
                                     proxy_server.GetPort());
    case ProxyServer::SCHEME_SOCKS5:
      return std::string("SOCKS5 ") +
             ConstructHostPortString(proxy_server.GetHost(),
                                     proxy_server.GetPort());
    case ProxyServer::SCHEME_HTTPS:
      return std::string("HTTPS ") +
             ConstructHostPortString(proxy_server.GetHost(),
                                     proxy_server.GetPort());
    case ProxyServer::SCHEME_QUIC:
      return std::string("QUIC ") +
             ConstructHostPortString(proxy_server.GetHost(),
                                     proxy_server.GetPort());
    default:
      // Got called with an invalid scheme.
      NOTREACHED();
      return std::string();
  }
}

ProxyChain ProxyUriToProxyChain(base::StringPiece uri,
                                ProxyServer::Scheme default_scheme) {
  return ProxyChain(ProxyUriToProxyServer(uri, default_scheme));
}

ProxyServer ProxyUriToProxyServer(base::StringPiece uri,
                                  ProxyServer::Scheme default_scheme) {
  // We will default to |default_scheme| if no scheme specifier was given.
  ProxyServer::Scheme scheme = default_scheme;

  // Trim the leading/trailing whitespace.
  uri = HttpUtil::TrimLWS(uri);

  // Check for [<scheme> "://"]
  size_t colon = uri.find(':');
  if (colon != base::StringPiece::npos && uri.size() - colon >= 3 &&
      uri[colon + 1] == '/' && uri[colon + 2] == '/') {
    scheme = GetSchemeFromUriScheme(uri.substr(0, colon));
    uri = uri.substr(colon + 3);  // Skip past the "://"
  }

  // Now parse the <host>[":"<port>].
  return FromSchemeHostAndPort(scheme, uri);
}

std::string ProxyServerToProxyUri(const ProxyServer& proxy_server) {
  switch (proxy_server.scheme()) {
    case ProxyServer::SCHEME_DIRECT:
      return "direct://";
    case ProxyServer::SCHEME_HTTP:
      // Leave off "http://" since it is our default scheme.
      return ConstructHostPortString(proxy_server.GetHost(),
                                     proxy_server.GetPort());
    case ProxyServer::SCHEME_SOCKS4:
      return std::string("socks4://") +
             ConstructHostPortString(proxy_server.GetHost(),
                                     proxy_server.GetPort());
    case ProxyServer::SCHEME_SOCKS5:
      return std::string("socks5://") +
             ConstructHostPortString(proxy_server.GetHost(),
                                     proxy_server.GetPort());
    case ProxyServer::SCHEME_HTTPS:
      return std::string("https://") +
             ConstructHostPortString(proxy_server.GetHost(),
                                     proxy_server.GetPort());
    case ProxyServer::SCHEME_QUIC:
      return std::string("quic://") +
             ConstructHostPortString(proxy_server.GetHost(),
                                     proxy_server.GetPort());
    default:
      // Got called with an invalid scheme.
      NOTREACHED();
      return std::string();
  }
}

ProxyServer::Scheme GetSchemeFromUriScheme(base::StringPiece scheme) {
  if (base::EqualsCaseInsensitiveASCII(scheme, "http"))
    return ProxyServer::SCHEME_HTTP;
  if (base::EqualsCaseInsensitiveASCII(scheme, "socks4"))
    return ProxyServer::SCHEME_SOCKS4;
  if (base::EqualsCaseInsensitiveASCII(scheme, "socks"))
    return ProxyServer::SCHEME_SOCKS5;
  if (base::EqualsCaseInsensitiveASCII(scheme, "socks5"))
    return ProxyServer::SCHEME_SOCKS5;
  if (base::EqualsCaseInsensitiveASCII(scheme, "direct"))
    return ProxyServer::SCHEME_DIRECT;
  if (base::EqualsCaseInsensitiveASCII(scheme, "https"))
    return ProxyServer::SCHEME_HTTPS;
  if (base::EqualsCaseInsensitiveASCII(scheme, "quic"))
    return ProxyServer::SCHEME_QUIC;
  return ProxyServer::SCHEME_INVALID;
}

}  // namespace net
