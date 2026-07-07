// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

@interface AutofillAndPasswordsTestCase : ChromeTestCase
// Opens the Autofill and Passwords settings page.
- (void)openAutofillAndPasswordsSettings;
@end

@implementation AutofillAndPasswordsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kYourSavedInfoSettingsPageIos);
  return config;
}

- (void)setUp {
  [super setUp];
  // Ensure signed out before each test.
  [ChromeEarlGrey signOutAndClearIdentities];
}

- (void)tearDownHelper {
  // Close any open sub-menus to return to the main settings menu.
  NSError* error = nil;
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      assertWithMatcher:grey_notNil()
                  error:&error];
  if (!error) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
        performAction:grey_tap()];
  }

  // Close the settings menu to ensure a clean state for the next test.
  error = nil;
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      assertWithMatcher:grey_notNil()
                  error:&error];
  if (!error) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
        performAction:grey_tap()];
  }

  [super tearDownHelper];
}

#pragma mark - Helper

- (void)openAutofillAndPasswordsSettings {
  [ChromeEarlGreyUI openSettingsMenu];

  // Tap on Autofill and Passwords row.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsAutofillAndPasswordsCellId)]
      performAction:grey_tap()];
}

// Tests that the sign-in promo is visible on the Autofill and Passwords page
// when the user is not signed in.
- (void)testSignInPromoVisibleWhenLoggedOut {
  [self openAutofillAndPasswordsSettings];

  // Verify the promo is visible in "no accounts" mode.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];

  // Verify the sign-in promo text is visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SIGNIN_PROMO_AUTOFILL_AND_PASSWORDS))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the sign-in promo is not visible when the user is signed in.
- (void)testSignInPromoNotVisibleWhenSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [self openAutofillAndPasswordsSettings];

  // Verify the sign-in promo is not visible.
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that the sign-in promo can be dismissed by tapping the close button.
- (void)testSignInPromoDismiss {
  [self openAutofillAndPasswordsSettings];

  // Verify the promo is visible in "no accounts" mode.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];

  // Verify the sign-in promo text is visible.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SIGNIN_PROMO_AUTOFILL_AND_PASSWORDS))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap the close button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoCloseButtonId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Verify the sign-in promo is not visible.
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that tapping the primary button of the sign-in promo signs the user in
// when there is a device-level account.
- (void)testSignInPromoSignInWithAccount {
  // Set up a fake identity.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self openAutofillAndPasswordsSettings];

  // Verify the promo is visible in "signin with account" mode.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeSigninWithAccount];

  // Tap the promo's primary button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoPrimaryButtonId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Verify the user is signed in and the promo is gone.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests that tapping the primary button of the sign-in promo opens the add
// account flow when there are no accounts on the device.
- (void)testSignInPromoSignInWithoutAccount {
  [self openAutofillAndPasswordsSettings];

  // Verify the promo is visible in "no accounts" mode.
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts];

  // Tap the promo's primary button.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kSigninPromoPrimaryButtonId),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Set up a fake identity to add and sign-in with.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI addFakeAccountInFakeAddAccountMenu:fakeIdentity];

  // Verify the user is signed in and the promo is gone.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

@end
