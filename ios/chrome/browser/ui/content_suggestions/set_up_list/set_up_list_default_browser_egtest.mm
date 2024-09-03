// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/content_suggestions/new_tab_page_app_interface.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using l10n_util::GetNSString;

namespace {

// Returns a matcher to the Default Browser Set Up List item default title.
id<GREYMatcher> DefaultItemTitle() {
  return grey_text(
      GetNSString([ChromeEarlGrey isIPadIdiom]
                      ? IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_TITLE_IPAD
                      : IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_TITLE));
}

// Returns a matcher to the Default Browser see more Set Up List item default
// title.
id<GREYMatcher> DefaultItemSeeMoreTitle() {
  return grey_allOf(
      grey_text(GetNSString(IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_TITLE)),
      grey_accessibilityID(set_up_list::kAccessibilityID), nil);
}

// Returns a matcher to the Default Browser Set Up List item default
// description.
id<GREYMatcher> DefaultItemDescription() {
  return grey_text(
      GetNSString(IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_SHORT_DESCRIPTION));
}

// Returns a matcher to the Default Browser see more Set Up List item default
// description.
id<GREYMatcher> DefaultItemSeeMoreDescription() {
  return grey_allOf(DefaultItemDescription(),
                    grey_accessibilityID(set_up_list::kAccessibilityID), nil);
}

// Returns a matcher to the Default Browser Set Up List item description shown
// to device switchers.
id<GREYMatcher> DeviceSwitcherDescription() {
  return grey_text(GetNSString(
      IDS_IOS_SET_UP_LIST_SEGMENTED_DEFAULT_BROWSER_DEVICE_SWITCHER_SHORT_DESCRIPTION));
}

// Returns a matcher to the Default Browser see more Set Up List item
// description shown to device switchers.
id<GREYMatcher> DeviceSwitcherSeeMoreDescription() {
  return grey_allOf(DeviceSwitcherDescription(),
                    grey_accessibilityID(set_up_list::kAccessibilityID), nil);
}

// Returns a matcher to the Default Browser Set Up List item description shown
// to shopping users.
id<GREYMatcher> ShopperDescription() {
  return grey_text(GetNSString(
      IDS_IOS_SET_UP_LIST_SEGMENTED_DEFAULT_BROWSER_SHOPPER_SHORT_DESCRIPTION));
}

// Returns a matcher to the Default Browser see more Set Up List item
// description shown to shopping users.
id<GREYMatcher> ShopperSeeMoreDescription() {
  return grey_allOf(ShopperDescription(),
                    grey_accessibilityID(set_up_list::kAccessibilityID), nil);
}

// Returns a matcher to the Set Up List Default Browser promo title shown to
// device switchers.
id<GREYMatcher> DeviceSwitcherPromoTitle() {
  return grey_allOf(
      grey_text(GetNSString(
          [ChromeEarlGrey isIPadIdiom]
              ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DEVICE_SWITCHER_TITLE_IPAD
              : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DEVICE_SWITCHER_TITLE_IPHONE)),
      grey_accessibilityID(kPromoStyleTitleAccessibilityIdentifier), nil);
}

// Returns a matcher to the Set Up List Default Browser promo subtitle shown to
// desktop users.
id<GREYMatcher> DesktopUserPromoSubtitle() {
  return grey_allOf(
      grey_text(GetNSString(
          [ChromeEarlGrey isIPadIdiom]
              ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DESKTOP_USER_SUBTITLE_IPAD
              : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DESKTOP_USER_SUBTITLE_IPHONE)),
      grey_accessibilityID(kPromoStyleSubtitleAccessibilityIdentifier), nil);
}

// Returns a matcher to the Set Up List Default Browser promo subtitle shown to
// android switchers.
id<GREYMatcher> AndroidSwitcherPromoSubtitle() {
  return grey_allOf(
      grey_text(GetNSString(
          [ChromeEarlGrey isIPadIdiom]
              ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_ANDROID_SWITCHER_SUBTITLE_IPAD
              : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_ANDROID_SWITCHER_SUBTITLE_IPHONE)),
      grey_accessibilityID(kPromoStyleSubtitleAccessibilityIdentifier), nil);
}

// Returns a matcher to the Set Up List Default Browser promo title shown to
// shopping users.
id<GREYMatcher> ShopperPromoTitle() {
  return grey_allOf(
      grey_text(
          GetNSString(IDS_IOS_SEGMENTED_DEFAULT_BROWSER_SCREEN_SHOPPER_TITLE)),
      grey_accessibilityID(kPromoStyleTitleAccessibilityIdentifier), nil);
}

// Returns a matcher to the Set Up List Default Browser promo subtitle shown to
// shopping users.
id<GREYMatcher> ShopperPromoSubtitle() {
  return grey_allOf(
      grey_text(GetNSString(
          IDS_IOS_SEGMENTED_DEFAULT_BROWSER_SCREEN_SHOPPER_SUBTITLE)),
      grey_accessibilityID(kPromoStyleSubtitleAccessibilityIdentifier), nil);
}

// Returns a matcher to the Set Up List Default Browser promo default title.
id<GREYMatcher> DefaultPromoTitle() {
  return grey_allOf(
      grey_text(
          GetNSString([ChromeEarlGrey isIPadIdiom]
                          ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE_IPAD
                          : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE)),
      grey_accessibilityID(kPromoStyleTitleAccessibilityIdentifier), nil);
}

// Returns a matcher to the Set Up List Default Browser promo default subtitle.
id<GREYMatcher> DefaultPromoSubtitle() {
  return grey_text(
      GetNSString([ChromeEarlGrey isIPadIdiom]
                      ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE_IPAD
                      : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE));
}

}  // namespace

