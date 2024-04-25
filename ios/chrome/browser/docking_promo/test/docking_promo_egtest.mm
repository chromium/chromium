// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/base_earl_grey_test_case_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface DockingPromoTestCase : ChromeTestCase
@end

@implementation DockingPromoTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.iph_feature_enabled = "IPH_iOSDockingPromo";
  std::string docking_promo_feature_with_testing_params =
      "--enable-features=" + std::string(kIOSDockingPromo.name) + ":" +
      kIOSDockingPromoNewUserInactiveThreshold + "/" + "1s";
  config.additional_args.push_back(docking_promo_feature_with_testing_params);
  config.additional_args.push_back("-enable-promo-manager-fullscreen-promos");
  config.additional_args.push_back("-FirstRunRecency");
  config.additional_args.push_back("1");

  return config;
}

// Tests that the Docking Promo displays correctly when the inactivity window
// condition is met.
- (void)testDockingPromoDisplaysAfterInactivityWindow {
  BOOL appInBackground =
      [[AppLaunchManager sharedManager] backgroundApplication];

  GREYAssert(appInBackground, @"Application did not background.");

  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(2));

  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:[self appConfigurationForTestCase]];

  id<GREYMatcher> promo =
      grey_allOf(grey_accessibilityID(@"kDockingPromoAccessibilityId"),
                 grey_sufficientlyVisible(), nil);

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:promo
                                              timeout:base::Seconds(3)];
}

@end
