// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import "base/ios/ios_util.h"
#import "components/feature_engagement/public/feature_list.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/metrics/metrics_app_interface.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/signin_settings_app_interface.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::MatchInWindowWithNumber;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsLink;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SettingsSignInRowMatcher;

namespace {
NSString* const kPassphrase = @"hello";
}

@interface SyncEncryptionPassphraseTestCase : ChromeTestCase
@end

@implementation SyncEncryptionPassphraseTestCase

- (void)tearDown {
  [SigninEarlGrey signOut];
  [ChromeEarlGrey
      waitForSyncEngineInitialized:NO
                       syncTimeout:syncher::kSyncUKMOperationsTimeout];
  [ChromeEarlGrey clearSyncServerData];

  [super tearDown];
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey
      waitForSyncEngineInitialized:NO
                       syncTimeout:syncher::kSyncUKMOperationsTimeout];
  // TODO(crbug.com/1450472): Remove when kHideSettingsSyncPromo is launched.
  [SigninSettingsAppInterface setSettingsSigninPromoDisplayedCount:INT_MAX];
}

// Tests to open the sync passphrase view, and to close it.
- (void)testShowSyncPassphraseAndDismiss {
  // TODO(crbug.com/1475088): Remove the disabling after fixing the root cause.
  if (![ChromeEarlGrey isCompactWidth]) {
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithFeaturesEnabled:{}
                                    disabled:
                                        {feature_engagement::
                                             kIPHiOSTabGridToolbarItemFeature}
                              relaunchPolicy:ForceRelaunchByCleanShutdown];
  }

  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];
  // Signin.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGrey openNewTab];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_SYNC_ENTER_PASSPHRASE_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:NavigationBarCancelButton()]
      performAction:grey_tap()];
  // Wait until the settings is fully removed.
  [ChromeEarlGreyUI waitForAppToIdle];
}

// Tests opening the sync passphrase view, then a new window and check that
// enter passphrase message appears.
- (void)testShowSyncPassphraseInNewWindowAndDismiss {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];
  // Signin.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  [[EarlGrey
      selectElementWithMatcher:MatchInWindowWithNumber(
                                   1,
                                   ButtonWithAccessibilityLabelId(
                                       IDS_IOS_SYNC_ENTER_PASSPHRASE_BUTTON))]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:MatchInWindowWithNumber(
                                          1, NavigationBarCancelButton())]
      performAction:grey_tap()];
  // Wait until the settings is fully removed.
  [ChromeEarlGreyUI waitForAppToIdle];

  [ChromeEarlGrey closeWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:1];

  [SigninEarlGrey signOut];
  [SigninEarlGrey verifySignedOut];
}

// Tests Sync is on after opening settings from the Infobar and entering the
// passphrase.
- (void)testShowAddSyncPassphrphrase {
  // TODO(crbug.com/1475088): Remove the disabling after fixing the root cause.
  if (![ChromeEarlGrey isCompactWidth]) {
    [[AppLaunchManager sharedManager]
        ensureAppLaunchedWithFeaturesEnabled:{}
                                    disabled:
                                        {feature_engagement::
                                             kIPHiOSTabGridToolbarItemFeature}
                              relaunchPolicy:ForceRelaunchByCleanShutdown];
  }

  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];
  // Signin.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGrey openNewTab];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                          IDS_IOS_SYNC_ENTER_PASSPHRASE_BUTTON)]
      performAction:grey_tap()];

  // Type and submit the sync passphrase.
  [SigninEarlGreyUI submitSyncPassphrase:kPassphrase];
  [ChromeEarlGreyUI openSettingsMenu];
  // Check Sync On label is visible and user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
  [SigninEarlGrey verifySyncUIEnabled:YES];
}
@end
