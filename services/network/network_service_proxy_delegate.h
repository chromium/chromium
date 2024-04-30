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
#include "net/proxy_resolution/proxy_resolution_service.h"
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
      mojo::PendingRemote<mojom::CustomProxyConnectionObserver>
          observer_remote);

  NetworkServiceProxyDelegate(const NetworkServiceProxyDelegate&) = delete;
  NetworkServiceProxyDelegate& operator=(const NetworkServiceProxyDelegate&) =
      delete;

  ~NetworkServiceProxyDelegate() override;

  // net::ProxyDelegate implementation:
  void OnResolveProxy(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      const std::string& method,
      const net::ProxyRetryInfoMap& proxy_retry_info,
      net::ProxyInfo* result) override;
  void OnSuccessfulRequestAfterFailures(
      const net::ProxyRetryInfoMap& proxy_retry_info) override;
  void OnFallback(const net::ProxyChain& bad_chain, int net_error) override;
  net::Error OnBeforeTunnelRequest(
      const net::ProxyChain& proxy_chain,
      size_t chain_index,
      net::HttpRequestHeaders* extra_headers) override;
  net::Error OnTunnelHeadersReceived(
      const net::ProxyChain& proxy_chain,
      size_t chain_index,
      const net::HttpResponseHeaders& response_headers) override;
  void SetProxyResolutionService(
      net::ProxyResolutionService* proxy_resolution_service) override;

 private:
  friend class NetworkServiceProxyDelegateTest;

  // Checks whether `proxy_chain` is present in the current proxy config.
  bool IsInProxyConfig(const net::ProxyChain& proxy_chain) const;

  // Whether the HTTP |method| with current |proxy_info| is eligible to be
  // proxied.
  bool EligibleForProxy(const net::ProxyInfo& proxy_info,
                        const std::string& method) const;

  void OnObserverDisconnect();

  // mojom::CustomProxyConfigClient implementation:
  void OnCustomProxyConfigUpdated(
      mojom::CustomProxyConfigPtr proxy_config,
      OnCustomProxyConfigUpdatedCallback callback) override;

  mojom::CustomProxyConfigPtr proxy_config_;
  mojo::Receiver<mojom::CustomProxyConfigClient> receiver_;
  mojo::Remote<mojom::CustomProxyConnectionObserver> observer_;

  raw_ptr<net::ProxyResolutionService> proxy_resolution_service_ = nullptr;
};

}  // namespace network

#endif  // SERVICES_NETWORK_NETWORK_SERVICE_PROXY_DELEGATE_H_
