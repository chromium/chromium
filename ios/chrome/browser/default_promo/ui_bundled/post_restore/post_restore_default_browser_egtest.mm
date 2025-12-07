// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/time/time.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
base::TimeDelta kPromoAppearanceTimeout = base::Seconds(7);
NSString* const kDefaultBrowserKey = @"DefaultBrowserUtils";

// Matcher for the title.
id<GREYMatcher> TitleMatcher() {
  return grey_text(
      l10n_util::GetNSString(IDS_IOS_POST_RESTORE_DEFAULT_BROWSER_PROMO_TITLE));
}

// Matcher for the "Open Settings" button.
id<GREYMatcher> PrimaryActionMatcher() {
  return grey_text(l10n_util::GetNSString(
      IDS_IOS_POST_RESTORE_DEFAULT_BROWSER_PRIMARY_ACTION));
}

// Matcher for the "No Thanks" button.
id<GREYMatcher> SecondaryActionMatcher() {
  return grey_text(l10n_util::GetNSString(
      IDS_IOS_POST_RESTORE_DEFAULT_BROWSER_SECONDARY_ACTION));
}
}  // namespace

// Tests related to the Post Restore Default Browser Promo.
@interface PostRestoreDefaultBrowserPromoTestCase : ChromeTestCase
@end

@implementation PostRestoreDefaultBrowserPromoTestCase

#pragma mark - Helpers

- (void)checkThatCommonElementsAreVisible {
  [[EarlGrey selectElementWithMatcher:TitleMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:PrimaryActionMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:SecondaryActionMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)checkThatCommonElementsAreNotVisible {
  [[EarlGrey selectElementWithMatcher:TitleMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:PrimaryActionMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:SecondaryActionMatcher()]
      assertWithMatcher:grey_notVisible()];
}

- (void)simulateRestore {
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kSimulatePostDeviceRestore);

  // The post-restore default browser alert is a promo, and promo are
  // implemented as IPH. All IPH are disabled by default in EGtests. This flag
  // enable this particular IPH.
  config.iph_feature_enabled = "IPH_iOSPromoPostRestoreDefaultBrowser";

  // Relaunch the app to take the configuration into account.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];
}

#pragma mark - Tests

// Verifies that the promo appears for users who had Chrome set as their default
// browser before a restore. Verifies that secondary action button dismisses the
// promo.
// TODO(crbug.com/418750327): Test is failing on iPhone and iPad.
- (void)DISABLED_testPromoAppears {
  // Simulate setting Chrome as default browser.
  NSMutableDictionary<NSString*, NSObject*>* storage = [[ChromeEarlGrey
      userDefaultsObjectForKey:kDefaultBrowserKey] mutableCopy];
  storage[@"lastHTTPURLOpenTime"] = [NSDate date];
  [ChromeEarlGrey setUserDefaultsObject:storage forKey:kDefaultBrowserKey];

  [self simulateRestore];

  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:TitleMatcher()
                                              timeout:kPromoAppearanceTimeout];
  [self checkThatCommonElementsAreVisible];
  [[EarlGrey selectElementWithMatcher:SecondaryActionMatcher()]
      performAction:grey_tap()];
  [self checkThatCommonElementsAreNotVisible];

  [ChromeEarlGrey removeUserDefaultsObjectForKey:kDefaultBrowserKey];
}

// Verifies that the promo does not appear after a restore if the user did not
// previously set Chrome as their default browser.
- (void)testPromoDoesNotAppear_defaultBrowserNotEnabled {
  [self simulateRestore];
  [self checkThatCommonElementsAreNotVisible];
}

@end
