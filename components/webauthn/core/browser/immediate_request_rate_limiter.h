// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_IMMEDIATE_REQUEST_RATE_LIMITER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_IMMEDIATE_REQUEST_RATE_LIMITER_H_

#include "base/containers/flat_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webauthn/core/browser/rate_limiter_slide_window.h"
#include "url/origin.h"

namespace webauthn {

class ImmediateRequestRateLimiter : public KeyedService {
 public:
  ImmediateRequestRateLimiter();
  ~ImmediateRequestRateLimiter() override;

  // Returns true if a request at the current time will not exceed any of the
  // throttling limits for Immediate requests.
  // `top_frame_origin` must be the origin of the main frame because the
  // same throttle applies across all requests for a single page.
  bool IsRequestAllowed(const url::Origin& top_frame_origin);

 private:
  base::flat_map<std::string, std::unique_ptr<RateLimiterSlideWindow>>
      long_period_rate_limits_;
  base::flat_map<std::string, std::unique_ptr<RateLimiterSlideWindow>>
      short_period_rate_limits_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_IMMEDIATE_REQUEST_RATE_LIMITER_H_
