// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/metrics/model/metrics_app_interface.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message_test_utils.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/test_switches.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using signin::assertSnackbarNotShown;
using signin::assertSnackbarShownAndDismissItWithIdentity;

namespace {

// The primary identity.
FakeSystemIdentity* const kPrimaryIdentity = [FakeSystemIdentity fakeIdentity1];

FakeSystemIdentity* const kSecondaryIdentity =
    [FakeSystemIdentity fakeIdentity2];

}  // namespace

// Integration tests using the Account Menu.
@interface IdentitySnackbarMessageTestCase : WebHttpServerChromeTestCase
@end

@implementation IdentitySnackbarMessageTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  // This flag also controls the identity snackbar message.
  config.features_enabled.push_back(kIdentityDiscAccountMenu);
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.additional_args.push_back(
      "--enable-features=" + std::string(kStartSurface.name) + "<" +
      std::string(kStartSurface.name));
  config.additional_args.push_back(
      "--force-fieldtrials=" + std::string(kStartSurface.name) + "/Test");
  config.additional_args.push_back(
      "--force-fieldtrial-params=" + std::string(kStartSurface.name) +
      ".Test:" + std::string(kReturnToStartSurfaceInactiveDurationInSeconds) +
      "/" + "0");
  config.features_enabled.push_back(kIdentityConfirmationSnackbar);

  return config;
}

- (void)tearDownHelper {
  [ChromeEarlGrey signOutAndClearIdentities];
  [super tearDownHelper];
}

// Prepaes the NTP start surface.
- (void)prepareStartSurface {
  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL destinationUrl = self.testServer->GetURL("/pony.html");
  [ChromeEarlGrey loadURL:destinationUrl];
  [ChromeEarlGrey
      waitForWebStateContainingText:"Anyone know any good pony jokes?"];
}

// Update the identity snackbar params based on its last count, to allow
// displaying it.
- (void)prepareSnackbarParamsForNextDisplayWithLastCount:(int)lastCount {
  [ChromeEarlGrey
        setIntegerValue:lastCount
      forLocalStatePref:prefs::kIdentityConfirmationSnackbarDisplayCount];

  if (lastCount == 0) {
    // Set params for reminder after 1 day.
    [ChromeEarlGrey
             setTimeValue:base::Time::Now() - base::Days(1)
        forLocalStatePref:prefs::kIdentityConfirmationSnackbarLastPromptTime];
  } else if (lastCount == 1) {
    // Set params for reminder after 7 days.
    [ChromeEarlGrey
             setTimeValue:base::Time::Now() - base::Days(7)
        forLocalStatePref:prefs::kIdentityConfirmationSnackbarLastPromptTime];
  } else if (lastCount == 2) {
    // Set params for reminder after 30 days.
    [ChromeEarlGrey
             setTimeValue:base::Time::Now() - base::Days(30)
        forLocalStatePref:prefs::kIdentityConfirmationSnackbarLastPromptTime];
  }
}

#pragma mark - Test snackbar

// Verifies identity confirmation snackbar shows on startup with multiple
// identities on device after 1 day.
- (void)testMultipleIdentities_IdentityConfirmationToast {
  // TODO(crbug.com/433726717): Test disabled on iPhones.
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iPhones.");
  }

  [self prepareStartSurface];
  // Add multiple identities and sign in with one of them.
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  [SigninEarlGrey addFakeIdentity:kSecondaryIdentity];
  [self prepareSnackbarParamsForNextDisplayWithLastCount:0];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  // Confirm the snackbar shows after 1 day of signing in with multi identities
  // on device.
  assertSnackbarShownAndDismissItWithIdentity(kPrimaryIdentity);
}

// Verifies no identity confirmation snackbar shows on startup with only one
// identity on device.
- (void)testSingleIdentity_IdentityConfirmationToast {
  [self prepareStartSurface];
  // Add multiple identities and sign in with one of them.
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  [self prepareSnackbarParamsForNextDisplayWithLastCount:0];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  assertSnackbarNotShown(kPrimaryIdentity);
}

// Verifies no identity confirmation snackbar shows on startup when there is an
// identity on the device but the user is signed-out.
- (void)testNoIdentity_IdentityConfirmationToast {
  [self prepareStartSurface];
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];

  // Keep the identity on device but sign-out.
  [SigninEarlGrey signOut];
  [self prepareSnackbarParamsForNextDisplayWithLastCount:0];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  assertSnackbarNotShown(kPrimaryIdentity);
}

// Verifies identity confirmation snackbar on startup does not show after a
// recent sign-in.
- (void)testRecentSignin_IdentityConfirmationToast {
  [self prepareStartSurface];
  // Add multiple identities and sign in with one of them.
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  [SigninEarlGrey addFakeIdentity:kSecondaryIdentity];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  assertSnackbarNotShown(kPrimaryIdentity);
}

// Verifies identity confirmation snackbar shows on startup with multiple
// identities on device with frequency limitations.
- (void)testFrequencyLimitation_IdentityConfirmationToast {
  // TODO(crbug.com/433726717): Test disabled on iPhones.
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iPhones.");
  }

  [self prepareStartSurface];
  // Add multiple identities and sign in with one of them.
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  [SigninEarlGrey addFakeIdentity:kSecondaryIdentity];

  // Snackbar shows after 1 day of signing in.
  [self prepareSnackbarParamsForNextDisplayWithLastCount:0];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  assertSnackbarShownAndDismissItWithIdentity(kPrimaryIdentity);

  // Background then foreground the app again.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  assertSnackbarNotShown(kPrimaryIdentity);

  // Update params to be ready for a second display after 7 days.
  [self prepareSnackbarParamsForNextDisplayWithLastCount:1];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  assertSnackbarShownAndDismissItWithIdentity(kPrimaryIdentity);

  // Background then foreground the app again.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  assertSnackbarNotShown(kPrimaryIdentity);

  // Update params to be ready for a third display after 30 days.
  [self prepareSnackbarParamsForNextDisplayWithLastCount:2];
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  assertSnackbarShownAndDismissItWithIdentity(kPrimaryIdentity);

  // Update params after third display.
  [self prepareSnackbarParamsForNextDisplayWithLastCount:3];

  // Background then foreground the app again, the snackbar does not show after
  // third display.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];
  assertSnackbarNotShown(kPrimaryIdentity);
}

// Verifies identity confirmation snackbar does not show if NTP start surface
// not shown.
- (void)testNoStartSurfaceNoSnackbar_IdentityConfirmationToast {
  // Skip prepareStartSurface.

  // Add multiple identities and sign in with one of them.
  [SigninEarlGrey signinWithFakeIdentity:kPrimaryIdentity];
  [SigninEarlGrey addFakeIdentity:kSecondaryIdentity];
  [self prepareSnackbarParamsForNextDisplayWithLastCount:0];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  assertSnackbarNotShown(kPrimaryIdentity);
}

// Verifies identity confirmation snackbar does not show in incognito.
- (void)testNoSnackbarShownIncognito_IdentityConfirmationToast {
  [ChromeEarlGrey openNewIncognitoTab];

  // Background then foreground the app.
  [[AppLaunchManager sharedManager] backgroundAndForegroundApp];

  assertSnackbarNotShown(kPrimaryIdentity);
}

@end
