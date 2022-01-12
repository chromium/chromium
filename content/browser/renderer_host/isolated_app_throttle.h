// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ISOLATED_APP_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_ISOLATED_APP_THROTTLE_H_

#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

// Blocks navigations into or out of isolated apps.
class CONTENT_EXPORT IsolatedAppThrottle : public NavigationThrottle {
 public:
  static std::unique_ptr<IsolatedAppThrottle> MaybeCreateThrottleFor(
      NavigationHandle* handle);

  explicit IsolatedAppThrottle(NavigationHandle* navigation_handle);
  ~IsolatedAppThrottle() override;
  IsolatedAppThrottle() = delete;
  IsolatedAppThrottle(const IsolatedAppThrottle&) = delete;
  IsolatedAppThrottle& operator=(const IsolatedAppThrottle&) = delete;

 private:
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override;
  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

  NavigationThrottle::ThrottleCheckResult DoThrottle(
      bool needs_app_isolation,
      NavigationThrottle::ThrottleAction block_action);

  // Opens a url in the systems' default application for the given url.
  bool OpenUrlExternal(const GURL& url);

  bool embedder_requests_app_isolation();

  // These two fields store the starting and destination origins of the most
  // recent step in the navigation's redirect chain, including the initial
  // navigation. |prev_origin_| will be nullopt if the frame being navigated
  // hasn't committed any navigation yet.
  absl::optional<url::Origin> prev_origin_;
  url::Origin dest_origin_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_ISOLATED_APP_THROTTLE_H_
