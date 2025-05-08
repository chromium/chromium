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

  bool IsRequestAllowed(const url::Origin& origin);

 private:
  base::flat_map<std::string,
                 std::unique_ptr<RateLimiterSlideWindow>>
      rate_limits_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_IMMEDIATE_REQUEST_RATE_LIMITER_H_