// Tests the Default Browser item in the Set Up List.
@interface SetUpListDefaultBrowserTestCase : ChromeTestCase
@end

@implementation SetUpListDefaultBrowserTestCase

#pragma mark - BaseEarlGreyTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Enable Segmented Default Browser promos and iPad tailored Default Browser
  // promo strings.
  config.features_enabled.push_back(kSegmentedDefaultBrowserPromo);
  config.features_enabled.push_back(kDefaultBrowserPromoIPadExperimentalString);
  // Set first run details to show Set Up List.
  config.additional_args.push_back("-FirstRunRecency");
  config.additional_args.push_back("1");
  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  if ([self isRunningTest:@selector(testDesktopUserPromo)] ||
      [self isRunningTest:@selector(testDesktopUserCompactedItem)] ||
      [self isRunningTest:@selector(testDesktopUserSeeMoreItem)]) {
    config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
    config.additional_args.push_back("Desktop");
  }
  if ([self isRunningTest:@selector(testAndroidSwitcherPromo)] ||
      [self isRunningTest:@selector(testAndroidSwitcherCompactedItem)] ||
      [self isRunningTest:@selector(testAndroidSwitcherSeeMoreItem)]) {
    config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
    config.additional_args.push_back("AndroidPhone");
  }
  if ([self isRunningTest:@selector(testShopperPromo)] ||
      [self isRunningTest:@selector(testShopperCompactedItem)] ||
      [self isRunningTest:@selector(testShopperSeeMoreItem)]) {
    config.additional_args.push_back("-ForceExperienceForShopper");
    config.additional_args.push_back("true");
  }
  return config;
}

- (void)setUp {
  [super setUp];
  [self prepareToTestSetUpListInMagicStack];
}

#pragma mark - Tests

