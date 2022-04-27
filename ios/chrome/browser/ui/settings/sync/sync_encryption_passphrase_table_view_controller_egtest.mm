// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarCancelButton;
using chrome_test_util::MatchInWindowWithNumber;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsLink;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::AdvancedSyncSettingsDoneButtonMatcher;
using chrome_test_util::PrimarySignInButton;

namespace {
NSString* const kPassphrase = @"hello";
}

@interface SyncEncryptionPassphraseTestCase : ChromeTestCase
@end

@implementation SyncEncryptionPassphraseTestCase

// Tests to open the sync passphrase view, and to close it.
- (void)testShowSyncPassphraseAndDismiss {
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];
  // Signin.
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];
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
// TODO(crbug.com/1320270): Test fails.
- (void)DISABLED_testShowSyncPassphraseInNewWindowAndDismiss {
  if (![ChromeEarlGrey areMultipleWindowsSupported])
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");

  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];
  // Signin.
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];
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
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];
  // Signin.
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];
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
