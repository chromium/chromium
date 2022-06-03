// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/alerts/alert_overlay_coordinator.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/public/web_content_area/alert_overlay.h"
#include "ios/chrome/browser/overlays/test/overlay_test_macros.h"
#import "ios/chrome/browser/ui/alert_view/alert_action.h"
#import "ios/chrome/browser/ui/alert_view/alert_view_controller.h"
#include "ios/chrome/browser/ui/overlays/test/fake_overlay_request_coordinator_delegate.h"
#include "ios/chrome/test/scoped_key_window.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using alert_overlays::AlertRequest;
using alert_overlays::ButtonConfig;

namespace {
// Consts used for the alert.
NSString* const kTitle = @"title";
NSString* const kMessage = @"message";
NSString* const kAccessibilityIdentifier = @"identifier";
NSString* const kDefaultTextFieldValue = @"default_text";
NSString* const kButtonTitle = @"button_title";

// Creates an AlertRequest for use in tests.
std::unique_ptr<OverlayRequest> CreateAlertRequest() {
  const std::vector<ButtonConfig> button_configs{ButtonConfig(kButtonTitle)};
  return OverlayRequest::CreateWithConfig<AlertRequest>(
      kTitle, kMessage, kAccessibilityIdentifier, nil, button_configs,
      base::BindRepeating(^std::unique_ptr<OverlayResponse>(
          std::unique_ptr<OverlayResponse> response) {
        return response;
      }));
}
}  // namespace

class AlertOverlayCoordinatorTest : public PlatformTest {
 public:
  AlertOverlayCoordinatorTest()
      : root_view_controller_([[UIViewController alloc] init]),
        request_(CreateAlertRequest()),
        coordinator_([[AlertOverlayCoordinator alloc]
            initWithBaseViewController:root_view_controller_
                               browser:&browser_
                               request:request_.get()
                              delegate:&delegate_]) {
    scoped_window_.Get().rootViewController = root_view_controller_;
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  TestBrowser browser_;
  ScopedKeyWindow scoped_window_;
  UIViewController* root_view_controller_ = nil;
  std::unique_ptr<OverlayRequest> request_;
  FakeOverlayRequestCoordinatorDelegate delegate_;
  AlertOverlayCoordinator* coordinator_ = nil;
};

// Tests that the coordinator creates an alert view, sets it up using its
// mediator presents it within the base UIViewController's hierarchy.
TEST_F(AlertOverlayCoordinatorTest, ViewSetup) {
  ASSERT_FALSE(delegate_.HasUIBeenPresented(request_.get()));
  [coordinator_ startAnimated:NO];
  AlertViewController* view_controller =
      base::mac::ObjCCast<AlertViewController>(coordinator_.viewController);
  EXPECT_TRUE(view_controller);
  EXPECT_EQ(view_controller.parentViewController, root_view_controller_);
  EXPECT_TRUE(
      [view_controller.view isDescendantOfView:root_view_controller_.view]);
  EXPECT_TRUE(delegate_.HasUIBeenPresented(request_.get()));
  [coordinator_ stopAnimated:NO];
  EXPECT_TRUE(delegate_.HasUIBeenDismissed(request_.get()));
}
