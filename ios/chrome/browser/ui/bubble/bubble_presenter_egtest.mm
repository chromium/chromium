// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface BubblePresenterTestCase : ChromeTestCase
@end

@implementation BubblePresenterTestCase

// Open a random url from omnibox. `isAfterNewAppLaunch` is used for deciding
// whether the step of tapping the fake omnibox is needed.
- (void)openURLFromOmniboxWithIsAfterNewAppLaunch:(BOOL)isAfterNewAppLaunch {
  if (isAfterNewAppLaunch) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
        performAction:grey_tap()];
    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                        chrome_test_util::Omnibox()];
  }

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_replaceText(@"chrome://version")];
  // TODO(crbug.com/1454516): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];
}

#pragma mark - Tests

// Tests that the New Tab IPH can be displayed when opening an URL from omnibox.
// TODO(crbug.com/1471222): Test is flaky on device. Re-enable the test.
#if !TARGET_OS_SIMULATOR
#define MAYBE_testNewTabIPH FLAKY_testNewTabIPH
#else
#define MAYBE_testNewTabIPH testNewTabIPH
#endif
- (void)MAYBE_testNewTabIPH {
  // Enable the IPH Demo Mode feature to ensure the IPH triggers
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.additional_args.push_back(
      base::StringPrintf("--enable-features=%s:chosen_feature/"
                         "IPH_iOSNewTabToolbarItemFeature,IPHForSafariSwitcher",
                         feature_engagement::kIPHDemoMode.name));
  // Force the conditions that allow the iph to show.
  config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
  config.additional_args.push_back("SyncedAndFirstDevice");
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self openURLFromOmniboxWithIsAfterNewAppLaunch:YES];

  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              @"BubbleViewLabelIdentifier")];
}

// Tests that the Tab Grid IPH can be displayed when opening a new tab and there
// are multiple tabs.
// TODO(crbug.com/1471222): Test is flaky on device. Re-enable the test.
#if !TARGET_OS_SIMULATOR
#define MAYBE_testTabGridIPH FLAKY_testTabGridIPH
#else
#define MAYBE_testTabGridIPH testTabGridIPH
#endif
- (void)MAYBE_testTabGridIPH {
  // Enable the IPH Demo Mode feature to ensure the IPH triggers
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.additional_args.push_back(base::StringPrintf(
      "--enable-features=%s:chosen_feature/"
      "IPH_iOSTabGridToolbarItemFeature,IPHForSafariSwitcher",
      feature_engagement::kIPHDemoMode.name));
  // Force the conditions that allow the iph to show.
  config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
  config.additional_args.push_back("SyncedAndFirstDevice");
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_accessibilityID(
                                              @"BubbleViewLabelIdentifier")];
}

@end
