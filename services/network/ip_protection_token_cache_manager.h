// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_TOKEN_CACHE_MANAGER_H_
#define SERVICES_NETWORK_IP_PROTECTION_TOKEN_CACHE_MANAGER_H_

#include "base/component_export.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

// Manages the cache of blind-signed auth tokens.
//
// This class is responsible for checking, fetching, and refilling auth tokens
// for IpProtectionConfigCache.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionTokenCacheManager {
 public:
  virtual ~IpProtectionTokenCacheManager() = default;

  // Check whether tokens are available.
  //
  // This function is called on every URL load, so it should complete quickly.
  virtual bool IsAuthTokenAvailable() = 0;

  // Get a token, if one is available.
  //
  // Returns `nullopt` if no token is available, whether for a transient or
  // permanent reason. This method may return `nullopt` even if
  // `IsAuthTokenAvailable()` recently returned `true`.
  virtual absl::optional<network::mojom::BlindSignedAuthTokenPtr>
  GetAuthToken() = 0;

  // Invalidate any previous instruction that token requests should not be
  // made until after a specified time.
  virtual void InvalidateTryAgainAfterTime() = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_TOKEN_CACHE_MANAGER_H_
