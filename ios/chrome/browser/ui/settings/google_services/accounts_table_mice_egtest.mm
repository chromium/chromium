// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "components/signin/ios/browser/features.h"
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
@interface AccountTableMiceTestCase : WebHttpServerChromeTestCase
@end

@implementation AccountTableMiceTestCase

- (void)setUp {
  [super setUp];

  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];
  GREYAssertEqual(
      [ChromeEarlGrey numberOfSyncEntitiesWithType:syncer::BOOKMARKS], 0,
      @"No bookmarks should exist before tests start.");
}

- (void)tearDown {
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [ChromeEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];

  [ChromeEarlGrey clearSyncServerData];
  [super tearDown];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(signin::kSimplifySignOutIOS);
  return config;
}

// Tests that selecting sign-out from a non-managed account keeps the user's
// synced data.
// TODO(crbug.com/1166148) This test needs to be moved back into
// accounts_table_egtest.mm once kSimplifySignOutIOS is removed.
- (void)testSignOutFromNonManagedAccountKeepsData {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];

  // Sign In |fakeIdentity|.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceKeepData];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Assert that the no bookmarks label is not present.
  [[EarlGrey selectElementWithMatcher:NoBookmarksLabel()]
      assertWithMatcher:grey_nil()];
}

// Tests that selecting sign-out and clear data from a non-managed user account
// clears the user's synced data.
// TODO(crbug.com/1166148) This test needs to be moved back into
// accounts_table_egtest.mm once kSimplifySignOutIOS is removed.
- (void)testSignOutAndClearDataFromNonManagedAccountClearsData {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];

  // Sign In |fakeIdentity|.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceClearData];

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
// TODO(crbug.com/1166148) This test needs to be moved back into
// accounts_table_egtest.mm once kSimplifySignOutIOS is removed.
- (void)testsSignOutFromManagedAccount {
  // Sign In |fakeManagedIdentity|.
  [SigninEarlGreyUI
      signinWithFakeIdentity:[SigninEarlGrey fakeManagedIdentity]];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceClearData];

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
// TODO(crbug.com/1166148) This test needs to be moved back into
// accounts_table_egtest.mm once kSimplifySignOutIOS is removed.
- (void)testSwitchingAccountsWithClearedData {
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGrey fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [SigninEarlGrey fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceClearData];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity2];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity2];
}

// Tests to open the sign-out confirmation dialog, and then remove the primary
// account while the dialog is still opened.
// TODO(crbug.com/1166148) This test needs to be moved back into
// accounts_table_egtest.mm once kSimplifySignOutIOS is removed.
// TODO(crbug.com/1179458): Re-enable.
- (void)DISABLED_testRemovePrimaryAccountWhileSignOutConfirmation {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Opens the sign out confirmation dialog.
  [ChromeEarlGreyUI
      tapAccountsMenuButton:chrome_test_util::SignOutAccountsButton()];
  // Remove the primary accounts.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts
                           closeButton:YES];

  // Closes the settings.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];
}

// Tests to open the sign-out confirmation dialog, and then remove a secondary
// account while the dialog is still opened.
// TODO(crbug.com/1166148) This test needs to be moved back into
// accounts_table_egtest.mm once kSimplifySignOutIOS is removed.
- (void)testRemoveSecondaryAccountWhileSignOutConfirmation {
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGrey fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [SigninEarlGrey fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Opens the sign out confirmation dialog.
  [ChromeEarlGreyUI
      tapAccountsMenuButton:chrome_test_util::SignOutAccountsButton()];
  // Remove the primary accounts.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity2];
  [ChromeEarlGreyUI waitForAppToIdle];
  [[[EarlGrey
      selectElementWithMatcher:grey_anyOf(chrome_test_util::CancelButton(),
                                          SignOutAccountsButton(), nil)]
      atIndex:1] performAction:grey_tap()];

  // Closes the settings.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  // Verifies we are still signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];
}

// Tests to open the sign-out confirmation dialog, and then open an external
// URL.
// TODO(crbug.com/1166148) This test needs to be moved back into
// accounts_table_egtest.mm once kSimplifySignOutIOS is removed.
// TODO(crbug.com/1179458): Re-enable.
- (void)DISABLED_testRemoveInterrupSignOutConfirmation {
  if (@available(iOS 13, *)) {
    // TODO(crbug.com/1179231) Enable the test for iOS 13.
    EARL_GREY_TEST_DISABLED(@"Fails on iOS 13.");
  }
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Opens the sign out confirmation dialog.
  [ChromeEarlGreyUI
      tapAccountsMenuButton:chrome_test_util::SignOutAccountsButton()];
  // Open the URL as if it was opened from another app.
  [ChromeEarlGrey simulateExternalAppURLOpening];
  // Verifies we are still signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests to sign out with a non managed account without syncing.
// TODO(crbug.com/1166148) This test needs to be moved back into
// accounts_table_egtest.mm once kSimplifySignOutIOS is removed.
- (void)testSignOutWithNonManagedAccountFromNoneSyncingAccount {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  // Add a bookmark.
  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceNotSyncing];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Assert that the no bookmarks label is not present.
  [[EarlGrey selectElementWithMatcher:NoBookmarksLabel()]
      assertWithMatcher:grey_nil()];
}

// Tests to sign out with a managed account without syncing.
// TODO(crbug.com/1166148) This test needs to be moved back into
// accounts_table_egtest.mm once kSimplifySignOutIOS is removed.
- (void)testSignOutWithManagedAccountFromNoneSyncingAccount {
  // Sign In |fakeManagedIdentity|.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeManagedIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceNotSyncing];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Assert that the no bookmarks label is not present.
  [[EarlGrey selectElementWithMatcher:NoBookmarksLabel()]
      assertWithMatcher:grey_nil()];
}

@end
