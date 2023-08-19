// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_SERVICE_PROXY_DELEGATE_H_
#define SERVICES_NETWORK_NETWORK_SERVICE_PROXY_DELEGATE_H_

#include <deque>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/proxy_delegate.h"
#include "services/network/ip_protection_auth_token_cache.h"
#include "services/network/network_service_proxy_allow_list.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace net {
class HttpRequestHeaders;
class ProxyResolutionService;
}  // namespace net

namespace network {

// NetworkServiceProxyDelegate is used to support the custom proxy
// configuration, which can be set in
// NetworkContextParams.custom_proxy_config_client_receiver.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkServiceProxyDelegate
    : public net::ProxyDelegate,
      public mojom::CustomProxyConfigClient {
 public:
  NetworkServiceProxyDelegate(
      mojom::CustomProxyConfigPtr initial_config,
      mojo::PendingReceiver<mojom::CustomProxyConfigClient>
          config_client_receiver,
      mojo::PendingRemote<mojom::CustomProxyConnectionObserver> observer_remote,
      NetworkServiceProxyAllowList* network_service_proxy_allow_list);

  NetworkServiceProxyDelegate(const NetworkServiceProxyDelegate&) = delete;
  NetworkServiceProxyDelegate& operator=(const NetworkServiceProxyDelegate&) =
      delete;

  ~NetworkServiceProxyDelegate() override;

  void SetProxyResolutionService(
      net::ProxyResolutionService* proxy_resolution_service) {
    proxy_resolution_service_ = proxy_resolution_service;
  }

  void SetIpProtectionAuthTokenCache(
      std::unique_ptr<IpProtectionAuthTokenCache> auth_token_cache) {
    auth_token_cache_ = std::move(auth_token_cache);
  }

  // net::ProxyDelegate implementation:
  void OnResolveProxy(const GURL& url,
                      const GURL& top_frame_url,
                      const std::string& method,
                      const net::ProxyRetryInfoMap& proxy_retry_info,
                      net::ProxyInfo* result) override;
  void OnFallback(const net::ProxyServer& bad_proxy, int net_error) override;
  void OnBeforeTunnelRequest(const net::ProxyServer& proxy_server,
                             net::HttpRequestHeaders* extra_headers) override;
  net::Error OnTunnelHeadersReceived(
      const net::ProxyServer& proxy_server,
      const net::HttpResponseHeaders& response_headers) override;

  IpProtectionAuthTokenCache* GetAuthTokenCacheForTesting() {
    return auth_token_cache_.get();
  }

 private:
  // Checks if this CustomProxyConfig is supporting IP Protection.
  bool IsForIpProtection();

  // Checks whether |proxy_server| is present in the current proxy config.
  bool IsInProxyConfig(const net::ProxyServer& proxy_server) const;

  // Whether the current config may proxy |url|.
  bool MayProxyURL(const GURL& url) const;

  // Whether the HTTP |method| with current |proxy_info| is eligible to be
  // proxied.
  bool EligibleForProxy(const net::ProxyInfo& proxy_info,
                        const std::string& method) const;

  // Replaces all DIRECT options in `proxy_info`'s proxy_list with the HTTPS
  // proxy set in `proxy_config_`. No op when the HTTPS proxy list in
  // `proxy_config_` is empty.
  void MergeProxyRules(const net::ProxyList& existing_proxy_list,
                       net::ProxyInfo& proxy_info) const;

  void OnObserverDisconnect();

  // mojom::CustomProxyConfigClient implementation:
  void OnCustomProxyConfigUpdated(
      mojom::CustomProxyConfigPtr proxy_config,
      OnCustomProxyConfigUpdatedCallback callback) override;
  void MarkProxiesAsBad(base::TimeDelta bypass_duration,
                        const net::ProxyList& bad_proxies,
                        MarkProxiesAsBadCallback callback) override;
  void ClearBadProxiesCache() override;

  mojom::CustomProxyConfigPtr proxy_config_;
  mojo::Receiver<mojom::CustomProxyConfigClient> receiver_;
  mojo::Remote<mojom::CustomProxyConnectionObserver> observer_;
  raw_ptr<NetworkServiceProxyAllowList> network_service_proxy_allow_list_;

  raw_ptr<net::ProxyResolutionService, DanglingUntriaged>
      proxy_resolution_service_ = nullptr;

  std::unique_ptr<IpProtectionAuthTokenCache> auth_token_cache_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SERVICE_PROXY_DELEGATE_H_
