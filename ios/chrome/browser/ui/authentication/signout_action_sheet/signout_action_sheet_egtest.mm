// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/sync/base/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface SignoutActionSheetTestCase : ChromeTestCase

@end

@implementation SignoutActionSheetTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
  return config;
}

#pragma mark - Tests

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
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE))]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedOut];

  // Verify the "manage accounts" view is popped.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsAccountsTableViewId)]
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

@end
