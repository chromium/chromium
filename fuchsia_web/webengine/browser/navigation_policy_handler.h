// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_NAVIGATION_POLICY_HANDLER_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_NAVIGATION_POLICY_HANDLER_H_

#include <fuchsia/web/cpp/fidl.h>

#include "base/containers/flat_set.h"
#include "fuchsia_web/webengine/web_engine_export.h"

class NavigationPolicyThrottle;

namespace content {
class NavigationHandle;
}  // namespace content

class WEB_ENGINE_EXPORT NavigationPolicyHandler {
 public:
  NavigationPolicyHandler(
      fuchsia::web::NavigationPolicyProviderParams params,
      fidl::InterfaceHandle<fuchsia::web::NavigationPolicyProvider> delegate);
  ~NavigationPolicyHandler();

  NavigationPolicyHandler(const NavigationPolicyHandler&) = delete;
  NavigationPolicyHandler& operator=(const NavigationPolicyHandler&) = delete;

  void RegisterNavigationThrottle(
      NavigationPolicyThrottle* navigation_throttle);
  void RemoveNavigationThrottle(NavigationPolicyThrottle* navigation_throttle);

  bool is_provider_connected();

  // Sends the request to evaluate the `requested_navigation` to the
  // `NavigationPolicyProvider`.
  void EvaluateRequestedNavigation(
      fuchsia::web::RequestedNavigation requested_navigation,
      fuchsia::web::NavigationPolicyProvider::
          EvaluateRequestedNavigationCallback callback);

  // Determines whether or not the client is interested in evaluating |handle|.
  bool ShouldEvaluateNavigation(content::NavigationHandle* handle,
                                fuchsia::web::NavigationPhase phase);

 private:
  void OnNavigationPolicyProviderDisconnected(zx_status_t status);

  fuchsia::web::NavigationPolicyProviderParams params_;
  fuchsia::web::NavigationPolicyProviderPtr provider_;

  // Keeps track of the NavigationThrottles associated with the Frame that owns
  // |this|.
  base::flat_set<NavigationPolicyThrottle*> navigation_throttles_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_NAVIGATION_POLICY_HANDLER_H_
