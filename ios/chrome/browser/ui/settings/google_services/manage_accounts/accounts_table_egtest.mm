// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
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
@interface AccountsTableTestCase : WebHttpServerChromeTestCase
@end

@implementation AccountsTableTestCase

- (void)tearDown {
  [ChromeEarlGrey clearFakeSyncServerData];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  config.features_enabled.push_back(kIdentityDiscAccountMenu);

  return config;
}

- (void)openAccountsListFromSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Tap "Manage accounts on this device" to get to the accounts view.
  // First scroll down so that the button is visible.
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:scrollViewMatcher]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Now tab the "Manage accounts on this device" button.
  id<GREYMatcher> manageAccountsButtonMatcher =
      grey_allOf(grey_text(l10n_util::GetNSString(
                     IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_ACCOUNTS_ITEM)),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:manageAccountsButtonMatcher]
      performAction:grey_tap()];
}

// Tests that the Accounts list screen is correctly popped if the signed in
// account is removed.
- (void)testPopUpAccountsListViewOnSignOut {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In identity, then open the Sync Settings.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [self openAccountsListFromSettings];

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
  [self openAccountsListFromSettings];

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

// Tests that the Account Settings screen is popped and the user signed out
// when the primary account is removed.
- (void)testSignOutOnRemovePrimaryAccount {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Sign In `fakeIdentity1`, then open the Account Settings.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];
  [self openAccountsListFromSettings];

  // Tap on Remove fakeIdentity1 button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              [kSettingsAccountsRemoveAccountButtonAccessibilityIdentifier
                  stringByAppendingString:fakeIdentity1.userEmail])]
      performAction:grey_tap()];

  // Tap on fakeIdentity1 confirm remove button.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_REMOVE_ACCOUNT_LABEL)]
      performAction:grey_tap()];

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
- (void)testRemoveAccountSeveralTime {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  FakeSystemIdentity* fakeIdentity3 = [FakeSystemIdentity fakeIdentity3];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity3];

  // Sign In `fakeIdentity1`, then open the Account Settings.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];
  [self openAccountsListFromSettings];

  // Open the remove identity confirmation dialog for the first time.
  // Tap on Remove fakeIdentity1 button.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              [kSettingsAccountsRemoveAccountButtonAccessibilityIdentifier
                  stringByAppendingString:fakeIdentity1.userEmail])]
      performAction:grey_tap()];

  // Cancel it.
  if ([ChromeEarlGrey isIPadIdiom]) {
    // There is no Cancel button on ipad.
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
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_REMOVE_ACCOUNT_LABEL)]
      performAction:grey_tap()];

  // Open it a third time for `fakeIdentity3`, confirmal removal.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              [kSettingsAccountsRemoveAccountButtonAccessibilityIdentifier
                  stringByAppendingString:fakeIdentity3.userEmail])]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_REMOVE_ACCOUNT_LABEL)]
      performAction:grey_tap()];
}

// Tests add account flow.
- (void)testAddAccount {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In identity, then open the Sync Settings.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [self openAccountsListFromSettings];

  // Tap on "Add Account".
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSettingsAccountsTableViewAddAccountCellId)]
      performAction:grey_tap()];

  // Checks the Fake authentication view is shown
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthActivityViewIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close the SSO view controller.
  id<GREYMatcher> matcher =
      grey_allOf(grey_accessibilityID(kFakeAuthCancelButtonIdentifier),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
}

@end
