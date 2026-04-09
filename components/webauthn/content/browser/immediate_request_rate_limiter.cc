// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/content/browser/immediate_request_rate_limiter.h"

#include <memory>
#include <string>

#include "base/time/time.h"
#include "components/webauthn/core/browser/rate_limiter_slide_window.h"
#include "content/public/browser/render_frame_host.h"
#include "device/fido/public/features.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/origin.h"

namespace {
constexpr size_t kBucketCount = 1;

bool AcquireFromRateLimiter(
    base::flat_map<std::string,
                   std::unique_ptr<webauthn::RateLimiterSlideWindow>>& limiter,
    const std::string& relying_party,
    int max_requests,
    int window_seconds) {
  auto it = limiter.find(relying_party);
  if (it == limiter.end()) {
    base::TimeDelta time_window = base::Seconds(window_seconds);

    // Use try_emplace to insert the new rate limiter.
    it = limiter
             .try_emplace(relying_party,
                          std::make_unique<webauthn::RateLimiterSlideWindow>(
                              max_requests, time_window, kBucketCount))
             .first;
  }
  return it->second->Acquire(1);
}
}  // namespace

namespace webauthn {

ImmediateRequestRateLimiter::ImmediateRequestRateLimiter() = default;

ImmediateRequestRateLimiter::~ImmediateRequestRateLimiter() = default;

bool ImmediateRequestRateLimiter::IsRequestAllowed(
    content::RenderFrameHost& render_frame_host) {
  if (!base::FeatureList::IsEnabled(
          device::kWebAuthnImmediateRequestRateLimit)) {
    return true;
  }

  const url::Origin top_frame_origin =
      render_frame_host.GetMainFrame()->GetLastCommittedOrigin();
  if (top_frame_origin.host() == "localhost") {
    return true;
  }
  std::string relying_party =
      net::registry_controlled_domains::GetDomainAndRegistry(
          top_frame_origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  CHECK(!relying_party.empty());

  return AcquireFromRateLimiter(
             long_period_rate_limits_, relying_party,
             device::kWebAuthnImmediateRequestLongRateLimitMaxRequests.Get(),
             device::kWebAuthnImmediateRequestLongRateLimitWindowSeconds
                 .Get()) &&
         AcquireFromRateLimiter(
             short_period_rate_limits_, relying_party,
             device::kWebAuthnImmediateRequestShortRateLimitMaxRequests.Get(),
             device::kWebAuthnImmediateRequestShortRateLimitWindowSeconds
                 .Get());
}

}  // namespace webauthn
