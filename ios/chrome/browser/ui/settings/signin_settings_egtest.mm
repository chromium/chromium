// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/policy/policy_constants.h"
#import "components/sync/base/features.h"
#import "components/sync/base/user_selectable_type.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/browser/ui/authentication/history_sync/pref_names.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/settings/elements/elements_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::GoogleSyncSettingsButton;
using chrome_test_util::SettingsSignInRowMatcher;

@interface SigninSettingsTestCase : ChromeTestCase
@end

@implementation SigninSettingsTestCase

- (void)tearDown {
  [PolicyAppInterface clearPolicies];

  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  if ([self isRunningTest:@selector
            (testSigninRowOpensSyncDialogIfSignedOutAndNoDeviceAccounts)] ||
      [self isRunningTest:@selector
            (testSigninRowOpensSyncDialogIfSignedOutAndSomeDeviceAccounts)] ||
      [self isRunningTest:@selector(testSigninRowDisabledBySyncPolicy)] ||
      [self isRunningTest:@selector(testSyncOffRowOpensDialogIfSignedIn)] ||
      [self isRunningTest:@selector(testSyncOffRowDisabledByPolicy)]) {
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }
  if ([self isRunningTest:@selector
            (testSigninRowOpensSheetIfSignedOutAndSomeDeviceAccounts)] ||
      [self isRunningTest:@selector
            (testSigninRowOpensAuthActivityIfSignedOutAndNoDeviceAccounts)] ||
      [self isRunningTest:@selector(testSigninRowNotDisabledBySyncPolicy)] ||
      [self isRunningTest:@selector(testSignInAndDeclineHistorySync)] ||
      [self isRunningTest:@selector
            (testHistorySyncSkippedIfDeclinedJustBefore)] ||
      [self isRunningTest:@selector(testHistorySyncSkippedIfDeclinedTwice)] ||
      [self isRunningTest:@selector(testInterruptWhenHistoryOptInShown)] ||
      [self isRunningTest:@selector
            (testSigninSecondTimeAfterAddingAccountAgain)] ||
      [self isRunningTest:@selector(testSigninWithNoAccountOnDevice)] ||
      [self isRunningTest:@selector(testSigninWithHistorySync)] ||
      [self isRunningTest:@selector
            (testSigninSecondTimeShouldNotShowHistorySyncOptin)]) {
    config.features_enabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }
  return config;
}

- (void)testPromoCardHidden {
  [ChromeEarlGreyUI openSettingsMenu];

  [SigninEarlGreyUI verifySigninPromoNotVisible];
}

