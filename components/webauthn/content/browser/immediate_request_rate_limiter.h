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
  ImmediateRequestRateLimiter();
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
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CONTENT_BROWSER_IMMEDIATE_REQUEST_RATE_LIMITER_H_
