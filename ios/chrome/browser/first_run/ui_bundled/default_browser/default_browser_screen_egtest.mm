// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
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

- (void)tearDownHelper {
  [self reset];
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Show the First Run UI at startup.
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");
  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByCleanShutdown;

  // Animated Default Browser introduces a new Default Browser screen with a
  // different ID.
  config.additional_args.push_back(
      "--disable-features=AnimatedDefaultBrowserPromoInFRE");

  // The UpdatedFirstRunSequence experiment makes changes to the Default Browser
  // screen's view that cause the test to fail.
  config.additional_args.push_back(
      "--disable-features=UpdatedFirstRunSequence");

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
  [self tapPromoButton:chrome_test_util::ButtonStackPrimaryButton()];
  // Enable history/tab sync.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:
          grey_accessibilityID(kHistorySyncViewAccessibilityIdentifier)];
  [self tapPromoButton:chrome_test_util::ButtonStackPrimaryButton()];
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
  [ChromeEarlGrey clearFakeSyncServerData];
}

// Taps a promo button.
- (void)tapPromoButton:(id<GREYMatcher>)buttonMatcher {
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

// Tests if the default promo is correctly displayed.
- (void)testDefaultPromoDisplayed {
  [[EarlGrey selectElementWithMatcher:DefaultPromoTitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:DefaultPromoSubtitle()]
      assertWithMatcher:grey_sufficientlyVisible()];
}


@end
