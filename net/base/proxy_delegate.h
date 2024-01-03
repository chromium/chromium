// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_PROXY_DELEGATE_H_
#define NET_BASE_PROXY_DELEGATE_H_

#include <string>

#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/proxy_chain.h"
#include "net/proxy_resolution/proxy_retry_info.h"

class GURL;

namespace net {

class HttpRequestHeaders;
class HttpResponseHeaders;
class ProxyInfo;
class ProxyResolutionService;

// Delegate for setting up a connection.
class NET_EXPORT ProxyDelegate {
 public:
  ProxyDelegate() = default;
  ProxyDelegate(const ProxyDelegate&) = delete;
  ProxyDelegate& operator=(const ProxyDelegate&) = delete;
  virtual ~ProxyDelegate() = default;

  // Called as the proxy is being resolved for |url| for a |method| request.
  // The caller may pass an empty string to get method agnostic resoulution.
  // Allows the delegate to override the proxy resolution decision made by
  // ProxyResolutionService. The delegate may override the decision by modifying
  // the ProxyInfo |result|.
  virtual void OnResolveProxy(
      const GURL& url,
      const NetworkAnonymizationKey& network_anonymization_key,
      const std::string& method,
      const ProxyRetryInfoMap& proxy_retry_info,
      ProxyInfo* result) = 0;

  // Called when use of a proxy chain failed due to `net_error`, but another
  // proxy chain in the list succeeded. The failed proxy is within `bad_chain`,
  // but it is undefined at which proxy in that chain. `net_error` is the
  // network error encountered, if any, and OK if the fallback was for a reason
  // other than a network error (e.g. the proxy service was explicitly directed
  // to skip a proxy).
  virtual void OnFallback(const ProxyChain& bad_chain, int net_error) = 0;

  // Called immediately before a proxy tunnel request is sent. Provides the
  // embedder an opportunity to add extra request headers.
  virtual void OnBeforeTunnelRequest(const ProxyChain& proxy_chain,
                                     size_t chain_index,
                                     HttpRequestHeaders* extra_headers) = 0;

  // Called when the response headers for the proxy tunnel request have been
  // received. Allows the delegate to override the net error code of the tunnel
  // request. Returning OK causes the standard tunnel response handling to be
  // performed. Implementations should make sure they can trust the proxy server
  // at position `chain_index` in `proxy_chain` before making decisions based on
  // `response_headers`.
  virtual Error OnTunnelHeadersReceived(
      const ProxyChain& proxy_chain,
      size_t chain_index,
      const HttpResponseHeaders& response_headers) = 0;

  // Associates a `ProxyResolutionService` with this `ProxyDelegate`.
  // `proxy_resolution_service` must outlive `this`.
  virtual void SetProxyResolutionService(
      ProxyResolutionService* proxy_resolution_service) = 0;
};

}  // namespace net

#endif  // NET_BASE_PROXY_DELEGATE_H_
