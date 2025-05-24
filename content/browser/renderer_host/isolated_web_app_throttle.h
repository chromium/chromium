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
  using NavigationThrottle::ThrottleAction;
  using NavigationThrottle::ThrottleCheckResult;

  static std::unique_ptr<IsolatedWebAppThrottle> MaybeCreateThrottleFor(
      NavigationHandle* handle);

  explicit IsolatedWebAppThrottle(NavigationHandle* navigation_handle);
  ~IsolatedWebAppThrottle() override;
  IsolatedWebAppThrottle() = delete;
  IsolatedWebAppThrottle(const IsolatedWebAppThrottle&) = delete;
  IsolatedWebAppThrottle& operator=(const IsolatedWebAppThrottle&) = delete;

 private:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

  // Validates whether the current navigation transition
  // (WillStartRequest/WillRedirectRequest/WillProcessResponse) is valid with
  // respect to the IWA navigation rules for the current WebContents.
  //
  // Returns PROCEED upon success; upon failure, returns `block_action` or
  // CANCEL depending on the fail stage.
  //
  // `dest_needs_app_isolation` has different sources of truth depending on the
  // navigation stage: for WillStartRequest/WillRedirectRequest it's derived
  // based on the tentative destination origin (as that's the only signal we
  // get), whereas for WillProcessResponse this value is based on the web
  // exposed isolation level assigned to the render process host this navigation
  // will commit in.
  //
  // In theory, once this value is set to `true` during a
  // navigation, it should never fall back to `false`; practically, the WEIL is
  // a combination of an IWA URL and COOP/COEP headers, so it's better to
  // gracefully handle this case instead of making a hard assumption.
  // TODO(crbug.com/417403902): Investigate this.
  ThrottleCheckResult MaybeThrottleNavigationTransition(
      bool dest_needs_app_isolation,
      ThrottleAction block_action);

  // Opens a url in the systems' default application for the given url.
  bool OpenUrlExternal(const GURL& url);

  // These two fields store the starting and destination origins of the most
  // recent step in the navigation's redirect chain, including the initial
  // navigation. `prev_origin_` will be nullopt if the frame being navigated
  // hasn't committed any navigation yet.
  std::optional<url::Origin> prev_origin_;
  url::Origin dest_origin_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_ISOLATED_WEB_APP_THROTTLE_H_
