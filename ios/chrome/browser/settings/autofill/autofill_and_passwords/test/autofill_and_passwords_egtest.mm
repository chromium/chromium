// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "components/autofill/core/common/autofill_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/device_reauth/test/reauthentication_app_interface.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/public/autofill_ai_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_constants.h"
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

@implementation AutofillAndPasswordsTestCase {
  NSString* _testEntityUUID;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];
  config.features_enabled.push_back(kYourSavedInfoSettingsPageIos);
  config.features_enabled.push_back(
      autofill::features::kAutofillAiWithDataSchema);
  config.features_enabled.push_back(
      autofill::features::kAutofillAiReauthRequired);
  config.features_enabled.push_back(
      autofill::features::kAutofillAmbientAutofill);
  return config;
}

- (void)setUp {
  [super setUp];
  _testEntityUUID = nil;
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

  if (_testEntityUUID) {
    [AutofillAppInterface removeEntityWithUUID:_testEntityUUID];
    _testEntityUUID = nil;
  }

  [ReauthenticationAppInterface mockReauthenticationModuleCanAttempt:NO];
  [ReauthenticationAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  [ReauthenticationAppInterface mockReauthenticationModuleShouldSkipReAuth:YES];
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

// Tests navigating to each subpage from "Autofill and passwords" and verifies
// that each correct target page opens with the expected title and elements.
- (void)testNavigateToAllSubpages {
  [ReauthenticationAppInterface mockReauthenticationModuleCanAttempt:YES];
  [ReauthenticationAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];

  [self openAutofillAndPasswordsSettings];

  // 1. Password Manager.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsPasswordsCellId)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kPasswordsTableViewID)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];

  // 2. Payments.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsPaymentMethodsCellId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillCreditCardTableViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_AUTOFILL_PAYMENTS_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];

  // 3. Contact info.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsAddressesAndMoreCellId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kAutofillProfileTableViewID)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_AUTOFILL_CONTACT_INFO_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];

  // 4. Identity docs.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsIdentityDocsCellId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_AUTOFILL_IDENTITY_DOCS_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];

  // 5. Travel.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsTravelInfoCellId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_AUTOFILL_TRAVEL_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];

  // 6. Shopping.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsShoppingInfoCellId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_AUTOFILL_SHOPPING_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];

  // 7. Autofill Settings.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsAutofillSettingsCellId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kEnhancedAutofillSwitchViewId,
                                          /*is_toggled_on=*/NO,
                                          /*is_enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_SETTINGS_AUTOFILL_SETTINGS))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];
}

// Tests that viewing and editing sensitive Autofill AI data (e.g. Redress
// Number under Travel) triggers device reauthentication before unlocking
// sensitive details in the edit view.
- (void)testViewSensitiveDataReauthRequired {
  NSString* ownerName = @"Sensitive Travel User";
  NSString* redressNumber = @"123456789";
  _testEntityUUID =
      [AutofillAppInterface saveRedressNumberEntityWithName:ownerName
                                                     number:redressNumber];

  // Set up mock device reauthentication to require auth and return success.
  [ReauthenticationAppInterface mockReauthenticationModuleCanAttempt:YES];
  [ReauthenticationAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  [ReauthenticationAppInterface mockReauthenticationModuleShouldSkipReAuth:NO];

  [self openAutofillAndPasswordsSettings];

  // Tap Travel row.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSettingsTravelInfoCellId)]
      performAction:grey_tap()];

  // Wait for saved entity item to appear in Travel view.
  id<GREYMatcher> entityCellMatcher = grey_allOf(
      grey_accessibilityLabel(ownerName), grey_sufficientlyVisible(), nil);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:entityCellMatcher];

  // Tap the saved sensitive entity item.
  [[EarlGrey selectElementWithMatcher:entityCellMatcher]
      performAction:grey_tap()];

  // Wait for entity edit view transition to complete.
  id<GREYMatcher> editTableViewMatcher =
      grey_accessibilityID(kAutofillAIEntityEditTableViewId);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:editTableViewMatcher];

  // Wait for the Edit toolbar button to appear and become interactable.
  id<GREYMatcher> editButtonMatcher =
      chrome_test_util::SettingsToolbarEditButton();
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:editButtonMatcher];
  [[EarlGrey selectElementWithMatcher:editButtonMatcher]
      performAction:grey_tap()];

  [ReauthenticationAppInterface mockReauthenticationModuleReturnMockedResult];

  // Verify that reauthentication succeeded and the Done button is shown.
  id<GREYMatcher> doneButtonMatcher =
      grey_accessibilityID(kSettingsToolbarEditDoneButtonId);
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:doneButtonMatcher];
  [[EarlGrey selectElementWithMatcher:doneButtonMatcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap Done button to finish editing.
  [[EarlGrey selectElementWithMatcher:doneButtonMatcher]
      performAction:grey_tap()];

  // Pop back to Travel screen.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];

  // Wait for Travel screen to finish rendering before tapping back again.
  id<GREYMatcher> travelTitleMatcher =
      grey_text(l10n_util::GetNSString(IDS_AUTOFILL_TRAVEL_TITLE));
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:travelTitleMatcher];

  // Pop back to Autofill and Passwords screen.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      performAction:grey_tap()];
}

@end
