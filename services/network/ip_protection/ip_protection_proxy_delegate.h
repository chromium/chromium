// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_DELEGATE_H_
#define SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_DELEGATE_H_

#include <deque>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "components/ip_protection/common/ip_protection_config_cache.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "components/ip_protection/common/masked_domain_list_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/proxy_delegate.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace net {
class HttpRequestHeaders;
class ProxyResolutionService;
}  // namespace net

namespace network {

using ip_protection::MaskedDomainListManager;

// IpProtectionProxyDelegate is used to support IP protection, by injecting
// proxies for requests where IP should be protected.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionProxyDelegate
    : public net::ProxyDelegate,
      public mojom::IpProtectionProxyDelegate {
 public:
  // Both masked_domain_list_manager and ipp_config_cache must be
  // non-null. The masked_domain_list_manager (MaskedDomainList) feature
  // must be enabled.
  IpProtectionProxyDelegate(
      MaskedDomainListManager* masked_domain_list_manager,
      std::unique_ptr<ip_protection::IpProtectionConfigCache> ipp_config_cache,
      bool is_ip_protection_enabled);

  IpProtectionProxyDelegate(const IpProtectionProxyDelegate&) = delete;
  IpProtectionProxyDelegate& operator=(const IpProtectionProxyDelegate&) =
      delete;

  ~IpProtectionProxyDelegate() override;

  void SetReceiver(
      mojo::PendingReceiver<network::mojom::IpProtectionProxyDelegate>
          pending_receiver);

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

  // mojom::IpProtectionProxyDelegate implementation:
  void VerifyIpProtectionConfigGetterForTesting(
      VerifyIpProtectionConfigGetterForTestingCallback callback) override;
  void InvalidateIpProtectionConfigCacheTryAgainAfterTime() override;
  void SetIpProtectionEnabled(bool enabled) override;
  void IsIpProtectionEnabledForTesting(
      IsIpProtectionEnabledForTestingCallback callback) override;

  void OnIpProtectionConfigAvailableForTesting(
      VerifyIpProtectionConfigGetterForTestingCallback callback);

 private:
  friend class IpProtectionProxyDelegateTest;
  FRIEND_TEST_ALL_PREFIXES(IpProtectionProxyDelegateTest, MergeProxyRules);

  bool CheckEligibility(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key) const;
  bool CheckAvailability(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key) const;

  // Returns the equivalent of replacing all DIRECT proxies in
  // `existing_proxy_list` with the proxies in `custom_proxy_list`.
  static net::ProxyList MergeProxyRules(
      const net::ProxyList& existing_proxy_list,
      const net::ProxyList& custom_proxy_list);

  const raw_ptr<MaskedDomainListManager> masked_domain_list_manager_;

  const std::unique_ptr<ip_protection::IpProtectionConfigCache>
      ipp_config_cache_;

  bool is_ip_protection_enabled_;

  mojo::Receiver<network::mojom::IpProtectionProxyDelegate> receiver_{this};

  base::WeakPtrFactory<IpProtectionProxyDelegate> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_PROXY_DELEGATE_H_
