// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_CONFIG_CACHE_H_
#define SERVICES_NETWORK_IP_PROTECTION_CONFIG_CACHE_H_

#include "base/component_export.h"
#include "services/network/ip_protection_proxy_list_manager.h"
#include "services/network/ip_protection_token_cache_manager.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

// A cache for blind-signed auth tokens.
//
// There is no API to fill the cache - it is the implementation's responsibility
// to do that itself.
//
// This class provides sync access to a token, returning nullopt if none is
// available, thereby avoiding adding latency to proxied requests.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionConfigCache {
 public:
  virtual ~IpProtectionConfigCache() = default;

  // Initializes the proxy chain list and token managers for the cache.
  virtual void SetUp() = 0;

  // Check whether tokens are available in all token caches.
  //
  // This function is called on every URL load, so it should complete quickly.
  virtual bool AreAuthTokensAvailable() = 0;

  // Get a token, if one is available.
  //
  // Returns `nullopt` if no token is available, whether for a transient or
  // permanent reason. This method may return `nullopt` even if
  // `IsAuthTokenAvailable()` recently returned `true`.
  virtual absl::optional<network::mojom::BlindSignedAuthTokenPtr> GetAuthToken(
      size_t chain_index) = 0;

  // Invalidate any previous instruction that token requests should not be
  // made until after a specified time.
  virtual void InvalidateTryAgainAfterTime() = 0;

  // Set the token cache manager for the cache.
  virtual void SetIpProtectionTokenCacheManagerForTesting(
      network::mojom::IpProtectionProxyLayer proxy_layer,
      std::unique_ptr<IpProtectionTokenCacheManager>
          ipp_token_cache_manager) = 0;

  // Fetch the token cache manager.
  virtual IpProtectionTokenCacheManager*
  GetIpProtectionTokenCacheManagerForTesting(
      network::mojom::IpProtectionProxyLayer proxy_layer) = 0;

  // Set the proxy chain list manager for the cache.
  virtual void SetIpProtectionProxyListManagerForTesting(
      std::unique_ptr<IpProtectionProxyListManager> ipp_proxy_list_manager) = 0;

  // Check whether a proxy chain list is available.
  virtual bool IsProxyListAvailable() = 0;

  // Return the currently cached proxy chain lists. This contains the lists of
  // hostnames corresponding to each proxy chain that should be used. This
  // may be empty even if `IsProxyListAvailable()` returned true.
  virtual std::vector<net::ProxyChain> GetProxyChainList() = 0;

  // Request a refresh of the proxy chain list. Call this when it's likely that
  // the proxy chain list is out of date.
  virtual void RequestRefreshProxyList() = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_CONFIG_CACHE_H_
