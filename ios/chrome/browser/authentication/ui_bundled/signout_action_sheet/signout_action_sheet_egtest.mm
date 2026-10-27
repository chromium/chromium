// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/app_launch_manager.h"

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
  [SigninEarlGreyUI signOut];
}

@end
