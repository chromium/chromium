// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/signin/internal/identity_manager/account_capabilities_constants.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/authentication/age_mismatch_signout/ui/age_mismatch_signout_ui_constants.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/first_run/public/first_run_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/safari_data_import/public/utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/common/ui/button_stack/button_stack_constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

namespace {

// Matcher for the primary button on the age mismatch prompt.
id<GREYMatcher> AgeMismatchPrimaryButton() {
  return grey_allOf(
      grey_accessibilityID(kButtonStackPrimaryActionAccessibilityIdentifier),
      grey_ancestor(
          grey_accessibilityID(kAgeMismatchSignoutViewAccessibilityIdentifier)),
      nil);
}

// Matcher for the secondary button on the age mismatch prompt.
id<GREYMatcher> AgeMismatchSecondaryButton() {
  return grey_allOf(
      grey_accessibilityID(kButtonStackSecondaryActionAccessibilityIdentifier),
      grey_ancestor(
          grey_accessibilityID(kAgeMismatchSignoutViewAccessibilityIdentifier)),
      nil);
}

}  // namespace

// Test case for the age mismatch signout prompt.
@interface AgeMismatchSignoutTestCase : ChromeTestCase
@end

@implementation AgeMismatchSignoutTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled_and_params.push_back(
      {switches::kBuildExternalPrivacyContext,
       {{"AgeMismatchLearnMoreUrl", "about:blank"}}});
  config.features_enabled.push_back(
      switches::kEnforceCanSignInToChromeCapability);
  return config;
}

// Helper to sign in the user, set capability to NO, and relaunch with age
// mismatch prompt active.
- (void)signInAndRelaunchWithAgeMismatch {
  // Ensure the app is running without `BuildExternalPrivacyContext` enabled
  // initially.
  AppLaunchConfiguration initConfig;
  initConfig.features_enabled.push_back(
      switches::kEnforceCanSignInToChromeCapability);
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:initConfig];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kCanSignInToChromeCapabilityName) : @NO,
                 }];

  // Sign in.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Relaunch the app with `BuildExternalPrivacyContext` enabled.
  AppLaunchConfiguration config;
  config.additional_args.push_back(std::string("-") +
                                   test_switches::kAddFakeIdentitiesAtStartup);
  config.features_enabled_and_params.push_back(
      {switches::kBuildExternalPrivacyContext,
       {{"AgeMismatchLearnMoreUrl", "about:blank"}}});
  config.features_enabled.push_back(
      switches::kEnforceCanSignInToChromeCapability);
  config.relaunch_policy = ForceRelaunchByKilling;
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Verify the prompt is shown on startup.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:AgeMismatchPrimaryButton()];
}

// Helper to initiate sign-in from the New Tab Page with an account that
// triggers the age mismatch prompt.
- (void)signInFromNTPWithAgeMismatch {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kCanSignInToChromeCapabilityName) : @NO,
                 }];

  // Tap the identity disc on NTP.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kNTPFeedHeaderIdentityDisc)]
      performAction:grey_tap()];

  // Tap "Continue as..." (Primary button) on the consistency sign-in sheet.
  [ChromeEarlGrey
      waitForMatcher:
          grey_accessibilityID(
              kConsistencySigninPrimaryButtonAccessibilityIdentifier)];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kConsistencySigninPrimaryButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // The age mismatch prompt should now be shown.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:AgeMismatchPrimaryButton()];
}

// Tests that the age mismatch prompt is shown to an existing signed-in account
// when its capability is updated to false.
- (void)testAgeMismatchPromptForExistingAccount {
  [self signInAndRelaunchWithAgeMismatch];

  // Verify the user is signed out.
  [SigninEarlGrey verifySignedOut];

  // Verify the identity view is NOT shown (`kStandard` mode).
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kAgeMismatchIdentityViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  // Tap "Use without an account".
  [[EarlGrey selectElementWithMatcher:AgeMismatchSecondaryButton()]
      performAction:grey_tap()];

  // Verify the prompt is dismissed.
  [[EarlGrey selectElementWithMatcher:AgeMismatchPrimaryButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests the primary action button ("Add another account") on the prompt.
- (void)testAgeMismatchPromptPrimaryButton {
  [self signInFromNTPWithAgeMismatch];

  // Tap "Use another account".
  [[EarlGrey selectElementWithMatcher:AgeMismatchPrimaryButton()]
      performAction:grey_tap()];

  // Verify the consistency sign-in bottom sheet is shown.
  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(
                         kConsistencySigninAccessibilityIdentifier)];
}

// Tests that the age mismatch prompt is shown during the First Run Experience.
- (void)testAgeMismatchPromptInFRE {
  // Relaunch the app to force First Run Experience.
  AppLaunchConfiguration config;
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");

  // Disable features that change the FRE sequence to keep the test simple.
  config.features_disabled.push_back(first_run::kUpdatedFirstRunSequence);
  config.features_disabled.push_back(
      first_run::kAnimatedDefaultBrowserPromoInFRE);
  config.features_disabled.push_back(kBestOfAppFRE);

  // Override the Learn More URL to about:blank.
  config.features_enabled_and_params.push_back(
      {switches::kBuildExternalPrivacyContext,
       {{"AgeMismatchLearnMoreUrl", "about:blank"}}});
  config.features_enabled.push_back(
      switches::kEnforceCanSignInToChromeCapability);

  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // We should see the FRE Signin screen.
  // Add the identity dynamically with `can_sign_in_to_chrome` = NO capability.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity
                 withCapabilities:@{
                   @(kCanSignInToChromeCapabilityName) : @NO,
                 }];

  // Tap "Continue as..." (Primary button) to sign in.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kButtonStackPrimaryActionAccessibilityIdentifier)]
      performAction:grey_tap()];

  // The age mismatch prompt should now be shown.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:AgeMismatchPrimaryButton()];

  // Verify the identity view is shown (`kSigninFlow` mode).
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kAgeMismatchIdentityViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap "Use without an account" (Secondary button).
  [[EarlGrey selectElementWithMatcher:AgeMismatchSecondaryButton()]
      performAction:grey_tap()];

  // Wait for the next FRE screen (Default Browser or Safari Data Import).
  id<GREYMatcher> nextScreenMatcher = grey_anyOf(
      grey_accessibilityID(
          first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier),
      grey_accessibilityID(GetSafariDataEntryPointAccessibilityIdentifier()),
      nil);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:nextScreenMatcher];
}

// Tests that the age mismatch prompt is shown when signing in from NTP
// with an account that has minor capabilities.
- (void)testAgeMismatchPromptDuringNTPSignin {
  [self signInFromNTPWithAgeMismatch];

  // Verify the identity view is shown (`kSigninFlow` mode).
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kAgeMismatchIdentityViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap "Use without an account" (Secondary button).
  [[EarlGrey selectElementWithMatcher:AgeMismatchSecondaryButton()]
      performAction:grey_tap()];

  // Verify the prompt is dismissed.
  [[EarlGrey selectElementWithMatcher:AgeMismatchPrimaryButton()]
      assertWithMatcher:grey_notVisible()];
}

@end
