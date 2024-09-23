// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
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

// Constant for timeout while waiting for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);

// Returns a matcher for the device switcher promo title.
id<GREYMatcher> DeviceSwitcherPromoTitle() {
  return grey_text(GetNSString(
      [ChromeEarlGrey isIPadIdiom]
          ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DEVICE_SWITCHER_TITLE_IPAD
          : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DEVICE_SWITCHER_TITLE_IPHONE));
}

// Returns a matcher for the desktop user promo subtitle.
id<GREYMatcher> DesktopUserPromoSubtitle() {
  return grey_text(GetNSString(
      [ChromeEarlGrey isIPadIdiom]
          ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DESKTOP_USER_SUBTITLE_IPAD
          : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_DESKTOP_USER_SUBTITLE_IPHONE));
}

// Returns a matcher for the android switcher promo subtitle.
id<GREYMatcher> AndroidSwitcherPromoSubtitle() {
  return grey_text(GetNSString(
      [ChromeEarlGrey isIPadIdiom]
          ? IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_ANDROID_SWITCHER_SUBTITLE_IPAD
          : IDS_IOS_FIRST_RUN_SEGMENTED_DEFAULT_BROWSER_ANDROID_SWITCHER_SUBTITLE_IPHONE));
}

// Returns a matcher for the default promo title.
id<GREYMatcher> DefaultPromoTitle() {
  return grey_text(
      GetNSString([ChromeEarlGrey isIPadIdiom]
                      ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE_IPAD
                      : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE));
}

// Returns a matcher for the default promo subtitle.
id<GREYMatcher> DefaultPromoSubtitle() {
  return grey_text(
      GetNSString([ChromeEarlGrey isIPadIdiom]
                      ? IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE_IPAD
                      : IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE));
}

}  // namespace

// Test Default Browser promo that appears during the First Run Experience.
@interface DefaultBrowserScreenTestCase : ChromeTestCase
@end

@implementation DefaultBrowserScreenTestCase

#pragma mark - BaseEarlGreyTestCase

+ (void)setUpForTestCase {
  // Leave this empty so that the FRE shows during the first test.
}

- (void)setUp {
  [super setUp];
  [self signIn];
}

- (void)tearDown {
  [self reset];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Enable Segmented Default Browser promos and iPad tailored Default Browser
  // promo strings.
  config.features_enabled.push_back(kSegmentedDefaultBrowserPromo);
  config.features_enabled.push_back(kDefaultBrowserPromoIPadExperimentalString);
  // Show the First Run UI at startup.
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");
  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  if ([self isRunningTest:@selector(testDesktopUserPromoDisplayed)]) {
    config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
    config.additional_args.push_back("Desktop");
  }
  if ([self isRunningTest:@selector(testAndroidSwitcherPromoDisplayed)]) {
    config.additional_args.push_back("-ForceExperienceForDeviceSwitcher");
    config.additional_args.push_back("AndroidPhone");
  }
  if ([self isRunningTest:@selector(DISABLED_testShopperPromoNotDisplayed)]) {
    config.additional_args.push_back("-ForceExperienceForShopper");
    config.additional_args.push_back("true");
  }
  return config;
}

#pragma mark - Helpers

// Signs in and enables sync.
- (void)signIn {
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunSignInScreenAccessibilityIdentifier)];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  // Sign in.
  [self tapPromoButton:kPromoStylePrimaryActionAccessibilityIdentifier];
  // Enable history/tab sync.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier)];
  [self tapPromoButton:kPromoStylePrimaryActionAccessibilityIdentifier];
  [ChromeEarlGrey
      waitForSyncTransportStateActiveWithTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier)];
}

// Signs out and clears sync data.
- (void)reset {
  [SigninEarlGrey signOut];
  [ChromeEarlGrey waitForSyncEngineInitialized:NO
                                   syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey clearFakeSyncServerData];
}

// Taps a promo button.
- (void)tapPromoButton:(NSString*)buttonID {
  id<GREYMatcher> buttonMatcher = grey_accessibilityID(buttonID);
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kPromoStyleScrollViewAccessibilityIdentifier);
  // Needs to scroll slowly to make sure to not miss a cell if it is not
  // currently on the screen. It should not be bigger than the visible part
  // of the collection view.
  id<GREYAction> searchAction = grey_scrollInDirection(kGREYDirectionDown, 200);
  GREYElementInteraction* element =
      [[EarlGrey selectElementWithMatcher:buttonMatcher]
             usingSearchAction:searchAction
          onElementWithMatcher:scrollViewMatcher];
  [element performAction:grey_tap()];
}

#pragma mark - Tests

// Tests if the desktop user promo is correctly displayed.
- (void)testDesktopUserPromoDisplayed {
  [[EarlGrey selectElementWithMatcher:DeviceSwitcherPromoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DesktopUserPromoSubtitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests if the android switcher promo is correctly displayed.
- (void)testAndroidSwitcherPromoDisplayed {
  [[EarlGrey selectElementWithMatcher:DeviceSwitcherPromoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:AndroidSwitcherPromoSubtitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests if the default promo is correctly displayed.
- (void)testDefaultPromoDisplayed {
  [[EarlGrey selectElementWithMatcher:DefaultPromoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DefaultPromoSubtitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the shopping user promo is not displayed when the Shopper
// experience is forced through experimental settings. Shopper segmentation
// information should not be available during first run.
// TODO(crbug.com/360395573): Enable after modifying segmented default browser
// utils to add this check.
- (void)DISABLED_testShopperPromoNotDisplayed {
  [[EarlGrey selectElementWithMatcher:DefaultPromoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DefaultPromoSubtitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
