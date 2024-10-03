// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using l10n_util::GetNSString;

// Tests messaging in the recurring Default Browser video promo.
@interface DefaultBrowserGenericPromoTestCase : ChromeTestCase
@end

@implementation DefaultBrowserGenericPromoTestCase

#pragma mark - BaseEarlGreyTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Enable Segmented Default Browser Promos.
  config.features_enabled.push_back(kSegmentedDefaultBrowserPromo);
  // Show the promo at startup.
  config.additional_args.push_back("-enable-promo-manager-fullscreen-promos");
  config.additional_args.push_back("-NextPromoForDisplayOverride");
  config.additional_args.push_back("promos_manager::Promo::DefaultBrowser");
  config.additional_args.push_back("-FirstRunRecency");
  config.additional_args.push_back("1");
  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  if ([self isRunningTest:@selector(testDesktopUserPromoDisplayed)]) {
    config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
    config.additional_args.push_back("Desktop");
  }
  if ([self isRunningTest:@selector
            (DISABLED_testAndroidSwitcherPromoDisplayed)]) {
    config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
    config.additional_args.push_back("AndroidPhone");
  }
  if ([self isRunningTest:@selector(testShopperPromoDisplayed)]) {
    config.additional_args.push_back("-ForceExperienceForShopper");
    config.additional_args.push_back("true");
  }
  return config;
}

+ (void)setUpForTestCase {
  // Leave this empty so that the promo shows for the first test.
}

- (void)setUp {
  [super setUp];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kConfirmationAlertTitleAccessibilityIdentifier)];
}

#pragma mark - Tests

// Tests if the desktop user promo text is correctly displayed.
- (void)testDesktopUserPromoDisplayed {
  [[EarlGrey
      selectElementWithMatcher:
          grey_text(GetNSString(
              IDS_IOS_SEGMENTED_DEFAULT_BROWSER_VIDEO_PROMO_DEVICE_SWITCHER_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests if the android switcher promo text is correctly displayed.
// TODO(crbug.com/371185539): fails consistently.
- (void)DISABLED_testAndroidSwitcherPromoDisplayed {
  [[EarlGrey
      selectElementWithMatcher:
          grey_text(GetNSString(
              IDS_IOS_SEGMENTED_DEFAULT_BROWSER_VIDEO_PROMO_DEVICE_SWITCHER_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests if the shopping user promo text is correctly displayed.
- (void)testShopperPromoDisplayed {
  [[EarlGrey
      selectElementWithMatcher:
          grey_text(GetNSString(
              IDS_IOS_SEGMENTED_DEFAULT_BROWSER_VIDEO_PROMO_SHOPPER_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests if the default promo text is correctly displayed.
- (void)testDefaultPromoDisplayed {
  [[EarlGrey selectElementWithMatcher:
                 grey_text(GetNSString(
                     IDS_IOS_DEFAULT_BROWSER_VIDEO_PROMO_TITLE_TEXT))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
