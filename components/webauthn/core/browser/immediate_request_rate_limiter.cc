// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/immediate_request_rate_limiter.h"

#include <memory>
#include <string>

#include "base/time/time.h"
#include "components/webauthn/core/browser/rate_limiter_slide_window.h"
#include "device/fido/features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/origin.h"

namespace {
constexpr size_t kBucketCount = 1;
}

namespace webauthn {

ImmediateRequestRateLimiter::ImmediateRequestRateLimiter() = default;

ImmediateRequestRateLimiter::~ImmediateRequestRateLimiter() = default;

bool ImmediateRequestRateLimiter::IsRequestAllowed(const url::Origin& origin) {
  if (!base::FeatureList::IsEnabled(
          device::kWebAuthnImmediateRequestRateLimit)) {
    return true;
  }
  if (origin.host() == "localhost") {
    return true;
  }
  std::string relying_party =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  CHECK(!relying_party.empty());

  auto it = rate_limits_.find(relying_party);
  if (it == rate_limits_.end()) {
    int max_requests =
        device::kWebAuthnImmediateRequestRateLimitMaxRequests.Get();
    int window_seconds =
        device::kWebAuthnImmediateRequestRateLimitWindowSeconds.Get();
    base::TimeDelta time_window = base::Seconds(window_seconds);

    // Use try_emplace to insert the new rate limiter.
    it = rate_limits_
             .try_emplace(relying_party,
                          std::make_unique<webauthn::RateLimiterSlideWindow>(
                              max_requests, time_window, kBucketCount))
             .first;
  }

  return it->second->Acquire(1);
}

}  // namespace webauthn
