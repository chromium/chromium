// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/first_run/public/first_run_constants.h"
#import "ios/chrome/browser/first_run/test/first_run_test_case_base.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Matcher for the main title header in the Gemini promo.
id<GREYMatcher> GeminiPromoMainTitle() {
  return grey_allOf(grey_accessibilityLabel(
                        l10n_util::GetNSString(IDS_IOS_BWG_PROMO_MAIN_TITLE)),
                    grey_accessibilityTrait(UIAccessibilityTraitHeader), nil);
}

// Helper to retrieve the XCUIApplication instance.
XCUIApplication* GetApplication() {
  static XCUIApplication* app = [[XCUIApplication alloc] init];
  return app;
}

// Backgrounds the app.
void BackgroundApp() {
  [[XCUIDevice sharedDevice] pressButton:XCUIDeviceButtonHome];
  XCUIApplication* currentApplication = GetApplication();
  ConditionBlock condition = ^BOOL {
    return currentApplication.state == XCUIApplicationStateRunningBackground ||
           currentApplication.state ==
               XCUIApplicationStateRunningBackgroundSuspended;
  };
  // A 20-second timeout is used here to give slow bots sufficient time to
  // transition the application to the background.
  GREYAssertTrue(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(20),
                                                              condition),
                 @"Failed to background application.");
}

// Simulates opening the Gemini promo URL.
void SimulateGeminiPromoURLOpening() {
  BackgroundApp();
  [ChromeEarlGrey
      simulateExternalAppURLOpeningWithURL:
          [NSURL
              URLWithString:
                  @"googlechromes://ChromeExternalAction/appstoregeminipromo"]];
  [GetApplication() activate];
}

}  // namespace

// Test suite for Gemini promo from App Store external events.
@interface GeminiAppStorePromoEGTest : FirstRunTestCaseBase
@end

@implementation GeminiAppStorePromoEGTest

+ (BOOL)forceRestartAndWipe {
  return YES;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.additional_args.push_back(
      "--disable-features=UpdatedFirstRunSequence");
  config.additional_args.push_back(
      "--disable-features=AnimatedDefaultBrowserPromoInFRE");
  config.features_enabled.push_back(kPageActionMenu);
  config.features_enabled.push_back(kAppStoreInAppEvents);
  config.iph_feature_enabled = "IPH_iOSGeminiExternalAppStoreEvent";
  return config;
}

// Tests that the Gemini FRE promo shows on a fresh install after deep link and
// signing in.
// TODO(crbug.com/509887034): Fails on official test and other bots.
- (void)DISABLED_testAppStorePromoFreshInstallSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  SimulateGeminiPromoURLOpening();

  // Sign in on Welcome screen.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
      performAction:grey_tap()];

  // Accept History Sync.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
      performAction:grey_tap()];

  // Dismiss Default Browser and remaining screens.
  [FirstRunTestCaseBase dismissDefaultBrowserAndRemainingScreens];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:GeminiPromoMainTitle()];

  [ChromeEarlGrey waitForPageToFinishLoading];

  // Dismiss Gemini Promo screen.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
      performAction:grey_tap()];

  // Verify that the specialized IPH bubble is displayed.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_accessibilityID(@"BubbleViewLabelIdentifier")];
}

// Tests that the snackbar shows on a fresh install after deep link if user
// stays signed out.
- (void)testAppStorePromoFreshInstallSignedOut {
  SimulateGeminiPromoURLOpening();

  // Skip sign-in on Welcome screen.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
      performAction:grey_tap()];

  // Dismiss Default Browser and remaining screens.
  [FirstRunTestCaseBase dismissDefaultBrowserAndRemainingScreens];

  // Verify that the snackbar appears.
  id<GREYMatcher> snackbarMatcher = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_GEMINI_SIGN_IN_REQUIRED_SNACKBAR));
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:snackbarMatcher];

  // Verify that the specialized IPH bubble is not visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"BubbleViewLabelIdentifier")]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Gemini FRE promo shows for an existing signed-in user after
// deep link.
// TODO(crbug.com/509887034): Fails on official test and other bots.
- (void)DISABLED_testAppStorePromoExistingUserSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Dismiss FRE to simulate existing user.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
      performAction:grey_tap()];
  [FirstRunTestCaseBase dismissDefaultBrowserAndRemainingScreens];

  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  SimulateGeminiPromoURLOpening();
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:GeminiPromoMainTitle()];

  [ChromeEarlGrey waitForPageToFinishLoading];

  // Dismiss Gemini Promo screen.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
      performAction:grey_tap()];

  // Verify that the specialized IPH bubble is displayed.
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                      grey_accessibilityID(@"BubbleViewLabelIdentifier")];
}

// Tests that the snackbar shows for an existing signed-out user after deep
// link.
- (void)testAppStorePromoExistingUserSignedOut {
  // Dismiss FRE to simulate existing user.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
      performAction:grey_tap()];
  [FirstRunTestCaseBase dismissDefaultBrowserAndRemainingScreens];

  SimulateGeminiPromoURLOpening();
  id<GREYMatcher> snackbarMatcher = grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_GEMINI_SIGN_IN_REQUIRED_SNACKBAR));
  [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:snackbarMatcher];

  // Verify that the specialized IPH bubble is not visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          @"BubbleViewLabelIdentifier")]
      assertWithMatcher:grey_notVisible()];
}

@end
