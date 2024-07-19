// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

// Integration tests using the Account Menu.
@interface AccountMenuTestCase : WebHttpServerChromeTestCase
@end

@implementation AccountMenuTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.features_enabled.push_back(kIdentityDiscAccountMenu);

  return config;
}

- (void)tearDown {
  base::TimeDelta marginToAllowIdentityConfirmationSnackbar = base::Days(20);
  [ChromeEarlGrey
      setTimeValue:base::Time::FromDeltaSinceWindowsEpoch(
                       marginToAllowIdentityConfirmationSnackbar)
       forUserPref:prefs::kIdentityConfirmationSnackbarLastPromptTime];
  [super tearDown];
}

- (void)updateLastSignInToPastDate {
  base::TimeDelta marginBetweenLastSigninAndIdentityConfirmationPrompt =
      base::Days(20);
  [ChromeEarlGrey
      setTimeValue:base::Time::FromDeltaSinceWindowsEpoch(
                       marginBetweenLastSigninAndIdentityConfirmationPrompt)
       forUserPref:prefs::kLastSigninTimestamp];
}

- (void)testViewAccountMenu {
  // Sign in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Select the identity disc particle.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kNTPFeedHeaderIdentityDisc)]
      performAction:grey_tap()];

  // Ensure the Account Menu is displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kAccountMenuTableViewId),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

- (void)testCloseButtonAccountMenu {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"The close button exists only on iPhones.");
  }

  // Sign in.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Select the identity disc particle.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kNTPFeedHeaderIdentityDisc)]
      performAction:grey_tap()];

  // Ensure the Account Menu is displayed.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kAccountMenuTableViewId),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the Close button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAccountMenuCloseButtonId)]
      performAction:grey_tap()];

  // Verify the Account Menu is dismissed.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kAccountMenuTableViewId),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notVisible()];
}

// Verifies identity confirmation snackbar shows on startup with multiple
// identities on device.
- (void)testMultipleIdentities_IdentityConfirmationToast {
  // Add multiple identities and sign in with one of them.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  FakeSystemIdentity* secondaryIdentity = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:secondaryIdentity];
  [self updateLastSignInToPastDate];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Confirm the snackbar shows.
  NSString* snackbarMessage = l10n_util::GetNSStringF(
      IDS_IOS_ACCOUNT_MENU_SWITCH_CONFIRMATION_TITLE,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:grey_text(snackbarMessage)];
}

// Verifies no identity confirmation snackbar shows on startup with only one
// identity on device.
- (void)testSingleIdentity_IdentityConfirmationToast {
  // Add multiple identities and sign in with one of them.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [self updateLastSignInToPastDate];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Confirm the snackbar does not show.
  NSString* snackbarMessage = l10n_util::GetNSStringF(
      IDS_IOS_ACCOUNT_MENU_SWITCH_CONFIRMATION_TITLE,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(snackbarMessage),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

// Verifies identity confirmation snackbar shows on startup with multiple
// identities on device with frequency limitations.
- (void)testFrequencyLimitation_IdentityConfirmationToast {
  // Add multiple identities and sign in with one of them.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  FakeSystemIdentity* secondaryIdentity = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:secondaryIdentity];
  [self updateLastSignInToPastDate];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Confirm the snackbar shows.
  NSString* snackbarMessage = l10n_util::GetNSStringF(
      IDS_IOS_ACCOUNT_MENU_SWITCH_CONFIRMATION_TITLE,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(snackbarMessage),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Dismiss the snackabr.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(snackbarMessage),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Background then foreground the app again.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Confirm the snackbar does not show.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(snackbarMessage),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

// Verifies identity confirmation snackbar on startup does not show after a
// recent sign-in.
- (void)testRecentSignin_IdentityConfirmationToast {
  // Add multiple identities and sign in with one of them.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  FakeSystemIdentity* secondaryIdentity = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:secondaryIdentity];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Confirm the snackbar does not show.
  NSString* snackbarMessage = l10n_util::GetNSStringF(
      IDS_IOS_ACCOUNT_MENU_SWITCH_CONFIRMATION_TITLE,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_text(snackbarMessage),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

@end
