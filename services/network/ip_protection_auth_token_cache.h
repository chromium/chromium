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
// to do that itself. The `MayNeedAuthTokenSoon()` method provides a hint that a
// `GetAuthToken()` call may occur in the near future, and allows
// implementations time to refresh the cache if it has grown stale. Callers
// should call `MayNeedAuthTokenSoon()` at least once for each call to
// `GetAuthToken()`.
//
// This class provides sync access to a token, returning nullopt if none is
// available, thereby avoiding adding latency to proxied requests.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionAuthTokenCache {
 public:
  virtual ~IpProtectionAuthTokenCache() = default;

  // Advise that a token will be required soon.
  //
  // Prefer to send this signal as early as possible, as it may initiate Mojo
  // IPCs and even communication with remote systems. This should be called
  // at least once (but more than once is OK) per call to `GetAuthToken()`.
  virtual void MayNeedAuthTokenSoon() = 0;

  // Get a token, if one is available.
  //
  // Returns `nullopt` if no token is available, whether for a transient or
  // permanent reason.
  virtual absl::optional<network::mojom::BlindSignedAuthTokenPtr>
  GetAuthToken() = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_AUTH_TOKEN_CACHE_H_
