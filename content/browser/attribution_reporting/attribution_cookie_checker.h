// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_COOKIE_CHECKER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_COOKIE_CHECKER_H_

#include "base/functional/callback_forward.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

class AttributionCookieChecker {
 public:
  virtual ~AttributionCookieChecker() = default;

  using Callback = base::OnceCallback<void(bool is_debug_cookie_set)>;

  // Checks if an attribution debug key is set for `origin`.
  virtual void IsDebugCookieSet(const url::Origin& origin, Callback) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_COOKIE_CHECKER_H_
