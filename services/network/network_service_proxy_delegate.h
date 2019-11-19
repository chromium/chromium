// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_SERVICE_PROXY_DELEGATE_H_
#define SERVICES_NETWORK_NETWORK_SERVICE_PROXY_DELEGATE_H_

#include <deque>

#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/proxy_delegate.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace net {
class HttpRequestHeaders;
class ProxyResolutionService;
class URLRequest;
}  // namespace net

namespace network {

// NetworkServiceProxyDelegate is used to support the custom proxy
// configuration, which can be set in
// NetworkContextParams.custom_proxy_config_client_receiver.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkServiceProxyDelegate
    : public net::ProxyDelegate,
      public mojom::CustomProxyConfigClient {
 public:
  explicit NetworkServiceProxyDelegate(
      mojom::CustomProxyConfigPtr initial_config,
      mojo::PendingReceiver<mojom::CustomProxyConfigClient>
          config_client_receiver);
  ~NetworkServiceProxyDelegate() override;

  void SetProxyResolutionService(
      net::ProxyResolutionService* proxy_resolution_service) {
    proxy_resolution_service_ = proxy_resolution_service;
  }

  // These methods are forwarded from the NetworkDelegate.
  void OnBeforeStartTransaction(net::URLRequest* request,
                                net::HttpRequestHeaders* headers);
  void OnBeforeSendHeaders(net::URLRequest* request,
                           const net::ProxyInfo& proxy_info,
                           net::HttpRequestHeaders* headers);

  // net::ProxyDelegate implementation:
  void OnResolveProxy(const GURL& url,
                      const std::string& method,
                      const net::ProxyRetryInfoMap& proxy_retry_info,
                      net::ProxyInfo* result) override;
  void OnFallback(const net::ProxyServer& bad_proxy, int net_error) override;
  void OnBeforeHttp1TunnelRequest(
      const net::ProxyServer& proxy_server,
      net::HttpRequestHeaders* extra_headers) override;
  net::Error OnHttp1TunnelHeadersReceived(
      const net::ProxyServer& proxy_server,
      const net::HttpResponseHeaders& response_headers) override;

 private:
  // Checks whether |proxy_server| is present in the current proxy config.
  bool IsInProxyConfig(const net::ProxyServer& proxy_server) const;

  // Whether the current config may proxy |url|.
  bool MayProxyURL(const GURL& url) const;

  // Whether the current config may have proxied |url| with the current config
  // or a previous config.
  bool MayHaveProxiedURL(const GURL& url) const;

  // Whether the HTTP |method| with current |proxy_info| is eligible to be
  // proxied.
  bool EligibleForProxy(const net::ProxyInfo& proxy_info,
                        const std::string& method) const;

  // Fills the alternative proxy config in |result| if applicable.
  void GetAlternativeProxy(const net::ProxyRetryInfoMap& proxy_retry_info,
                           net::ProxyInfo* result);

  // mojom::CustomProxyConfigClient implementation:
  void OnCustomProxyConfigUpdated(
      mojom::CustomProxyConfigPtr proxy_config) override;
  void MarkProxiesAsBad(base::TimeDelta bypass_duration,
                        const net::ProxyList& bad_proxies,
                        MarkProxiesAsBadCallback callback) override;
  void ClearBadProxiesCache() override;

  mojom::CustomProxyConfigPtr proxy_config_;
  mojo::Receiver<mojom::CustomProxyConfigClient> receiver_;

  // Cache of URLs for which the usage of custom proxy results
  // in redirect loops. A container is used here since it's possible that
  // at any given time, there are multiple URLs that result in redirect loops
  // when fetched via the custom proxy.
  std::deque<GURL> redirect_loop_cache_;

  // We keep track of a limited number of previous configs so we can determine
  // if a request used a custom proxy if the config happened to change during
  // the request.
  std::deque<mojom::CustomProxyConfigPtr> previous_proxy_configs_;

  net::ProxyResolutionService* proxy_resolution_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceProxyDelegate);
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SERVICE_PROXY_DELEGATE_H_
