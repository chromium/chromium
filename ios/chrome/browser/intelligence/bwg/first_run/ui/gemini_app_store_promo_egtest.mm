// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_matchers.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/first_run/public/first_run_constants.h"
#import "ios/chrome/browser/first_run/test/first_run_test_case_base.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Accessibility identifier for the bubble view label.
NSString* const kBubbleViewLabelIdentifier = @"BubbleViewLabelIdentifier";

// Matcher for slide titles in the visual-rich carousel.
id<GREYMatcher> VisualRichSlideTitle(int string_id) {
  NSString* baseTitle = l10n_util::GetNSString(string_id);
  NSString* suffix =
      l10n_util::GetNSString(IDS_IOS_BWG_PROMO_CAROUSEL_GEMINI_IN_CHROME);
  NSString* expectedTitle =
      (string_id == IDS_IOS_BWG_PROMO_CAROUSEL_SUMMARIZE_TITLE)
          ? [NSString stringWithFormat:@"%@\n%@", baseTitle, suffix]
          : [NSString stringWithFormat:@"%@ %@", baseTitle, suffix];
  return grey_allOf(grey_accessibilityLabel(expectedTitle),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for any sufficiently visible slide title in the carousel.
id<GREYMatcher> AnyVisualRichSlideTitle() {
  return grey_anyOf(
      VisualRichSlideTitle(IDS_IOS_BWG_PROMO_CAROUSEL_SUMMARIZE_TITLE),
      VisualRichSlideTitle(IDS_IOS_BWG_PROMO_CAROUSEL_SHOPPING_TITLE),
      VisualRichSlideTitle(IDS_IOS_BWG_PROMO_CAROUSEL_PLANNING_TITLE), nil);
}

// Matcher for the title in the lightweight FRE.
id<GREYMatcher> LightweightFRETitle(int string_id) {
  NSString* expectedTitle = l10n_util::GetNSString(string_id);
  return grey_allOf(grey_accessibilityLabel(expectedTitle),
                    grey_accessibilityTrait(UIAccessibilityTraitHeader),
                    grey_sufficientlyVisible(), nil);
}

// Matcher for any lightweight FRE title.
id<GREYMatcher> AnyLightweightFRETitle() {
  return grey_anyOf(
      LightweightFRETitle(IDS_IOS_BWG_LIGHTWEIGHT_PROMO_PAGE_SHARING_TITLE),
      LightweightFRETitle(IDS_IOS_BWG_LIGHTWEIGHT_PROMO_DIVERSE_TITLE),
      LightweightFRETitle(IDS_IOS_BWG_LIGHTWEIGHT_PROMO_CONVENIENCE_TITLE),
      nil);
}

// Matcher for the main title header in the Gemini promo or FRE.
id<GREYMatcher> GeminiPromoMainTitle() {
  id<GREYMatcher> legacyOrRefactoredTitle =
      grey_allOf(grey_accessibilityLabel(
                     l10n_util::GetNSString(IDS_IOS_BWG_PROMO_MAIN_TITLE)),
                 grey_accessibilityTrait(UIAccessibilityTraitHeader), nil);
  return grey_anyOf(legacyOrRefactoredTitle, AnyVisualRichSlideTitle(),
                    AnyLightweightFRETitle(), nil);
}

// Helper to retrieve the XCUIApplication instance.
XCUIApplication* GetApplication() {
  static XCUIApplication* app = [[XCUIApplication alloc] init];
  return app;
}

// Backgrounds the app.
void BackgroundApp() {
  GREYAssertTrue([[AppLaunchManager sharedManager] backgroundApplication],
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
  config.iph_feature_enabled = "IPH_iOSGeminiExternalAppStoreEvent";
  return config;
}

// Tests that the Gemini FRE promo shows on a fresh install after deep link and
// signing in.
- (void)testAppStorePromoFreshInstallSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kCanUseModelExecutionFeaturesName) : @YES,
                   @(kCanUseGeminiInChromeCapabilityName) : @YES,
                 }];

  SimulateGeminiPromoURLOpening();
  [ChromeEarlGreyUI waitForAppToIdle];

  // Sign in on Welcome screen.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
      performAction:grey_tap()];

  // Wait for History Sync screen.
  [ChromeEarlGrey waitForMatcher:grey_accessibilityID(
                                     kHistorySyncViewAccessibilityIdentifier)];

  // Accept History Sync.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackPrimaryButton()]
      performAction:grey_tap()];

  // Wait for Default Browser screen and dismiss it.
  [ChromeEarlGrey
      waitForMatcher:
          grey_accessibilityID(
              first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier)];
  [FirstRunTestCaseBase dismissDefaultBrowser];

  // Wait for Best Features screen and dismiss it.
  [ChromeEarlGrey
      waitForMatcher:
          grey_accessibilityID(
              first_run::kBestFeaturesMainScreenAccessibilityIdentifier)];
  id<GREYMatcher> bestFeaturesButtonMatcher = grey_allOf(
      grey_ancestor(grey_accessibilityID(
          first_run::kBestFeaturesMainScreenAccessibilityIdentifier)),
      chrome_test_util::ButtonStackPrimaryButton(), nil);
  [[EarlGrey selectElementWithMatcher:bestFeaturesButtonMatcher]
      performAction:grey_tap()];

  // Verify user signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:GeminiPromoMainTitle()];

  [ChromeEarlGrey waitForPageToFinishLoading];

  // Dismiss Gemini Promo screen.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
      performAction:grey_tap()];

  // In Next IA, the Page Action Menu onboarding IPH is suppressed.
  if ([ChromeEarlGrey isChromeNextEnabled]) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kBubbleViewLabelIdentifier)]
        assertWithMatcher:grey_notVisible()];
  } else {
    // Verify that the specialized IPH bubble is displayed.
    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                        grey_accessibilityID(kBubbleViewLabelIdentifier)];
  }
}

