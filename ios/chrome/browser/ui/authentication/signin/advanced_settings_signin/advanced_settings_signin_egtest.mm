// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"
#import "components/signin/public/base/account_consistency_method.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::GoogleServicesSettingsView;
using chrome_test_util::GoogleServicesSettingsButton;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SyncSettingsConfirmButton;
using l10n_util::GetNSString;
using testing::ButtonWithAccessibilityLabel;

namespace {

// Timeout in seconds to wait for asynchronous sync operations.
const NSTimeInterval kSyncOperationTimeout = 5.0;

}  // namespace

// Interaction tests for advanced settings sign-in with
// |kMobileIdentityConsistency| feature enabled.
@interface AdvancedSettingsSigninTestCase : WebHttpServerChromeTestCase
@end

@implementation AdvancedSettingsSigninTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(signin::kMobileIdentityConsistency);
  return config;
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];
}

- (void)tearDown {
  [super tearDown];
  [ChromeEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];
}

// Tests that signing in, tapping the Settings link on the confirmation screen
// and closing the advanced sign-in settings correctly leaves the user signed
// in.
- (void)testSignInOpenSyncSettings {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [SigninEarlGreyUI tapSettingsLink];
  [[EarlGrey selectElementWithMatcher:SyncSettingsConfirmButton()]
      performAction:grey_tap()];

  // Test the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that a user that signs in and gives sync consent can sign
// out through the "Sign out and Turn Off Sync" > "Clear Data" option in Sync
// settings.
- (void)testSignInOpenSyncSettingsSignOutAndTurnOffSyncWithClearData {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [SigninEarlGreyUI tapSettingsLink];
  [[EarlGrey selectElementWithMatcher:SyncSettingsConfirmButton()]
      performAction:grey_tap()];

  // Test the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out and clear data from Sync settings.
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  [[[EarlGrey selectElementWithMatcher:
                  grey_accessibilityLabel(l10n_util::GetNSString(
                      IDS_IOS_OPTIONS_ACCOUNTS_SIGN_OUT_TURN_OFF_SYNC))]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON)]
      performAction:grey_tap()];

  // Verify signed out.
  [SigninEarlGrey verifySignedOut];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Verify bookmarks are cleared.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
}

// Tests that a user that signs in and gives sync consent can sign
// out through the "Sign out and Turn Off Sync" > "Keep Data" option in Sync
// setting.
- (void)testSignInOpenSyncSettingsSignOutAndTurnOffSyncWithKeepData {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [SigninEarlGreyUI tapSettingsLink];
  [[EarlGrey selectElementWithMatcher:SyncSettingsConfirmButton()]
      performAction:grey_tap()];

  // Test the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out and keep data from Sync settings.
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  [[[EarlGrey selectElementWithMatcher:
                  grey_accessibilityLabel(l10n_util::GetNSString(
                      IDS_IOS_OPTIONS_ACCOUNTS_SIGN_OUT_TURN_OFF_SYNC))]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_SIGNOUT_DIALOG_KEEP_DATA_BUTTON)]
      performAction:grey_tap()];

  // Verify signed out.
  [SigninEarlGrey verifySignedOut];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Verify bookmarks are available.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI verifyEmptyBackgroundIsAbsent];
}

// Tests that "Sign out and Turn Off Sync" is not present in advanced settings.
- (void)testSignInOpenSyncSettingsNoSignOut {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [SigninEarlGreyUI tapSettingsLink];

  [[[EarlGrey selectElementWithMatcher:
                  grey_accessibilityLabel(l10n_util::GetNSString(
                      IDS_IOS_OPTIONS_ACCOUNTS_SIGN_OUT_TURN_OFF_SYNC))]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(
                               kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that a user account with a sync password displays a sync error
// message after sign-in.
- (void)testSigninOpenSyncSettingsWithPasswordError {
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:@"hello"];
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [SigninEarlGreyUI tapSettingsLink];
  [[EarlGrey selectElementWithMatcher:SyncSettingsConfirmButton()]
      performAction:grey_tap()];

  // Test the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Test the sync error message is visible.
  [ChromeEarlGreyUI tapSettingsMenuButton:GoogleServicesSettingsButton()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_SYNC_ERROR_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verifies that advanced sign-in shows an alert dialog when being swiped to
// dismiss.
- (void)testSwipeDownToCancelAdvancedSignin {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [SigninEarlGreyUI tapSettingsLink];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [[EarlGrey
      selectElementWithMatcher:
          ButtonWithAccessibilityLabel(GetNSString(
              IDS_IOS_ADVANCED_SIGNIN_SETTINGS_CANCEL_SYNC_ALERT_CANCEL_SYNC_BUTTON))]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedOut];
}

// Tests interrupting sign-in by opening an URL from another app.
// Sign-in opened from: setting menu.
// Interrupted at: advanced sign-in.
- (void)testInterruptAdvancedSigninSettingsFromAdvancedSigninSettings {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [ChromeEarlGreyUI waitForAppToIdle];

  [SigninEarlGreyUI tapSettingsLink];
  [ChromeEarlGreyUI waitForAppToIdle];

  [ChromeEarlGrey simulateExternalAppURLOpening];

  [ChromeEarlGrey waitForSyncInitialized:NO syncTimeout:kSyncOperationTimeout];
  [SigninEarlGrey verifySignedOut];
}

// Tests interrupting sign-in by opening an URL from another app.
// Sign-in opened from: bookmark view.
// Interrupted at: advanced sign-in.
- (void)testInterruptAdvancedSigninBookmarksFromAdvancedSigninSettings {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:chrome_test_util::BookmarksMenuButton()];
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSettingsLink];
  [ChromeEarlGreyUI waitForAppToIdle];

  [ChromeEarlGrey simulateExternalAppURLOpening];

  [ChromeEarlGrey waitForSyncInitialized:NO syncTimeout:kSyncOperationTimeout];
  [SigninEarlGrey verifySignedOut];
}

// Tests interrupting sign-in by opening an URL from another app.
// Sign-in opened from: recent tabs.
// Interrupted at: advanced sign-in.
- (void)testInterruptSigninFromRecentTabsFromAdvancedSigninSettings {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI tapPrimarySignInButtonInRecentTabs];
  [SigninEarlGreyUI tapSettingsLink];
  [ChromeEarlGreyUI waitForAppToIdle];

  [ChromeEarlGrey simulateExternalAppURLOpening];

  [ChromeEarlGrey waitForSyncInitialized:NO syncTimeout:kSyncOperationTimeout];
  [SigninEarlGrey verifySignedOut];
}

// Tests interrupting sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: advanced sign-in.
- (void)testInterruptSigninFromTabSwitcherFromAdvancedSigninSettings {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI tapPrimarySignInButtonInTabSwitcher];
  [SigninEarlGreyUI tapSettingsLink];
  [ChromeEarlGreyUI waitForAppToIdle];

  [ChromeEarlGrey simulateExternalAppURLOpening];

  [ChromeEarlGrey waitForSyncInitialized:NO syncTimeout:kSyncOperationTimeout];
  [SigninEarlGrey verifySignedOut];
}

@end
