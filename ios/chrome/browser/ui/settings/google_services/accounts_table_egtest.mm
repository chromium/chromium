// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "components/signin/public/base/account_consistency_method.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SignOutAccountsButton;

namespace {

// Constant for timeout while waiting for asynchronous sync operations.
const NSTimeInterval kSyncOperationTimeout = 10.0;

// Returns a matcher for when there are no bookmarks saved.
id<GREYMatcher> NoBookmarksLabel() {
  return grey_text(l10n_util::GetNSString(IDS_IOS_BOOKMARK_NO_BOOKMARKS_LABEL));
}
}

// Integration tests using the Account Settings screen.
@interface AccountCollectionsTestCase : WebHttpServerChromeTestCase
@end

@implementation AccountCollectionsTestCase

- (void)tearDown {
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];

  [ChromeEarlGrey clearSyncServerData];
  [super tearDown];
}

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];
  GREYAssertEqual(
      [ChromeEarlGrey numberOfSyncEntitiesWithType:syncer::BOOKMARKS], 0,
      @"No bookmarks should exist before tests start.");
}

// Tests that the Sync and Account Settings screen are correctly popped if the
// signed in account is removed.
- (void)testSignInPopUpAccountOnSyncSettings {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];

  // Sign In |identity|, then open the Sync Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Forget |fakeIdentity|, screens should be popped back to the Main Settings.
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];

  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGrey verifySignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is correctly popped if the signed in
// account is removed while the "Disconnect Account" dialog is up.
- (void)testSignInPopUpAccountOnDisconnectAccount {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign In |fakeIdentity|, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [ChromeEarlGreyUI tapAccountsMenuButton:SignOutAccountsButton()];

  // Forget |fakeIdentity|, screens should be popped back to the Main Settings.
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];

  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGrey verifySignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is correctly reloaded when one of
// the non-primary account is removed.
- (void)testSignInReloadOnRemoveAccount {
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGrey fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [SigninEarlGrey fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Sign In |fakeIdentity|, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  [SigninEarlGreyUI tapRemoveAccountFromDeviceWithFakeIdentity:fakeIdentity2];

  // Check that |fakeIdentity2| isn't available anymore on the Account Settings.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              fakeIdentity2.userEmail),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is popped and the user signed out
// when the account is removed.
- (void)testSignOutOnRemoveAccount {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];

  // Sign In |fakeIdentity|, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  [SigninEarlGreyUI tapRemoveAccountFromDeviceWithFakeIdentity:fakeIdentity];

  // Check that the user is signed out and the Main Settings screen is shown.
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGrey verifySignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that selecting sign-out from a non-managed account keeps the user's
// synced data.
- (void)testSignOutFromNonManagedAccountKeepsData {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];

  // Sign In |fakeIdentity|.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out.
  [SigninEarlGreyUI signOut];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Assert that the no bookmarks label is not present.
  [[EarlGrey selectElementWithMatcher:NoBookmarksLabel()]
      assertWithMatcher:grey_nil()];
}

// Tests that selecting sign-out and clear data from a non-managed user account
// clears the user's synced data.
- (void)testSignOutAndClearDataFromNonManagedAccountClearsData {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];

  // Sign In |fakeIdentity|.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out.
  [SigninEarlGreyUI signOutAndClearDataFromDevice];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];

  // Assert that there are no bookmarks.
  if ([ChromeEarlGrey isIllustratedEmptyStatesEnabled]) {
    // The empty background appears in the root directory if the leaf folders
    // are empty.
    [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
  } else {
    [BookmarkEarlGreyUI openMobileBookmarks];
    [[EarlGrey selectElementWithMatcher:NoBookmarksLabel()]
        assertWithMatcher:grey_notNil()];
  }
}

// Tests that signing out from a managed user account clears the user's data.
- (void)testsSignOutFromManagedAccount {
  // Sign In |fakeManagedIdentity|.
  [SigninEarlGreyUI
      signinWithFakeIdentity:[SigninEarlGrey fakeManagedIdentity]];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out.
  [SigninEarlGreyUI signOutAndClearDataFromDevice];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];

  // Assert that there are no bookmarks.
  if ([ChromeEarlGrey isIllustratedEmptyStatesEnabled]) {
    // The empty background appears in the root directory if the leaf folders
    // are empty.
    [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
  } else {
    [BookmarkEarlGreyUI openMobileBookmarks];
    [[EarlGrey selectElementWithMatcher:NoBookmarksLabel()]
        assertWithMatcher:grey_notNil()];
  }
}

// Tests that given two accounts A and B that are available on the device -
// signing in and out from account A, then signing in to account B, properly
// identifies the user with account B.
- (void)testSwitchingAccountsWithClearedData {
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGrey fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [SigninEarlGrey fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUI signOutAndClearDataFromDevice];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity2];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity2];
}

// Tests that the user isn't signed out and the UI is correct when the
// disconnect is cancelled in the Account Settings screen.
#if !TARGET_IPHONE_SIMULATOR
// TODO(crbug.com/669613): Re-enable this test on devices.
#define MAYBE_testSignInDisconnectCancelled \
  DISABLED_testSignInDisconnectCancelled
#else
#define MAYBE_testSignInDisconnectCancelled testSignInDisconnectCancelled
#endif
- (void)MAYBE_testSignInDisconnectCancelled {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];

  // Sign In |fakeIdentity|, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Open the "Disconnect Account" dialog, then tap "Cancel".
  [ChromeEarlGreyUI tapAccountsMenuButton:SignOutAccountsButton()];
  // Note that the iPad does not provide a CANCEL button by design. Click
  // anywhere on the screen to exit.
  [[[EarlGrey
      selectElementWithMatcher:grey_anyOf(chrome_test_util::CancelButton(),
                                          SignOutAccountsButton(), nil)]
      atIndex:1] performAction:grey_tap()];

  // Check that Account Settings screen is open and |fakeIdentity| is signed in.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsAccountsCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

@end
