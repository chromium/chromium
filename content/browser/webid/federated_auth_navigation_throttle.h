// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_FEDERATED_AUTH_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_WEBID_FEDERATED_AUTH_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;

// Used to delay a navigation while the browser gathers the user's
// awareness / permission of the tracking risks involved in third party
// federated identity flows.
class CONTENT_EXPORT FederatedAuthNavigationThrottle
    : public NavigationThrottle {
 public:
  explicit FederatedAuthNavigationThrottle(NavigationHandle* handle);
  ~FederatedAuthNavigationThrottle() override;
  FederatedAuthNavigationThrottle(const FederatedAuthNavigationThrottle&) =
      delete;
  FederatedAuthNavigationThrottle& operator=(
      const FederatedAuthNavigationThrottle&) = delete;

  static std::unique_ptr<NavigationThrottle> MaybeCreateThrottleFor(
      NavigationHandle* handle);

  // NavigationThrottle implementation:
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override;
  const char* GetNameForLogging() override;

 private:
  void OnSigninApproved(IdentityRequestDialogController::UserApproval approval);
  void OnTokenProvisionApproved(
      IdentityRequestDialogController::UserApproval approval);

  bool IsFederationRequest(GURL url);
  bool IsFederationResponse(GURL url);

  std::unique_ptr<IdentityRequestDialogController> request_dialog_controller_;
  std::string redirect_uri_;
  base::WeakPtrFactory<FederatedAuthNavigationThrottle> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_FEDERATED_AUTH_NAVIGATION_THROTTLE_H_
