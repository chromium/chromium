// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/policy/core/common/policy_loader_ios_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager_constants.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::IdentityCellMatcherForEmail;

namespace {

// Identifier for the main scroll view covering all the screen content.
NSString* const kScrollViewIdentifier =
    @"kPromoStyleScrollViewAccessibilityIdentifier";

// Returns a matcher for the sign-in screen "Continue as <identity>" button.
id<GREYMatcher> GetContinueButtonWithIdentityMatcher(
    FakeChromeIdentity* fakeIdentity) {
  NSString* buttonTitle = l10n_util::GetNSStringF(
      IDS_IOS_FIRST_RUN_SIGNIN_CONTINUE_AS,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));

  return grey_allOf(grey_accessibilityLabel(buttonTitle),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the whole forced sign-in screen.
id<GREYMatcher> GetFirstRunScreenMatcher() {
  return grey_accessibilityID(
      first_run::kFirstRunSignInScreenAccessibilityIdentifier);
}

// Triggers the forced sign-in screen by backgrounding and re-foregrounding the
// app. Waits on the forced sign-in screen to show up.
void TriggerForcedSigninScreen() {
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  [ChromeEarlGrey waitForMatcher:GetFirstRunScreenMatcher()];
}

// Checks that the sync screen is displayed.
void VerifySyncScreenIsDisplayed() {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSyncScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Checks that the forced sign-in prompt is fully dismissed by making sure
// that there isn't any forced sign-in screen displayed.
void VerifyForcedSigninFullyDismissed() {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSyncScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
}

}  // namespace

// Test the forced sign-in screens.
@interface ForcedSigninTestCase : ChromeTestCase

@end

@implementation ForcedSigninTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  // Enable the possibility of using the forced sign-in policy.
  config.additional_args.push_back("--enable-forced-signin-policy");

  // Configure the policy to force sign-in.
  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(
      "<dict><key>BrowserSignin</key><integer>2</integer></dict>");

  return config;
}

// Scrolls down to |elementMatcher| in the scrollable content of the first run
// screen.
void ScrollToElementAndAssertVisibility(id<GREYMatcher> elementMatcher) {
  id<GREYMatcher> scrollView = grey_accessibilityID(kScrollViewIdentifier);

  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(elementMatcher,
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:scrollView] assertWithMatcher:grey_notNil()];
}

#pragma mark - Tests

// Tests the sign-in screen with accounts that are already available.
- (void)testSignInScreenWithAccount {
  // Add an identity to sign-in to enable the "Continue as ..." button in the
  // sign-in screen.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  TriggerForcedSigninScreen();

  // Validate the Title text of the forced sign-in screen.
  id<GREYMatcher> title =
      grey_text(l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE));
  ScrollToElementAndAssertVisibility(title);

  // Validate the Subtitle text of the forced sign-in screen.
  id<GREYMatcher> subtitle = grey_text(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE_MANAGED));
  ScrollToElementAndAssertVisibility(subtitle);

  // Scroll to the "Continue as ..." button to go to the bottom of the screen.
  ScrollToElementAndAssertVisibility(
      GetContinueButtonWithIdentityMatcher(fakeIdentity));

  // Check that there isn't the button to skip sign-in.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_DONT_SIGN_IN))]
      assertWithMatcher:grey_nil()];

  // Touch the continue button to go to the next screen.
  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      performAction:grey_tap()];

  // Make sure that the next screen can be successfully displayed.
  VerifySyncScreenIsDisplayed();
}

// Tests the sign-in screen without accounts where an account has to be added
// before signing in.
- (void)testSignInScreenWithoutAccount {
  TriggerForcedSigninScreen();

  // Tap on the "Sign in" button.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_SIGN_IN_ACTION))]
      performAction:grey_tap()];

  // Check for the fake SSO screen.
  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(kFakeAddAccountViewIdentifier)];
  // Close the SSO view controller.
  id<GREYMatcher> matcher =
      grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(@"Cancel"),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  // Make sure the SSO view controller is fully removed before ending the test.
  // The tear down needs to remove other view controllers, and it cannot be done
  // during the animation of the SSO view controler.
  [ChromeEarlGreyUI waitForAppToIdle];

  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Check that the title of the primary button updates for |fakeIdentity|.
  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check the "Sign in" button isn't there anymore.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_SIGN_IN_ACTION))]
      assertWithMatcher:grey_nil()];

  // Check that there isn't the button to skip sign-in.
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_DONT_SIGN_IN))]
      assertWithMatcher:grey_nil()];
}

// Tests that accounts can be switched and that there is the button add a new
// account.
- (void)testSignInScreenSwitchAccount {
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGrey fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [SigninEarlGrey fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  TriggerForcedSigninScreen();

  // Tap on the account switcher.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];

  // Check that |fakeIdentity2| is displayed.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that 'Add Account' is displayed.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_IDENTITY_CHOOSER_ADD_ACCOUNT))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select |fakeIdentity2|.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      performAction:grey_tap()];

  // Check that the title of the primary button updates for |fakeIdentity2|.
  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity2)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that sync can be turned on from the sync screen.
- (void)testTurnOnSync {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  TriggerForcedSigninScreen();

  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      performAction:grey_tap()];

  VerifySyncScreenIsDisplayed();
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_text(l10n_util::GetNSString(
                                IDS_IOS_FIRST_RUN_SYNC_SCREEN_PRIMARY_ACTION)),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  VerifyForcedSigninFullyDismissed();

  // Check that sign-in is considered as enabled in settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:YES];
}

// Tests that sync is off when the user chooses to not sync.
- (void)testNoSync {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  TriggerForcedSigninScreen();

  [[EarlGrey selectElementWithMatcher:GetContinueButtonWithIdentityMatcher(
                                          fakeIdentity)]
      performAction:grey_tap()];

  VerifySyncScreenIsDisplayed();
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_text(l10n_util::GetNSString(
                         IDS_IOS_FIRST_RUN_SYNC_SCREEN_SECONDARY_ACTION)),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Verify that the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  VerifyForcedSigninFullyDismissed();

  // Check that sign-in is considered as enabled in settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:NO];
}

@end
