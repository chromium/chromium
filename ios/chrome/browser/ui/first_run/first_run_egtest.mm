// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/policy/core/common/policy_loader_ios_constants.h"
#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns a matcher for the welcome screen accept button.
id<GREYMatcher> GetAcceptButton() {
  return grey_allOf(grey_text(l10n_util::GetNSString(
                        IDS_IOS_FIRST_RUN_WELCOME_SCREEN_ACCEPT_BUTTON)),
                    grey_sufficientlyVisible(), nil);
}

}  // namespace

// Test first run stages
@interface FirstRunTestCase : ChromeTestCase

@end

@implementation FirstRunTestCase

- (void)setUp {
  [[self class] testForStartup];

  [super setUp];
  [FirstRunAppInterface setUMACollectionEnabled:NO];
  [FirstRunAppInterface resetUMACollectionEnabledByDefault];
}

- (void)tearDown {
  [super tearDown];
  [FirstRunAppInterface setUMACollectionEnabled:NO];
  [FirstRunAppInterface resetUMACollectionEnabledByDefault];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_disabled.push_back(kLocationPermissionsPrompt);

  config.features_enabled.push_back(kEnableFREUIModuleIOS);

  // Show the First Run UI at startup.
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");

  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByKilling;

  return config;
}

#pragma mark - Helpers

// Checks that the welcome screen is displayed.
- (void)verifyWelcomeScreenIsDisplayed {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunWelcomeScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Checks that the sign in screen is displayed.
- (void)verifySignInScreenIsDisplayed {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Checks that the sync screen is displayed.
- (void)verifySyncScreenIsDisplayed {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSyncScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Scrolls down to |elementMatcher| in the scrollable content of the first run
// screen.
- (void)scrollToElementAndAssertVisibility:(id<GREYMatcher>)elementMatcher {
  id<GREYMatcher> scrollView =
      grey_accessibilityID(@"kFirstRunScrollViewAccessibilityIdentifier");

  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(elementMatcher,
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:scrollView] assertWithMatcher:grey_notNil()];
}

#pragma mark - Tests

// Checks that the Welcome screen is displayed correctly.
- (void)testWelcomeScreenUI {
  [self verifyWelcomeScreenIsDisplayed];

  // Validate the Title text.
  NSString* expectedTitleText =
      [ChromeEarlGrey isIPadIdiom]
          ? l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPAD)
          : l10n_util::GetNSString(
                IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPHONE);
  id<GREYMatcher> title = grey_text(expectedTitleText);
  [self scrollToElementAndAssertVisibility:title];

  // Validate the Subtitle text.
  id<GREYMatcher> subtitle = grey_text(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_SUBTITLE));
  [self scrollToElementAndAssertVisibility:subtitle];

  // Validate the Metrics Consent box.
  id<GREYMatcher> metricsConsent = grey_text(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRICS_CONSENT));
  [self scrollToElementAndAssertVisibility:metricsConsent];

  // Validate the Accept box.
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
}

// Checks that the Sign In screen is displayed correctly.
- (void)testSignInScreenUI {
  [self verifyWelcomeScreenIsDisplayed];

  // Go to the sign-in screen.
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInScreenIsDisplayed];

  // Valide the Title text.
  id<GREYMatcher> title =
      grey_text(l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE));
  [self scrollToElementAndAssertVisibility:title];

  // Validate the Subtitle text.
  id<GREYMatcher> subtitle =
      grey_text(l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE));
  [self scrollToElementAndAssertVisibility:subtitle];
}

// Navigates to the Terms of Service and back.
- (void)testTermsAndConditions {
  // Tap on “Terms of Service” on the first screen
  [self verifyWelcomeScreenIsDisplayed];

  // Scroll to and open the ToS screen.
  id<GREYMatcher> termsOfServiceLink =
      grey_accessibilityLabel(@"Terms of Service");
  [self scrollToElementAndAssertVisibility:termsOfServiceLink];
  [[EarlGrey selectElementWithMatcher:termsOfServiceLink]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_FIRSTRUN_TERMS_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on “Done” on the ToS screen
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Ensure we went back to the First Run screen.
  [self verifyWelcomeScreenIsDisplayed];

  // Scroll to and tap the accept ToS button.
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInScreenIsDisplayed];
}

// Tests that the FRE is shown when incognito is forced by policy.
- (void)testFirstRunWithIncognitoForced {
  AppLaunchConfiguration config = self.appConfigurationForTestCase;

  std::string policy_data = "<dict>"
                            "    <key>IncognitoModeAvailability</key>"
                            "    <integer>2</integer>"
                            "</dict>";
  base::RemoveChars(policy_data, base::kWhitespaceASCII, &policy_data);

  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(policy_data);

  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Verify that the FRE UI is shown while the browser is in incognito mode.
  [self verifyWelcomeScreenIsDisplayed];
}

@end
