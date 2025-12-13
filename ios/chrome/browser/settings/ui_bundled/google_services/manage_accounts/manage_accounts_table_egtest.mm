// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/authentication/test/signin_matchers.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/browser/signin/model/test_constants_utils.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsSignInRowMatcher;

// Integration tests using the Account Settings screen.
@interface ManageAccountsTableTestCase : WebHttpServerChromeTestCase
@end

@implementation ManageAccountsTableTestCase

- (void)tearDownHelper {
  [ChromeEarlGrey clearFakeSyncServerData];
  [super tearDownHelper];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  if ([self isRunningTest:@selector
            (testReloadOnRemoveSecondaryAccountInOtherProfile)]) {
    config.features_enabled.push_back(kSeparateProfilesForManagedAccounts);
  }

  return config;
}

// Tests that the Accounts list screen is correctly popped if the signed in
// account is removed.
- (void)testPopUpAccountsListViewOnSignOut {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In identity, then open the Sync Settings.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI openAccountsListFromSettings];

  // Forget fakeIdentity, screens should be popped back to the Main Settings.
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SettingsSignInRowMatcher()];
  [SigninEarlGrey verifySignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Accounts list screen is correctly reloaded when one of
// the non-primary account is removed.
- (void)testReloadOnRemoveSecondaryAccount {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Sign In fakeIdentity, then open the Account Settings.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUI openAccountsListFromSettings];

  [SigninEarlGrey forgetFakeIdentity:fakeIdentity2];

  // Check that fakeIdentity2 isn't available anymore on the Accounts list.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              fakeIdentity2.userEmail),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];

  // Check fakeIdentity1 is still signed-in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSettingsAccountsTableViewDoneButtonId)]
      performAction:grey_tap()];
}

- (void)testReloadOnRemoveSecondaryAccountInOtherProfile {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:fakeManagedIdentity];

  // Sign In fakeIdentity1, then open the Account Settings.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUI openAccountsListFromSettings];

  // Ensure both identities show up.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              fakeIdentity1.userEmail),
                                          grey_text(fakeIdentity1.userEmail),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(
                                       fakeManagedIdentity.userEmail),
                                   grey_text(fakeManagedIdentity.userEmail),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  // Remove the identity that belongs to another profile.
  [SigninEarlGrey forgetFakeIdentity:fakeManagedIdentity];

  // Check that fakeIdentity1 is still there, but fakeManagedIdentity isn't
  // available anymore.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              fakeIdentity1.userEmail),
                                          grey_text(fakeIdentity1.userEmail),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(
                                       fakeManagedIdentity.userEmail),
                                   grey_text(fakeManagedIdentity.userEmail),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];

  // Check fakeIdentity1 is still signed-in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSettingsAccountsTableViewDoneButtonId)]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is popped and the user signed out
// when the primary account is removed.
- (void)testSignOutOnRemovePrimaryAccount {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Sign In `fakeIdentity1`, then open the Account Settings.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUI openAccountsListFromSettings];

  // Tap on Remove fakeIdentity1 button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              [kSettingsAccountsRemoveAccountButtonAccessibilityIdentifier
                  stringByAppendingString:fakeIdentity1.userEmail])]
      performAction:grey_tap()];

  // Tap on fakeIdentity1 confirm remove button.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                     IDS_IOS_REMOVE_ACCOUNT_LABEL)] performAction:grey_tap()];

  // Check that the user is signed out and the Main Settings screen is shown.
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGrey verifySignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests opening the remove identity confirmation dialog once, and cancel the
// dialog. Then we open the remove identity confirmation dialog to remove the
// identity. And finally the remove identity confirmation dialog is opened a
// third time to remove a second identity.
// The goal of this test is to confirm the dialog can be opened several times.
// TODO(crbug.com/460742009): Test is flaky.
- (void)FLAKY_testRemoveAccountSeveralTime {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  FakeSystemIdentity* fakeIdentity3 = [FakeSystemIdentity fakeIdentity3];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity3];

  // Sign In `fakeIdentity1`, then open the Account Settings.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUI openAccountsListFromSettings];

  // Open the remove identity confirmation dialog for the first time.
  // Tap on Remove fakeIdentity1 button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              [kSettingsAccountsRemoveAccountButtonAccessibilityIdentifier
                  stringByAppendingString:fakeIdentity1.userEmail])]
      performAction:grey_tap()];

  // Cancel it.
  if ([ChromeEarlGrey isIPadIdiom] || iOS26_OR_ABOVE()) {
    // There is no Cancel button on ipad and on newer iOS versions.
    // Tap on Remove fakeIdentity1 button to dismiss the alert.
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID(
                [kSettingsAccountsRemoveAccountButtonAccessibilityIdentifier
                    stringByAppendingString:fakeIdentity1.userEmail])]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
        performAction:grey_tap()];
  }

  // Open it a second time for `fakeIdentity2`, confirmal removal.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              [kSettingsAccountsRemoveAccountButtonAccessibilityIdentifier
                  stringByAppendingString:fakeIdentity2.userEmail])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                     IDS_IOS_REMOVE_ACCOUNT_LABEL)] performAction:grey_tap()];

  // Open it a third time for `fakeIdentity3`, confirmal removal.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              [kSettingsAccountsRemoveAccountButtonAccessibilityIdentifier
                  stringByAppendingString:fakeIdentity3.userEmail])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ActionSheetItemWithAccessibilityLabelId(
                     IDS_IOS_REMOVE_ACCOUNT_LABEL)] performAction:grey_tap()];
}

// Tests add account flow.
- (void)testAddAccount {
  chrome_test_util::GREYAssertErrorNil(
      [MetricsAppInterface setupHistogramTester]);
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In identity, then open the Sync Settings.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI openAccountsListFromSettings];

  for (NSString* cancelButtonId in
           signin::FakeSystemIdentityManagerStaySignedOutButtons()) {
    // Tap on "Add Account".
    [[EarlGrey
        selectElementWithMatcher:
            grey_accessibilityID(kSettingsAccountsTableViewAddAccountCellId)]
        performAction:grey_tap()];
    // Checks the Fake authentication view is shown
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kFakeAuthActivityViewIdentifier)]
        assertWithMatcher:grey_sufficientlyVisible()];
    // Close the SSO view controller.
    id<GREYMatcher> matcher = grey_allOf(grey_accessibilityID(cancelButtonId),
                                         grey_sufficientlyVisible(), nil);
    [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  }

  [ChromeEarlGreyUI waitForAppToIdle];
}

@end
