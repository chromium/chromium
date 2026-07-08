// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CONTENT_BROWSER_IMMEDIATE_REQUEST_RATE_LIMITER_H_
#define COMPONENTS_WEBAUTHN_CONTENT_BROWSER_IMMEDIATE_REQUEST_RATE_LIMITER_H_

#include "base/containers/flat_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webauthn/core/browser/rate_limiter_slide_window.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace webauthn {

class ImmediateRequestRateLimiter : public KeyedService {
 public:
  // Rate limiting thresholds and window durations per eTLD+1 (relying party).
  // A request must pass both the long and short sliding window checks.
  struct Limits {
    // Maximum number of requests allowed within `window_seconds_long`.
    int max_requests_long = 10;
    // Duration of the long rate limit sliding window in seconds.
    int window_seconds_long = 60;
    // Maximum number of requests allowed within `window_seconds_short`.
    int max_requests_short = 2;
    // Duration of the short rate limit sliding window in seconds.
    int window_seconds_short = 5;
  };

  ImmediateRequestRateLimiter();
  explicit ImmediateRequestRateLimiter(Limits limits);
  ~ImmediateRequestRateLimiter() override;

  // Returns true if a request at the current time will not exceed any of the
  // throttling limits for Immediate requests.
  // The origin of the main frame of `render_frame_host`'s page will be used to
  // scope the rate limit.
  bool IsRequestAllowed(content::RenderFrameHost& render_frame_host);

 private:
  base::flat_map<std::string, std::unique_ptr<RateLimiterSlideWindow>>
      long_period_rate_limits_;
  base::flat_map<std::string, std::unique_ptr<RateLimiterSlideWindow>>
      short_period_rate_limits_;
  const Limits limits_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CONTENT_BROWSER_IMMEDIATE_REQUEST_RATE_LIMITER_H_
