// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/mini_map/ui_bundled/mini_map_interstitial_view_controller.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/platform_test.h"

using MiniMapInterstitialViewControllerTest = PlatformTest;

// Tests that consent screen is displayed correctly.
TEST_F(MiniMapInterstitialViewControllerTest, TestScreen) {
  ScopedKeyWindow scoped_key_window;
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  [scoped_key_window.Get() setRootViewController:base_view_controller];
  MiniMapInterstitialViewController* mini_map_interstial_view_controller =
      [[MiniMapInterstitialViewController alloc] init];
  [base_view_controller
      presentViewController:mini_map_interstial_view_controller
                   animated:NO
                 completion:nil];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return mini_map_interstial_view_controller.beingPresented;
      }));
  [base_view_controller dismissViewControllerAnimated:NO completion:nil];
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return !mini_map_interstial_view_controller.beingPresented;
      }));
}
