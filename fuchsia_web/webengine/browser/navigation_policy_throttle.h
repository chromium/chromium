// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_NAVIGATION_POLICY_THROTTLE_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_NAVIGATION_POLICY_THROTTLE_H_

#include <fuchsia/web/cpp/fidl.h>

#include "content/public/browser/navigation_throttle.h"
#include "fuchsia_web/webengine/web_engine_export.h"

class NavigationPolicyHandler;

class WEB_ENGINE_EXPORT NavigationPolicyThrottle
    : public content::NavigationThrottle {
 public:
  explicit NavigationPolicyThrottle(content::NavigationHandle* handle,
                                    NavigationPolicyHandler* policy_handler);
  ~NavigationPolicyThrottle() override;

  NavigationPolicyThrottle(const NavigationPolicyThrottle&) = delete;
  NavigationPolicyThrottle& operator=(const NavigationPolicyThrottle&) = delete;

  void OnNavigationPolicyProviderDisconnected(ThrottleCheckResult check_result);

  // content::NavigationThrottle implementation.
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillFailRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  void OnRequestedNavigationEvaluated(
      fuchsia::web::NavigationDecision decision);

  content::NavigationThrottle::ThrottleCheckResult HandleNavigationPhase(
      fuchsia::web::NavigationPhase phase);

  NavigationPolicyHandler* policy_handler_;
  content::NavigationHandle* navigation_handle_;

  // Indicates if the navigation is currently paused.
  bool is_paused_ = false;

  // Used for `EvaluateRequestedNavigation()` results callbacks that may outlive
  // this object.
  base::WeakPtrFactory<NavigationPolicyThrottle> weak_factory_{this};
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_NAVIGATION_POLICY_THROTTLE_H_
