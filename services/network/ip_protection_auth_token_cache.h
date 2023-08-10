// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_AUTH_TOKEN_CACHE_H_
#define SERVICES_NETWORK_IP_PROTECTION_AUTH_TOKEN_CACHE_H_

#include "base/component_export.h"
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
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionAuthTokenCache {
 public:
  virtual ~IpProtectionAuthTokenCache() = default;

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
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_AUTH_TOKEN_CACHE_H_
