// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_CONFIG_CACHE_IMPL_H_
#define SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_CONFIG_CACHE_IMPL_H_

#include <deque>
#include <map>
#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/features.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_chain.h"
#include "services/network/ip_protection/ip_protection_config_cache.h"
#include "services/network/ip_protection/ip_protection_config_getter.h"
#include "services/network/ip_protection/ip_protection_data_types.h"
#include "services/network/ip_protection/ip_protection_proxy_list_manager.h"
#include "services/network/ip_protection/ip_protection_token_cache_manager.h"

namespace network {

// An implementation of IpProtectionConfigCache that fills itself by making
// IPC calls to the IpProtectionConfigGetter in the browser process.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionConfigCacheImpl
    : public IpProtectionConfigCache,
      net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // If `config_getter` is unbound, no tokens will be provided.
  explicit IpProtectionConfigCacheImpl(
      std::unique_ptr<IpProtectionConfigGetter> config_getter);
  ~IpProtectionConfigCacheImpl() override;

  // IpProtectionConfigCache implementation.
  bool AreAuthTokensAvailable() override;
  std::optional<BlindSignedAuthToken> GetAuthToken(size_t chain_index) override;
  void InvalidateTryAgainAfterTime() override;
  bool IsProxyListAvailable() override;
  void QuicProxiesFailed() override;
  std::vector<net::ProxyChain> GetProxyChainList() override;
  void RequestRefreshProxyList() override;
  void GeoObserved(const std::string& geo_id) override;
  void SetIpProtectionTokenCacheManagerForTesting(
      IpProtectionProxyLayer proxy_layer,
      std::unique_ptr<IpProtectionTokenCacheManager> ipp_token_cache_manager)
      override;
  IpProtectionTokenCacheManager* GetIpProtectionTokenCacheManagerForTesting(
      IpProtectionProxyLayer proxy_layer) override;
  void SetIpProtectionProxyListManagerForTesting(
      std::unique_ptr<IpProtectionProxyListManager> ipp_proxy_list_manager)
      override;
  IpProtectionProxyListManager* GetIpProtectionProxyListManagerForTesting()
      override;

  // `NetworkChangeNotifier::NetworkChangeObserver` implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

 private:
  // Source of auth tokens and proxy list, when needed.
  std::unique_ptr<IpProtectionConfigGetter> config_getter_;

  // A manager for the list of currently cached proxy hostnames.
  std::unique_ptr<IpProtectionProxyListManager> ipp_proxy_list_manager_;

  // Proxy layer managers for cache of blind-signed auth tokens.
  std::map<IpProtectionProxyLayer,
           std::unique_ptr<IpProtectionTokenCacheManager>>
      ipp_token_cache_managers_;

  // If true, this class will try to connect to IP Protection proxies via QUIC.
  // Once this value becomes false, it stays false until a network change or
  // browser restart.
  bool ipp_over_quic_;

  // Feature flag to safely introduce token caching by geo.
  const bool enable_token_caching_by_geo_;

  base::WeakPtrFactory<IpProtectionConfigCacheImpl> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_CONFIG_CACHE_IMPL_H_