// Tests that the sign-in sheet is presented on a fresh install after deep link,
// and dismissing it leaves the user signed out with no promo shown.
- (void)testAppStorePromoFreshInstallSignedOut {
  SimulateGeminiPromoURLOpening();

  // Skip sign-in on Welcome screen.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
      performAction:grey_tap()];

  // Dismiss Default Browser and remaining screens.
  [FirstRunTestCaseBase dismissDefaultBrowserAndRemainingScreens];

  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(kFakeAuthActivityViewIdentifier)];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthCancelButtonIdentifier)]
      performAction:grey_tap()];

  // Verify that the specialized IPH bubble is not visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kBubbleViewLabelIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the Gemini FRE promo shows for an existing signed-in user after
// deep link.
- (void)testAppStorePromoExistingUserSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kCanUseModelExecutionFeaturesName) : @YES,
                   @(kCanUseGeminiInChromeCapabilityName) : @YES,
                 }];

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

  // In Next IA, the Page Action Menu onboarding IPH is suppressed.
  if ([ChromeEarlGrey isChromeNextEnabled]) {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kBubbleViewLabelIdentifier)]
        assertWithMatcher:grey_notVisible()];
  } else {
    // Verify that the specialized IPH bubble is displayed.
    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:
                        grey_accessibilityID(kBubbleViewLabelIdentifier)];
  }
}

// Tests that the sign-in sheet is presented for an existing signed-out user
// after deep link, and dismissing it leaves the user signed out with no promo.
- (void)testAppStorePromoExistingUserSignedOut {
  // Dismiss FRE to simulate existing user.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonStackSecondaryButton()]
      performAction:grey_tap()];
  [FirstRunTestCaseBase dismissDefaultBrowserAndRemainingScreens];

  SimulateGeminiPromoURLOpening();

  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(kFakeAuthActivityViewIdentifier)];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthCancelButtonIdentifier)]
      performAction:grey_tap()];

  // Verify that the specialized IPH bubble is not visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kBubbleViewLabelIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

@end