// Tests that the text on the Default Browser compacted Set Up List item shown
// to desktop users is correctly displayed.
- (void)testDesktopUserCompactedItem {
  [[EarlGrey selectElementWithMatcher:DefaultItemTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DeviceSwitcherDescription()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the text on the Default Browser compacted Set Up List item shown
// to android switchers is correctly displayed.
- (void)testAndroidSwitcherCompactedItem {
  [[EarlGrey selectElementWithMatcher:DefaultItemTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DeviceSwitcherDescription()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the text on the Default Browser compacted Set Up List item shown
// to shopping users is correctly displayed.
- (void)testShopperCompactedItem {
  [[EarlGrey selectElementWithMatcher:DefaultItemTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ShopperDescription()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the default text on the Default Browser compacted Set Up List item
// is correctly displayed.
- (void)testDefaultCompactedItem {
  [[EarlGrey selectElementWithMatcher:DefaultItemTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DefaultItemDescription()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the text on the Default Browser see more Set Up List item shown to
// desktop users is correctly displayed.
- (void)testDesktopUserSeeMoreItem {
  [self openSeeMore];
  [[EarlGrey selectElementWithMatcher:DefaultItemSeeMoreTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DeviceSwitcherSeeMoreDescription()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the text on the Default Browser see more Set Up List item shown to
// android switchers is correctly displayed.
- (void)testAndroidSwitcherSeeMoreItem {
  [self openSeeMore];
  [[EarlGrey selectElementWithMatcher:DefaultItemSeeMoreTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DeviceSwitcherSeeMoreDescription()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the text on the Default Browser see more Set Up List item shown to
// shopping users is correctly displayed.
- (void)testShopperSeeMoreItem {
  [self openSeeMore];
  [[EarlGrey selectElementWithMatcher:DefaultItemSeeMoreTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ShopperSeeMoreDescription()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the default text on the Default Browser see more Set Up List item
// is correctly displayed.
- (void)testDefaultSeeMoreItem {
  [self openSeeMore];
  [[EarlGrey selectElementWithMatcher:DefaultItemSeeMoreTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DefaultItemSeeMoreDescription()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the text on the Set Up List Default Browser promo shown to desktop
// users is correctly displayed.
- (void)testDesktopUserPromo {
  [self openPromo];
  [[EarlGrey selectElementWithMatcher:DeviceSwitcherPromoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DesktopUserPromoSubtitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the text on the Set Up List Default Browser promo shown to android
// switchers is correctly displayed.
- (void)testAndroidSwitcherPromo {
  [self openPromo];
  [[EarlGrey selectElementWithMatcher:DeviceSwitcherPromoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:AndroidSwitcherPromoSubtitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the text on the Set Up List Default Browser promo shown to
// shopping users is correctly displayed.
- (void)testShopperPromo {
  [self openPromo];
  [[EarlGrey selectElementWithMatcher:ShopperPromoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:ShopperPromoSubtitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the default text on the Set Up List Default Browser promo is
// correctly displayed.
- (void)testDefaultPromo {
  [self openPromo];
  [[EarlGrey selectElementWithMatcher:DefaultPromoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DefaultPromoSubtitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

#pragma mark - Helpers

// Opens the Set Up List "See More" view.
- (void)openSeeMore {
  id<GREYMatcher> seeMoreButton =
      grey_allOf(grey_text(@"See more"), grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:seeMoreButton] performAction:grey_tap()];
  // Swipe up to expand the "See more" view.
  id<GREYMatcher> setUpListSubtitle = chrome_test_util::ContainsPartialText(
      @"Complete these suggested actions below");
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:setUpListSubtitle];
  if (![ChromeEarlGrey isIPadIdiom]) {
    [[EarlGrey selectElementWithMatcher:setUpListSubtitle]
        performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  }
}

- (void)openPromo {
  [[EarlGrey selectElementWithMatcher:DefaultItemTitle()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier)];
}

// Sets conditions so that the Set Up List will display with no completed items.
- (void)prepareToTestSetUpListInMagicStack {
  [ChromeEarlGrey writeFirstRunSentinel];
  [ChromeEarlGrey clearDefaultBrowserPromoData];
  [NewTabPageAppInterface resetSetUpListPrefs];
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];
}

@end
