// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/proxy_string_util.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/buildflag.h"
#include "net/base/proxy_server.h"
#include "net/base/url_util.h"
#include "net/http/http_util.h"
#include "net/net_buildflags.h"
#include "url/third_party/mozilla/url_parse.h"

namespace net {

namespace {

// Parses the proxy type from a PAC string, to a ProxyServer::Scheme.
// This mapping is case-insensitive. If no type could be matched
// returns SCHEME_INVALID.
ProxyServer::Scheme GetSchemeFromPacTypeInternal(std::string_view type) {
  if (base::EqualsCaseInsensitiveASCII(type, "proxy")) {
    return ProxyServer::SCHEME_HTTP;
  }
  if (base::EqualsCaseInsensitiveASCII(type, "socks")) {
    // Default to v4 for compatibility. This is because the SOCKS4 vs SOCKS5
    // notation didn't originally exist, so if a client returns SOCKS they
    // really meant SOCKS4.
    return ProxyServer::SCHEME_SOCKS4;
  }
  if (base::EqualsCaseInsensitiveASCII(type, "socks4")) {
    return ProxyServer::SCHEME_SOCKS4;
  }
  if (base::EqualsCaseInsensitiveASCII(type, "socks5")) {
    return ProxyServer::SCHEME_SOCKS5;
  }
  if (base::EqualsCaseInsensitiveASCII(type, "https")) {
    return ProxyServer::SCHEME_HTTPS;
  }

  return ProxyServer::SCHEME_INVALID;
}

std::string ConstructHostPortString(std::string_view hostname, uint16_t port) {
  DCHECK(!hostname.empty());
  DCHECK((hostname.front() == '[' && hostname.back() == ']') ||
         hostname.find(":") == std::string_view::npos);

  return base::StrCat({hostname, ":", base::NumberToString(port)});
}

std::tuple<std::string_view, std::string_view>
PacResultElementToSchemeAndHostPort(std::string_view pac_result_element) {
  // Trim the leading/trailing whitespace.
  pac_result_element = HttpUtil::TrimLWS(pac_result_element);

  // Input should match:
  // ( <type> 1*(LWS) <host-and-port> )

  // Start by finding the first space (if any).
  size_t space = 0;
  for (; space < pac_result_element.size(); space++) {
    if (HttpUtil::IsLWS(pac_result_element[space])) {
      break;
    }
  }
  // Everything to the left of the space is the scheme.
  std::string_view scheme = pac_result_element.substr(0, space);

  // And everything to the right of the space is the
  // <host>[":" <port>].
  std::string_view host_and_port = pac_result_element.substr(space);
  return std::make_tuple(scheme, host_and_port);
}

}  // namespace

ProxyChain PacResultElementToProxyChain(std::string_view pac_result_element) {
  // Proxy chains are not supported in PAC strings, so this is just parsed
  // as a single server.
  auto [type, host_and_port] =
      PacResultElementToSchemeAndHostPort(pac_result_element);
  if (base::EqualsCaseInsensitiveASCII(type, "direct") &&
      host_and_port.empty()) {
    return ProxyChain::Direct();
  }
  return ProxyChain(PacResultElementToProxyServer(pac_result_element));
}

ProxyServer PacResultElementToProxyServer(std::string_view pac_result_element) {
  auto [type, host_and_port] =
      PacResultElementToSchemeAndHostPort(pac_result_element);
  ProxyServer::Scheme scheme = GetSchemeFromPacTypeInternal(type);
  return ProxySchemeHostAndPortToProxyServer(scheme, host_and_port);
}

std::string ProxyServerToPacResultElement(const ProxyServer& proxy_server) {
  switch (proxy_server.scheme()) {
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
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

ProxyChain ProxyUriToProxyChain(std::string_view uri,
                                ProxyServer::Scheme default_scheme,
                                bool is_quic_allowed) {
  // If uri is direct, return direct proxy chain.
  uri = HttpUtil::TrimLWS(uri);
  size_t colon = uri.find("://");
  if (colon != std::string_view::npos &&
      base::EqualsCaseInsensitiveASCII(uri.substr(0, colon), "direct")) {
    if (!uri.substr(colon + 3).empty()) {
      return ProxyChain();  // Invalid -- Direct chain cannot have a host/port.
    }
    return ProxyChain::Direct();
  }
  return ProxyChain(
      ProxyUriToProxyServer(uri, default_scheme, is_quic_allowed));
}

ProxyServer ProxyUriToProxyServer(std::string_view uri,
                                  ProxyServer::Scheme default_scheme,
                                  bool is_quic_allowed) {
  // We will default to |default_scheme| if no scheme specifier was given.
  ProxyServer::Scheme scheme = default_scheme;

  // Trim the leading/trailing whitespace.
  uri = HttpUtil::TrimLWS(uri);

  // Check for [<scheme> "://"]
  size_t colon = uri.find(':');
  if (colon != std::string_view::npos && uri.size() - colon >= 3 &&
      uri[colon + 1] == '/' && uri[colon + 2] == '/') {
    scheme = GetSchemeFromUriScheme(uri.substr(0, colon), is_quic_allowed);
    uri = uri.substr(colon + 3);  // Skip past the "://"
  }

  // Now parse the <host>[":"<port>].
  return ProxySchemeHostAndPortToProxyServer(scheme, uri);
}

std::string ProxyServerToProxyUri(const ProxyServer& proxy_server) {
  switch (proxy_server.scheme()) {
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
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }
}

ProxyServer ProxySchemeHostAndPortToProxyServer(
    ProxyServer::Scheme scheme,
    std::string_view host_and_port) {
  // Trim leading/trailing space.
  host_and_port = HttpUtil::TrimLWS(host_and_port);

  if (scheme == ProxyServer::SCHEME_INVALID) {
    return ProxyServer();
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

  std::string_view hostname =
      host_and_port.substr(hostname_component.begin, hostname_component.len);

  // Reject inputs like "foo:". /url parsing and canonicalization code generally
  // allows it and treats it the same as a URL without a specified port, but
  // Chrome has traditionally disallowed it in proxy specifications.
  if (port_component.is_valid() && port_component.is_empty()) {
    return ProxyServer();
  }
  std::string_view port =
      port_component.is_nonempty()
          ? host_and_port.substr(port_component.begin, port_component.len)
          : "";

  return ProxyServer::FromSchemeHostAndPort(scheme, hostname, port);
}

ProxyServer::Scheme GetSchemeFromUriScheme(std::string_view scheme,
                                           bool is_quic_allowed) {
  if (base::EqualsCaseInsensitiveASCII(scheme, "http")) {
    return ProxyServer::SCHEME_HTTP;
  }
  if (base::EqualsCaseInsensitiveASCII(scheme, "socks4")) {
    return ProxyServer::SCHEME_SOCKS4;
  }
  if (base::EqualsCaseInsensitiveASCII(scheme, "socks")) {
    return ProxyServer::SCHEME_SOCKS5;
  }
  if (base::EqualsCaseInsensitiveASCII(scheme, "socks5")) {
    return ProxyServer::SCHEME_SOCKS5;
  }
  if (base::EqualsCaseInsensitiveASCII(scheme, "https")) {
    return ProxyServer::SCHEME_HTTPS;
  }
#if BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
  if (is_quic_allowed && base::EqualsCaseInsensitiveASCII(scheme, "quic")) {
    return ProxyServer::SCHEME_QUIC;
  }
#endif  // BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
  return ProxyServer::SCHEME_INVALID;
}

ProxyChain MultiProxyUrisToProxyChain(std::string_view uris,
                                      ProxyServer::Scheme default_scheme,
                                      bool is_quic_allowed) {
#if !BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
  // This function should not be called in non-debug modes.
  CHECK(false);
#endif  // !BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)

  uris = HttpUtil::TrimLWS(uris);
  if (uris.empty()) {
    return ProxyChain();
  }

  bool has_multi_proxy_brackets = uris.front() == '[' && uris.back() == ']';
  // Remove `[]` if present
  if (has_multi_proxy_brackets) {
    uris = HttpUtil::TrimLWS(uris.substr(1, uris.size() - 2));
  }

  std::vector<ProxyServer> proxy_server_list;
  std::vector<std::string_view> uris_list = base::SplitStringPiece(
      uris, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  size_t number_of_proxy_uris = uris_list.size();
  bool has_invalid_format =
      number_of_proxy_uris > 1 && !has_multi_proxy_brackets;

  // If uris list is empty or has invalid formatting for multi-proxy chains, an
  // invalid `ProxyChain` should be returned.
  if (uris_list.empty() || has_invalid_format) {
    return ProxyChain();
  }

  for (const auto& uri : uris_list) {
    // If direct is found, it MUST be the only uri in the list. Otherwise, it is
    // an invalid `ProxyChain()`.
    if (base::EqualsCaseInsensitiveASCII(uri, "direct://")) {
      return number_of_proxy_uris > 1 ? ProxyChain() : ProxyChain::Direct();
    }

    proxy_server_list.push_back(
        ProxyUriToProxyServer(uri, default_scheme, is_quic_allowed));
  }

  return ProxyChain(std::move(proxy_server_list));
}
}  // namespace net
