// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_confirmation_overlay_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/web_content_area/java_script_confirmation_overlay.h"
#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/test/java_script_dialog_overlay_coordinator_test.h"
#import "ios/web/public/test/fakes/test_web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// Test fixture for JavaScriptConfirmationOverlayCoordinator.
using JavaScriptConfirmationOverlayCoordinatorTest =
    JavaScriptDialogOverlayCoordinatorTest;

// Tests that JavaScriptConfirmationOverlayCoordinator creates an alert and
// presents it non-modally.
TEST_F(JavaScriptConfirmationOverlayCoordinatorTest, StartAndStop) {
  web::TestWebState web_state;
  std::unique_ptr<OverlayRequest> passed_request =
      OverlayRequest::CreateWithConfig<
          JavaScriptConfirmationOverlayRequestConfig>(
          JavaScriptDialogSource(&web_state, GURL("https://chromium.test"),
                                 /*is_main_frame=*/true),
          "Message Text");
  OverlayRequest* request = passed_request.get();
  SetRequest(std::move(passed_request));

  // Start the coordinator and verify that the alert is shown.
  StartDialogCoordinator();
  __weak UIViewController* confirmation = GetAlertViewController();
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return !!confirmation.presentingViewController &&
           !confirmation.beingPresented;
  }));

  // Stop the coordinator and verify that the alert is dismissed and that the
  // dismissal delegate is notified.
  StopDialogCoordinator();
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, ^bool {
    return !confirmation.presentingViewController;
  }));
  EXPECT_TRUE(delegate().HasUIBeenDismissed(request));
}
