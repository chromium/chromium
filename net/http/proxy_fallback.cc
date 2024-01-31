// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/proxy_fallback.h"

#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"

namespace net {

NET_EXPORT bool CanFalloverToNextProxy(const ProxyChain& proxy_chain,
                                       int error,
                                       int* final_error,
                                       bool is_for_ip_protection) {
  *final_error = error;
  auto proxy_servers = proxy_chain.proxy_servers();
  bool has_quic_proxy = std::any_of(
      proxy_servers.begin(), proxy_servers.end(),
      [](const ProxyServer& proxy_server) { return proxy_server.is_quic(); });
  if (!proxy_chain.is_direct() && has_quic_proxy) {
    // The whole chain should be QUIC.
    for (const auto& proxy_server : proxy_servers) {
      CHECK(proxy_server.is_quic());
    }
    switch (error) {
      case ERR_QUIC_PROTOCOL_ERROR:
      case ERR_QUIC_HANDSHAKE_FAILED:
      case ERR_MSG_TOO_BIG:
        return true;
    }
  }

  // TODO(eroman): Split up these error codes across the relevant proxy types.
  //
  // A failure to resolve the hostname or any error related to establishing a
  // TCP connection could be grounds for trying a new proxy configuration.
  //
  // Why do this when a hostname cannot be resolved?  Some URLs only make sense
  // to proxy servers.  The hostname in those URLs might fail to resolve if we
  // are still using a non-proxy config.  We need to check if a proxy config
  // now exists that corresponds to a proxy server that could load the URL.

  switch (error) {
    case ERR_PROXY_CONNECTION_FAILED:
    case ERR_NAME_NOT_RESOLVED:
    case ERR_INTERNET_DISCONNECTED:
    case ERR_ADDRESS_UNREACHABLE:
    case ERR_CONNECTION_CLOSED:
    case ERR_CONNECTION_TIMED_OUT:
    case ERR_CONNECTION_RESET:
    case ERR_CONNECTION_REFUSED:
    case ERR_CONNECTION_ABORTED:
    case ERR_TIMED_OUT:
    case ERR_SOCKS_CONNECTION_FAILED:
    // ERR_PROXY_CERTIFICATE_INVALID can happen in the case of trying to talk to
    // a proxy using SSL, and ending up talking to a captive portal that
    // supports SSL instead.
    case ERR_PROXY_CERTIFICATE_INVALID:
    // ERR_SSL_PROTOCOL_ERROR can happen when trying to talk SSL to a non-SSL
    // server (like a captive portal).
    case ERR_SSL_PROTOCOL_ERROR:
      return true;

    case ERR_SOCKS_CONNECTION_HOST_UNREACHABLE:
      // Remap the SOCKS-specific "host unreachable" error to a more
      // generic error code (this way consumers like the link doctor
      // know to substitute their error page).
      //
      // Note that if the host resolving was done by the SOCKS5 proxy, we can't
      // differentiate between a proxy-side "host not found" versus a proxy-side
      // "address unreachable" error, and will report both of these failures as
      // ERR_ADDRESS_UNREACHABLE.
      *final_error = ERR_ADDRESS_UNREACHABLE;
      return false;

    case ERR_TUNNEL_CONNECTION_FAILED:
      // A failure while establishing a tunnel to the proxy is only considered
      // grounds for fallback when connecting to an IP Protection proxy. Other
      // browsers similarly don't fallback, and some client's PAC configurations
      // rely on this for some degree of content blocking. See
      // https://crbug.com/680837 for details.
      return is_for_ip_protection;
  }
  return false;
}

}  // namespace net
