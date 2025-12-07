// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
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

- (AppLaunchConfiguration)managedAppConfigurationForTestCase {
  AppLaunchConfiguration config = self.appConfigurationForTestCase;
  config.additional_args.push_back(base::StrCat(
      {"-", base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey)}));
  config.additional_args.push_back(std::string("<dict></dict>"));
  return config;
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
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:fakeManagedIdentity];

  // The sign out button should directly sign out the user.
  ClickSignOutInAccountSettings();
  [SigninEarlGrey verifySignedOut];
}

@end
