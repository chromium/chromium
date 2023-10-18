// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_CONFIG_CACHE_IMPL_H_
#define SERVICES_NETWORK_IP_PROTECTION_CONFIG_CACHE_IMPL_H_

#include <deque>
#include <map>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/ip_protection_config_cache.h"
#include "services/network/ip_protection_proxy_list_manager.h"
#include "services/network/ip_protection_token_cache_manager.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

// An implementation of IpProtectionConfigCache that fills itself by making
// IPC calls to the IpProtectionConfigGetter in the browser process.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionConfigCacheImpl
    : public IpProtectionConfigCache {
 public:
  // If `config_getter` is unbound, no tokens will be provided.
  explicit IpProtectionConfigCacheImpl(
      mojo::PendingRemote<network::mojom::IpProtectionConfigGetter>
          config_getter);
  ~IpProtectionConfigCacheImpl() override;

  // IpProtectionConfigCache implementation.
  void SetUp() override;
  bool AreAuthTokensAvailable() override;
  absl::optional<network::mojom::BlindSignedAuthTokenPtr> GetAuthToken(
      network::mojom::IpProtectionProxyLayer proxy_layer) override;
  void InvalidateTryAgainAfterTime() override;
  void SetIpProtectionTokenCacheManagerForTesting(
      network::mojom::IpProtectionProxyLayer proxy_layer,
      std::unique_ptr<IpProtectionTokenCacheManager> ipp_token_cache_manager)
      override;
  IpProtectionTokenCacheManager* GetIpProtectionTokenCacheManagerForTesting(
      network::mojom::IpProtectionProxyLayer proxy_layer) override;
  void SetIpProtectionProxyListManagerForTesting(
      std::unique_ptr<IpProtectionProxyListManager> ipp_proxy_list_manager)
      override;
  bool IsProxyListAvailable() override;
  const std::vector<std::string>& GetProxyList() override;
  void RequestRefreshProxyList() override;

 private:
  // Source of auth tokens and proxy list, when needed.
  mojo::Remote<network::mojom::IpProtectionConfigGetter> config_getter_;

  // A manager for the list of currently cached proxy hostnames.
  std::unique_ptr<IpProtectionProxyListManager> ipp_proxy_list_manager_;

  // Proxy layer managers for cache of blind-signed auth tokens.
  std::map<network::mojom::IpProtectionProxyLayer,
           std::unique_ptr<IpProtectionTokenCacheManager>>
      ipp_token_cache_managers_;

  base::WeakPtrFactory<IpProtectionConfigCacheImpl> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_CONFIG_CACHE_IMPL_H_
