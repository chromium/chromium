// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_app_interface.h"

#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/web_state_user_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::GetMainController;

namespace {

// This decider cancels all navigation.
class NavigationBlockerDecider
    : public web::WebStatePolicyDecider,
      public web::WebStateUserData<NavigationBlockerDecider> {
 public:
  NavigationBlockerDecider(web::WebState* web_state)
      : web::WebStatePolicyDecider(web_state) {}

  PolicyDecision ShouldAllowRequest(NSURLRequest* request,
                                    const RequestInfo& request_info) override {
    return PolicyDecision::Cancel();
  }

  WEB_STATE_USER_DATA_KEY_DECL();

 private:
  DISALLOW_COPY_AND_ASSIGN(NavigationBlockerDecider);
};

WEB_STATE_USER_DATA_KEY_IMPL(NavigationBlockerDecider)

}  // namespace

@implementation GoogleServicesSettingsAppInterface

+ (void)blockAllNavigationRequestsForCurrentWebState {
  NavigationBlockerDecider::CreateForWebState(
      chrome_test_util::GetCurrentWebState());
}

+ (void)unblockAllNavigationRequestsForCurrentWebState {
  NavigationBlockerDecider::RemoveFromWebState(
      chrome_test_util::GetCurrentWebState());
}

@end
