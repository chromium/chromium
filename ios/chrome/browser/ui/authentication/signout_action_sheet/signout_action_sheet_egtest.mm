// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

void VerifySignoutDialogShownForManagedAccount() {
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_SIGNOUT_CLEARS_DATA_DIALOG_TITLE_WITH_MANAGED_ACCOUNT);
  NSString* message = l10n_util::GetNSString(
      IDS_IOS_SIGNOUT_CLEARS_DATA_DIALOG_MESSAGE_WITH_MANAGED_ACCOUNT);

  [[EarlGrey selectElementWithMatcher:grey_text(title)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_text(message)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

void ClickSignOutInAccountSettings() {
  [ChromeEarlGreyUI openSettingsMenu];

  // Open the "Account Settings" view.
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsAccountButton()];

  // We're now in the "manage sync" view, and the signout button is at the very
  // bottom. Scroll there.
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:scrollViewMatcher]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Sign out" button.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM))]
      performAction:grey_tap()];
}

}  // namespace

@interface SignoutActionSheetTestCase : ChromeTestCase

@end

@implementation SignoutActionSheetTestCase

#pragma mark - Tests

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  // Enable the feature that shows the clear data on signout dialog for managed
  // accounts.
  config.features_enabled.push_back(kClearDeviceDataOnSignOutForManagedUsers);
  if ([self isRunningTest:@selector(testSignoutFromAccountsTableView)] ||
      [self isRunningTest:@selector(testCancelSignoutForManagedIdentity)] ||
      [self
          isRunningTest:@selector(testSignoutConfirmationForManagedIdentity)]) {
    // Once kIdentityDiscAccountMenu is launched, the sign out button in
    // AccountsTableView will be removed. It will be safe to remove this test at
    // that point. Also, testPopUpAccountsListViewOnSignOut covers the part of
    // correctly dismissing the view when the primary account is removed.
    config.features_disabled.push_back(kIdentityDiscAccountMenu);
  }
  return config;
}

- (AppLaunchConfiguration)managedAppConfigurationForTestCase {
  AppLaunchConfiguration config = self.appConfigurationForTestCase;
  config.additional_args.push_back(base::StrCat(
      {"-", base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey)}));
  config.additional_args.push_back(std::string("<dict></dict>"));
  return config;
}

// Tests the sign-out flow from the accounts table view. This test
// also makes sure the settings are not blocked after the sign-out.
// Related to crbug.com/1471942.
- (void)testSignoutFromAccountsTableView {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];

  // Open the "manage sync" view.
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsAccountButton()];

  // Scroll to the bottom to view the "manage accounts" button.
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:scrollViewMatcher]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "manage accounts on This Device" button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_ACCOUNTS_ITEM))]
      performAction:grey_tap()];

  // The "manage accounts" view should be shown, tap on "Sign Out" button on
  // that page.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSettingsAccountsTableViewSignoutCellId)]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedOut];

  // Verify the "manage accounts" view is popped.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsLegacyAccountsTableViewId)]
      assertWithMatcher:grey_notVisible()];

  // Verify the "manage sync" view is popped.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Verify the account settings row is not showing in the settings menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsAccountButton()]
      assertWithMatcher:grey_notVisible()];
  // Open Google services settings to prove settings are not blocked.
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::GoogleServicesSettingsButton()];
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kGoogleServicesSettingsViewIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Tests the signout flow for managed users that require clearing data on
// signout. A dialog should be displayed, and clicking on the `Sign Out` button
// should sign the user out.
- (void)testSignoutConfirmationForManagedIdentity {
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];
  // Sign in with managed account.
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeManagedIdentity];

  ClickSignOutInAccountSettings();
  VerifySignoutDialogShownForManagedAccount();

  // Click on signout and verify that data is cleared.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::AlertAction(l10n_util::GetNSString(
                         IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON)),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];

  // Verify histogram metric for confirming signout is recorded.
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:1
          forHistogram:@"Signin.SignoutAndClearDataFromManagedAccount"],
      @"Signin.SignoutAndClearDataFromManagedAccount metric was not recorded "
      @"when managed clicked cancel in the data clearing dialog shown on "
      @"signout.");

  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
}

// Tests the signout flow for managed users that require clearing data on
// signout. A dialog should be displayed, and clicking on the `Cancel` button
// should keep the user signed in.
- (void)testCancelSignoutForManagedIdentity {
  GREYAssertNil([MetricsAppInterface setupHistogramTester],
                @"Cannot setup histogram tester.");
  [MetricsAppInterface overrideMetricsAndCrashReportingForTesting];

  // Sign in with managed account.
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeManagedIdentity];

  ClickSignOutInAccountSettings();
  VerifySignoutDialogShownForManagedAccount();

  // Tap cancel and verify user is still signed in.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeManagedIdentity];

  // Verify histogram metric for cancelling signout is recorded.
  GREYAssertNil(
      [MetricsAppInterface
           expectCount:1
             forBucket:0
          forHistogram:@"Signin.SignoutAndClearDataFromManagedAccount"],
      @"Signin.SignoutAndClearDataFromManagedAccount metric was not recorded "
      @"when managed clicked cancel in the data clearing dialog shown on "
      @"signout.");

  [MetricsAppInterface stopOverridingMetricsAndCrashReportingForTesting];
  GREYAssertNil([MetricsAppInterface releaseHistogramTester],
                @"Cannot reset histogram tester.");
}

// Tests the signout flow for managed users in a managed browser does not show
// the dialog for clearing data on sign-out.
- (void)testNoSignoutConfirmationForManagedIdentityInManagedBrowser {
  // Relaunch the app with managed config to take the configuration into
  // account.
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithConfiguration:
          [self managedAppConfigurationForTestCase]];
  // Sign in with managed account.
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeManagedIdentity];

  // The sign out button should directly sign out the user.
  ClickSignOutInAccountSettings();
  [SigninEarlGrey verifySignedOut];
}

@end
