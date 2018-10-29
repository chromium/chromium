// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_NETWORK_SERVICE_PROXY_DELEGATE_H_
#define SERVICES_NETWORK_NETWORK_SERVICE_PROXY_DELEGATE_H_

#include <deque>

#include "base/component_export.h"
#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/proxy_delegate.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace net {
class HttpRequestHeaders;
class URLRequest;
}  // namespace net

namespace network {

// NetworkServiceProxyDelegate is used to support the custom proxy
// configuration, which can be set in
// NetworkContextParams.custom_proxy_config_client_request.
class COMPONENT_EXPORT(NETWORK_SERVICE) NetworkServiceProxyDelegate
    : public net::ProxyDelegate,
      public mojom::CustomProxyConfigClient {
 public:
  explicit NetworkServiceProxyDelegate(
      mojom::CustomProxyConfigPtr initial_config,
      mojom::CustomProxyConfigClientRequest config_client_request);
  ~NetworkServiceProxyDelegate() override;

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

 private:
  // Checks whether |proxy_server| is present in the current proxy config.
  bool IsInProxyConfig(const net::ProxyServer& proxy_server) const;

  // Whether the current config may proxy |url|.
  bool MayProxyURL(const GURL& url) const;

  // Whether the current config may have proxied |url| with the current config
  // or a previous config.
  bool MayHaveProxiedURL(const GURL& url) const;

  // Whether the |url| with current |proxy_info| is eligible to be proxied.
  bool EligibleForProxy(const net::ProxyInfo& proxy_info,
                        const GURL& url,
                        const std::string& method) const;

  // Get the proxy rules that apply to |url|.
  net::ProxyConfig::ProxyRules GetProxyRulesForURL(const GURL& url) const;

  // mojom::CustomProxyConfigClient implementation:
  void OnCustomProxyConfigUpdated(
      mojom::CustomProxyConfigPtr proxy_config) override;

  mojom::CustomProxyConfigPtr proxy_config_;
  mojo::Binding<mojom::CustomProxyConfigClient> binding_;

  base::MRUCache<std::string, bool> should_use_alternate_proxy_list_cache_;

  // We keep track of a limited number of previous configs so we can determine
  // if a request used a custom proxy if the config happened to change during
  // the request.
  std::deque<mojom::CustomProxyConfigPtr> previous_proxy_configs_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceProxyDelegate);
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SERVICE_PROXY_DELEGATE_H_
