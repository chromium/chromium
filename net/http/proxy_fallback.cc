// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/proxy_fallback.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_delegate.h"
#include "net/base/proxy_server.h"

namespace net {

NET_EXPORT bool CanFalloverToNextProxy(const ProxyChain& proxy_chain,
                                       int error,
                                       int* final_error,
                                       ProxyDelegate* proxy_delegate) {
  if (proxy_delegate) {
    std::optional<bool> can_fallover =
        proxy_delegate->CanFalloverToNextProxyOverride(proxy_chain, error);
    if (can_fallover.has_value()) {
      return *can_fallover;
    }
  }

  if (proxy_chain.is_for_ip_protection()) {
    // Log the error.
    // Useful to know if errors not handled below are passed to this function.
    if (const int chain_id = proxy_chain.ip_protection_chain_id();
        chain_id != ProxyChain::kNotIpProtectionChainId) {
      base::UmaHistogramSparse(
          base::StrCat({"Net.IpProtection.CanFalloverToNextProxy2.Error.Chain",
                        base::NumberToString(chain_id)}),
          error);
    }
  }
  *final_error = error;
  const auto& proxy_servers = proxy_chain.proxy_servers();
  const bool has_quic_proxy = std::any_of(
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
    // ERR_PROXY_DELEGATE_CANCELED_CONNECT_{REQUEST, RESPONSE} are used by
    // ProxyDelegates that rely on a separate entity to decide whether to cancel
    // tunnels being established. In these scenarios the expectation is to
    // always fall onto the next ProxyChain in the list.
    case ERR_PROXY_DELEGATE_CANCELED_CONNECT_REQUEST:
    case ERR_PROXY_DELEGATE_CANCELED_CONNECT_RESPONSE:
      return true;
    // A failure while establishing a tunnel through the proxy can fail for
    // reasons related to the request itself (for instance, failing to resolve
    // the hostname of the request) or because of issues with the proxy itself.
    // A ProxyDelegate differentiates the two based on response codes and/or
    // response headers. The delegate signals a destination-related error by
    // returning ERR_PROXY_UNABLE_TO_CONNECT_TO_DESTINATION, which prevents
    // fallback.
    case ERR_PROXY_UNABLE_TO_CONNECT_TO_DESTINATION:
      return false;
    case ERR_TUNNEL_CONNECTION_FAILED:
      // Tunnel connection failures not indicated by
      // ERR_PROXY_UNABLE_TO_CONNECT_TO_DESTINATION are only considered grounds
      // for fallback when connecting to an IP Protection proxy. Other browsers
      // similarly don't fallback, and some client's PAC configurations rely on
      // this for some degree of content blocking. See https://crbug.com/680837
      // for details.
      return proxy_chain.is_for_ip_protection();

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
  }
  return false;
}

}  // namespace net