// Tests the history sync opt-in after sign-in in settings.
- (void)testSigninWithHistorySync {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  // Tap on sign-in cell.
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];
  // Confirm sign in.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kWebSigninPrimaryButtonAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the footer is shown without the user's email.
  id<GREYMatcher> footerTextMatcher = grey_allOf(
      grey_text(
          l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_FOOTER_WITHOUT_EMAIL)),
      grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:footerTextMatcher]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      assertWithMatcher:grey_notNil()];
  // Accept History Sync.
  [[[EarlGrey selectElementWithMatcher:
                  chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
  // Verify that the history sync is enabled.
  GREYAssertTrue(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be enabled.");
  GREYAssertTrue([SigninEarlGreyAppInterface
                     isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                 @"Tabs sync should be enabled.");
}

// Tests that history sync opt-in is not presented after sign-in a second time.
- (void)testSigninSecondTimeShouldNotShowHistorySyncOptin {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  // Tap on sign-in cell.
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];
  // Confirm sign in.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kWebSigninPrimaryButtonAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Accept History Sync.
  [[[EarlGrey selectElementWithMatcher:
                  chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceNotSyncing];
  [ChromeEarlGreyUI openSettingsMenu];
  // Tap on sign-in cell.
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];
  // Confirm sign in.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kWebSigninPrimaryButtonAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // History sync opt-in not visibled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      assertWithMatcher:grey_nil()];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
  // Verify that the history sync is enabled.
  GREYAssertTrue(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be enabled.");
  GREYAssertTrue([SigninEarlGreyAppInterface
                     isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                 @"Tabs sync should be enabled.");
}

// Tests that history sync opt-in is presented for the following scenario:
// + Add account
// + Sign-in and accept history sync
// + Sign-out
// + Remove account
// + Add same account again
// + Sign-in
// => Expect history sync opt-in dialog.
- (void)testSigninSecondTimeAfterAddingAccountAgain {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  // Tap on sign-in cell.
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];
  // Confirm sign in.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kWebSigninPrimaryButtonAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Accept History Sync.
  [[[EarlGrey selectElementWithMatcher:
                  chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceNotSyncing];
  [ChromeEarlGreyUI openSettingsMenu];
  // Tap on sign-in cell.
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];
  // Confirm sign in.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kWebSigninPrimaryButtonAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // History sync opt-in not visibled.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      assertWithMatcher:grey_nil()];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
  // Verify that the history sync is enabled.
  GREYAssertTrue(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be enabled.");
  GREYAssertTrue([SigninEarlGreyAppInterface
                     isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                 @"Tabs sync should be enabled.");
}

// Tests sign-in and accept history sync opt-in from the settings when having
// no account on the device.
- (void)testSigninWithNoAccountOnDevice {
  [ChromeEarlGreyUI openSettingsMenu];
  // Tap on sign-in cell.
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];
  // Set up a fake identity to add and sign-in with.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentityForSSOAuthAddAccountFlow:fakeIdentity];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(
                                       kFakeAuthAddAccountButtonIdentifier),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Make sure the fake SSO view controller is fully removed.
  [ChromeEarlGreyUI waitForAppToIdle];
  // Accept History Sync.
  [[[EarlGrey selectElementWithMatcher:
                  chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
  // Close settings.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
  // Verify that the history sync is enabled.
  GREYAssertTrue(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be enabled.");
  GREYAssertTrue([SigninEarlGreyAppInterface
                     isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                 @"Tabs sync should be enabled.");
}

// For a signed out user without device accounts, tests that the sign-in row is
// shown with the correct strings and opens the dialog to enable sync upon tap.
// kReplaceSyncPromosWithSignInPromos is disabled. This test can be removed
// once the kReplaceSyncPromosWithSignInPromos is removed.
// This test is replace with `testSigninWithNoAccountOnDevice`.
- (void)testSigninRowOpensSyncDialogIfSignedOutAndNoDeviceAccounts {
  [ChromeEarlGreyUI openSettingsMenu];

  [[EarlGrey selectElementWithMatcher:GoogleSyncSettingsButton()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SUBTITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// For a signed out user with device accounts, tests that the sign-in row is
// shown with the correct strings and opens the dialog to enable sync upon tap.
- (void)testSigninRowOpensSyncDialogIfSignedOutAndSomeDeviceAccounts {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];

  [[EarlGrey selectElementWithMatcher:GoogleSyncSettingsButton()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SUBTITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// For a signed out user with device accounts, tests that the sign-in row is
// shown with the correct strings and opens the sign-in sheet upon tap.
- (void)testSigninRowOpensSheetIfSignedOutAndSomeDeviceAccounts {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_IDENTITY_DISC_SIGN_IN_PROMO_LABEL))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kWebSigninPrimaryButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// For a signed out user with no device accounts, tests that the sign-in row is
// shown with the correct strings and opens the auth activity on tap.
- (void)testSigninRowOpensAuthActivityIfSignedOutAndNoDeviceAccounts {
  [ChromeEarlGreyUI openSettingsMenu];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_IDENTITY_DISC_SIGN_IN_PROMO_LABEL))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthActivityViewIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// For a signed out user with the SyncDisabled policy, tests that the sign-in
// row is disabled and opens the "disabled by policy" bubble upon tap.
- (void)testSigninRowDisabledBySyncPolicy {
  [ChromeEarlGreyUI openSettingsMenu];
  // Set policy after opening settings so there's no need to dismiss the policy
  // bottom sheet.
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsSignInDisabledCellId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsSignInDisabledCellId)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SETTINGS_SIGNIN_DISABLED_POPOVER_TEXT))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// For a signed out user with the SyncDisabled policy, tests that the sign-in
// row is still enabled, since it leads to sign-in only.
- (void)testSigninRowNotDisabledBySyncPolicy {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  // Set policy after opening settings so there's no need to dismiss the policy
  // bottom sheet.
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_IDENTITY_DISC_SIGN_IN_PROMO_LABEL))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kWebSigninPrimaryButtonAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// For a signed out user with the BrowserSignin policy, tests that the sign-in
// row is disabled and opens the "disabled by policy" bubble upon tap.
- (void)testSigninRowDisabledBySigninPolicy {
  policy_test_utils::SetPolicy(static_cast<int>(BrowserSigninMode::kDisabled),
                               policy::key::kBrowserSignin);
  [ChromeEarlGreyUI openSettingsMenu];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsSignInDisabledCellId)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsSignInDisabledCellId)]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SETTINGS_SIGNIN_DISABLED_POPOVER_TEXT))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// For a signed in user, tests that the "Sync off" row is shown and opens the
// dialog to enable sync upon tap.
- (void)testSyncOffRowOpensDialogIfSignedIn {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  [ChromeEarlGreyUI openSettingsMenu];

  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:GoogleSyncSettingsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:GoogleSyncSettingsButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// For a signed in user with the SyncDisabled policy, tests that the "Sync off"
// row is disabled and opens the "disabled by policy" bubble upon tap.
- (void)testSyncOffRowDisabledByPolicy {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  // Set policy after opening settings so there's no need to dismiss the policy
  // bottom sheet.
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);

  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:GoogleSyncSettingsButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:GoogleSyncSettingsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEnterpriseInfoBubbleViewId)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_SYNC_SETTINGS_DISABLED_POPOVER_TEXT))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that when signing-in using the settings cell, the user is not signed
// out if history sync is declined.
- (void)testSignInAndDeclineHistorySync {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];

  // Tap on sign-in cell.
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];

  // Confirm sign in.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kWebSigninPrimaryButtonAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that the footer is shown without the user's email.
  id<GREYMatcher> footerTextMatcher = grey_allOf(
      grey_text(
          l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_FOOTER_WITHOUT_EMAIL)),
      grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:footerTextMatcher]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      assertWithMatcher:grey_notNil()];

  // Decline History Sync.
  [[[EarlGrey selectElementWithMatcher:
                  chrome_test_util::SigninScreenPromoSecondaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      chrome_test_util::HistoryOptInPromoMatcher()];

  // Verify that the history sync is disabled.
  GREYAssertFalse(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be disabled.");
  GREYAssertFalse([SigninEarlGreyAppInterface
                      isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                  @"Tabs sync should be disabled.");
  // Verify that the identity is still signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that if a user signs in and declines History Sync, then sign-out, then
// History Sync screen is skipped if they sign-in again from the settings.
- (void)testHistorySyncSkippedIfDeclinedJustBefore {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [ChromeEarlGreyUI openSettingsMenu];

  // Sign-in.
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kWebSigninPrimaryButtonAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Decline History Sync.
  [[[EarlGrey selectElementWithMatcher:
                  chrome_test_util::SigninScreenPromoSecondaryButtonMatcher()]
         usingSearchAction:chrome_test_util::HistoryOptInScrollDown()
      onElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForUIElementToDisappearWithMatcher:
                      chrome_test_util::HistoryOptInPromoMatcher()];

  // Sign-out then forget fakeIdentity1.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceKeepData];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity1];

  // Tap on sign-in cell in settings for fakeIdentity2.
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity2]];
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];

  // Confirm sign-in.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kWebSigninPrimaryButtonAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Verify that the History Sync Opt-In screen is not shown.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that if History Sync has been declined twice in a row previously, the
// opt-in screen will be skipped if a user signs-in in settings.
- (void)testHistorySyncSkippedIfDeclinedTwice {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Set the count of declined History Sync to 2.
  [ChromeEarlGrey
      setIntegerValue:2
          forUserPref:history_sync_prefs::kHistorySyncSuccessiveDeclineCount];

  // Tap on the sign-in cell in settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];

  // Confirm sign-in.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kWebSigninPrimaryButtonAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  // Verify that the History Sync Opt-In screen is skipped.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Verify that the History Sync is disabled.
  GREYAssertFalse(
      [SigninEarlGreyAppInterface
          isSelectedTypeEnabled:syncer::UserSelectableType::kHistory],
      @"History sync should be disabled.");
  GREYAssertFalse([SigninEarlGreyAppInterface
                      isSelectedTypeEnabled:syncer::UserSelectableType::kTabs],
                  @"Tabs sync should be disabled.");
  // Verify that the identity is still signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that if an external app opens an URL when
// SigninAndHistorySyncCoordinator is shown, the coordinator is interrupted
// correctly without triggering DCHECK.
// See https://crbug.com/1485570.
- (void)testInterruptWhenHistoryOptInShown {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Tap on the sign-in cell in settings.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      performAction:grey_tap()];

  // Confirm sign-in.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityID(
                                kWebSigninPrimaryButtonAccessibilityIdentifier),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Verify that the History Sync Opt-In screen is shown.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::HistoryOptInPromoMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open the URL as if it was opened from another app.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];
}

@end
