// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_ISOLATED_WEB_APP_THROTTLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_ISOLATED_WEB_APP_THROTTLE_H_

#include <optional>

#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/origin.h"

namespace content {

// Blocks navigations into or out of Isolated Web Apps.
class CONTENT_EXPORT IsolatedWebAppThrottle : public NavigationThrottle {
 public:
  static std::unique_ptr<IsolatedWebAppThrottle> MaybeCreateThrottleFor(
      NavigationHandle* handle);

  explicit IsolatedWebAppThrottle(NavigationHandle* navigation_handle);
  ~IsolatedWebAppThrottle() override;
  IsolatedWebAppThrottle() = delete;
  IsolatedWebAppThrottle(const IsolatedWebAppThrottle&) = delete;
  IsolatedWebAppThrottle& operator=(const IsolatedWebAppThrottle&) = delete;

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
  std::optional<url::Origin> prev_origin_;
  url::Origin dest_origin_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_ISOLATED_WEB_APP_THROTTLE_H_
