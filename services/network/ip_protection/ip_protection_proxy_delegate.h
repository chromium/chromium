// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_DELEGATE_H_
#define SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_DELEGATE_H_

#include <deque>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/proxy_delegate.h"
#include "services/network/ip_protection/ip_protection_config_cache.h"
#include "services/network/masked_domain_list/network_service_proxy_allow_list.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace net {
class HttpRequestHeaders;
class ProxyResolutionService;
}  // namespace net

namespace network {

// IpProtectionProxyDelegate is used to support IP protection, by injecting
// proxies for requests where IP should be protected.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionProxyDelegate
    : public net::ProxyDelegate {
 public:
  IpProtectionProxyDelegate(
      NetworkServiceProxyAllowList* network_service_proxy_allow_list);

  IpProtectionProxyDelegate(const IpProtectionProxyDelegate&) = delete;
  IpProtectionProxyDelegate& operator=(const IpProtectionProxyDelegate&) =
      delete;

  ~IpProtectionProxyDelegate() override;

  void SetIpProtectionConfigCache(
      std::unique_ptr<IpProtectionConfigCache> ipp_config_cache) {
    ipp_config_cache_ = std::move(ipp_config_cache);
  }

  // net::ProxyDelegate implementation:
  void OnResolveProxy(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      const std::string& method,
      const net::ProxyRetryInfoMap& proxy_retry_info,
      net::ProxyInfo* result) override;
  void OnFallback(const net::ProxyChain& bad_chain, int net_error) override;
  void OnBeforeTunnelRequest(const net::ProxyChain& proxy_chain,
                             size_t chain_index,
                             net::HttpRequestHeaders* extra_headers) override;
  net::Error OnTunnelHeadersReceived(
      const net::ProxyChain& proxy_chain,
      size_t chain_index,
      const net::HttpResponseHeaders& response_headers) override;
  void SetProxyResolutionService(
      net::ProxyResolutionService* proxy_resolution_service) override;

  IpProtectionConfigCache* GetIpProtectionConfigCache() {
    return ipp_config_cache_.get();
  }

 private:
  friend class IpProtectionProxyDelegateTest;
  FRIEND_TEST_ALL_PREFIXES(IpProtectionProxyDelegateTest, MergeProxyRules);

  // Returns the equivalent of replacing all DIRECT proxies in
  // `existing_proxy_list` with the proxies in `custom_proxy_list`.
  net::ProxyList MergeProxyRules(const net::ProxyList& existing_proxy_list,
                                 const net::ProxyList& custom_proxy_list) const;

  const raw_ptr<NetworkServiceProxyAllowList> network_service_proxy_allow_list_;

  std::unique_ptr<IpProtectionConfigCache> ipp_config_cache_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_DELEGATE_H_
