// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SERVICE_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/types/expected.h"
#import "components/keyed_service/core/keyed_service.h"

class AuthenticationService;

enum class PageContextWrapperError;

namespace optimization_guide::proto {
class PageContext;
}  // namespace optimization_guide::proto

namespace signin {
class IdentityManager;
}  // namespace signin
class PrefService;

// A browser-context keyed service for BWG.
class BwgService : public KeyedService {
 public:
  BwgService(AuthenticationService* auth_service,
             signin::IdentityManager* identity_manager,
             PrefService* pref_service);
  ~BwgService() override;

  // Presents the overlay on a given view controller for a given expected
  // PageContext.
  void PresentOverlayOnViewController(
      UIViewController* base_view_controller,
      base::expected<std::unique_ptr<optimization_guide::proto::PageContext>,
                     PageContextWrapperError> expected_page_context);

  // Returns whether the current profile is eligible for BWG.
  // TODO(crbug.com/419066154): Use this function to show the entry point.
  bool IsEligibleForBWG();

 private:
  // AuthenticationService used to check if a user is signed in or not.
  raw_ptr<AuthenticationService> auth_service_ = nullptr;

  // Identity manager used to check account capabilities.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  // The PrefService associated with the Profile.
  raw_ptr<PrefService> pref_service_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_SERVICE_H_
