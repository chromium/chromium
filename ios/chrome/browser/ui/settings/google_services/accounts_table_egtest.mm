// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/elements/activity_overlay_egtest_util.h"
#import "ios/chrome/browser/ui/elements/elements_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::Omnibox;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsCollectionView;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SignOutAccountsButton;

namespace {

// Constant for timeout while waiting for asynchronous sync operations.
const NSTimeInterval kSyncOperationTimeout = 10.0;
}

// Integration tests using the Account Settings screen.
@interface AccountsTableTestCase : WebHttpServerChromeTestCase
@end

@implementation AccountsTableTestCase

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
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];

  // Sign In `identity`, then open the Sync Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Forget `fakeIdentity`, screens should be popped back to the Main Settings.
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
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Sign In `fakeIdentity`, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [ChromeEarlGreyUI tapAccountsMenuButton:SignOutAccountsButton()];

  // Forget `fakeIdentity`, screens should be popped back to the Main Settings.
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:PrimarySignInButton()];
  [SigninEarlGrey verifySignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is correctly reloaded when one of
// the non-primary account is removed.
- (void)testSignInReloadOnRemoveAccount {
  FakeChromeIdentity* fakeIdentity1 = [FakeChromeIdentity fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [FakeChromeIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Sign In `fakeIdentity`, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  [SigninEarlGreyUI tapRemoveAccountFromDeviceWithFakeIdentity:fakeIdentity2];

  // Check that `fakeIdentity2` isn't available anymore on the Account Settings.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              fakeIdentity2.userEmail),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Opens the MyGoogleUI for the primary account, and then the primary account
// is removed while MyGoogle UI is still opened.
- (void)testRemoveAccountWithMyGoogleUIOpened {
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];

  // Sign In `fakeIdentity`, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Open MyGoogleUI.
  [SigninEarlGreyUI openMyGoogleDialogWithFakeIdentity:fakeIdentity];

  // Forget Identity.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey verifySignedOut];

  // Close settings.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests opening the remove identity confirmation dialog once, and cancel the
// dialog. Then we open the remove identity confirmation dialog to remove the
// identity. And finally the remove identity confirmation dialog is opened a
// third time to remove a second identity.
// The goal of this test is to confirm the dialog can be opened several times.
// Related to http://crbug.com/1180798
- (void)testRemoveAccountSeveralTime {
  FakeChromeIdentity* fakeIdentity1 = [FakeChromeIdentity fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [FakeChromeIdentity fakeIdentity2];
  FakeChromeIdentity* fakeIdentity3 = [FakeChromeIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity3];

  // Sign In `fakeIdentity`, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Open the remove identity confirmation dialog for the first time.
  [SigninEarlGreyUI
      openRemoveAccountConfirmationDialogWithFakeIdentity:fakeIdentity2];
  // Cancel it.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()];
  // Open it a second time, confirmal removal.
  [SigninEarlGreyUI tapRemoveAccountFromDeviceWithFakeIdentity:fakeIdentity2];
  // Open it a third time, confirmal removal.
  [SigninEarlGreyUI tapRemoveAccountFromDeviceWithFakeIdentity:fakeIdentity3];
}

// Tests that the Account Settings screen is popped and the user signed out
// when the account is removed.
- (void)testSignOutOnRemoveAccount {
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];

  // Sign In `fakeIdentity`, then open the Account Settings.
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
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];

  // Sign In `fakeIdentity`.
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

  // Assert that the empty state background is absent.
  [BookmarkEarlGreyUI verifyEmptyBackgroundIsAbsent];
}

// Tests that selecting sign-out and clear data from a non-managed user account
// clears the user's synced data.
- (void)testSignOutAndClearDataFromNonManagedAccountClearsData {
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];

  // Sign In `fakeIdentity`.
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

  // Assert that there are no bookmarks. The empty background appears in the
  // root directory if the leaf folders are empty.
  [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
}

// Tests that signing out from a managed user account clears the user's data.
- (void)testsSignOutFromManagedAccount {
  // Sign In `fakeManagedIdentity`.
  [SigninEarlGreyUI
      signinWithFakeIdentity:[FakeChromeIdentity fakeManagedIdentity]];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceClearData];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];

  // Assert that there are no bookmarks. The empty background appears in the
  // root directory if the leaf folders are empty.
  [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
}

// Tests that given two accounts A and B that are available on the device -
// signing in and out from account A, then signing in to account B, properly
// identifies the user with account B.
- (void)testSwitchingAccountsWithClearedData {
  FakeChromeIdentity* fakeIdentity1 = [FakeChromeIdentity fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [FakeChromeIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceClearData];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity2];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity2];
}

// Tests that the user isn't signed out and the UI is correct when the
// sign-out is cancelled in the Account Settings screen.
- (void)testSignOutCancelled {
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];

  // Sign In `fakeIdentity`, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Open the SignOut dialog, then tap "Cancel".
  [ChromeEarlGreyUI tapAccountsMenuButton:SignOutAccountsButton()];
  // Note that the iPad does not provide a CANCEL button by design. Click
  // anywhere on the screen to exit.
  [[[EarlGrey
      selectElementWithMatcher:grey_anyOf(chrome_test_util::CancelButton(),
                                          SignOutAccountsButton(), nil)]
      atIndex:1] performAction:grey_tap()];

  // Check that Account Settings screen is open and `fakeIdentity` is signed in.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsAccountsCollectionView()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that users data is cleared out when the signed in account disappear and
// it is a managed account. Regression test for crbug.com/1208381.
- (void)testsManagedAccountRemovedFromAnotherGoogleApp {
  // Sign In `fakeManagedIdentity`.
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeManagedIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:kSyncOperationTimeout];
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  [BookmarkEarlGrey setupStandardBookmarks];

  // Simulate that the user remove their primary account from another Google
  // app.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];

  // Wait until sign-out and the overlay disappears.
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey verifySignedOut];
  WaitForActivityOverlayToDisappear();

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];

  // Assert that there are no bookmarks. The empty background appears in the
  // root directory if the leaf folders are empty.
  [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
}

// Tests to open the sign-out confirmation dialog, and then remove the primary
// account while the dialog is still opened.
- (void)testRemovePrimaryAccountWhileSignOutConfirmation {
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Opens the sign out confirmation dialog.
  [ChromeEarlGreyUI
      tapAccountsMenuButton:chrome_test_util::SignOutAccountsButton()];
  // Wait until the sheet is fully presented before removing the identity.
  [ChromeEarlGreyUI waitForAppToIdle];
  // Remove the primary accounts.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGreyUI
      verifySigninPromoVisibleWithMode:SigninPromoViewModeNoAccounts
                           closeButton:YES];

  // Closes the settings.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];
  [SigninEarlGrey verifySignedOut];
}

// Tests to open the sign-out confirmation dialog, and then remove a secondary
// account while the dialog is still opened.
- (void)testRemoveSecondaryAccountWhileSignOutConfirmation {
  FakeChromeIdentity* fakeIdentity1 = [FakeChromeIdentity fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [FakeChromeIdentity fakeIdentity2];
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
- (void)testInterruptDuringSignOutConfirmation {
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Opens the sign out confirmation dialog.
  [ChromeEarlGreyUI
      tapAccountsMenuButton:chrome_test_util::SignOutAccountsButton()];
  // Wait until the sheet is fully presented before to opening an external URL.
  [ChromeEarlGreyUI waitForAppToIdle];
  // Open the URL as if it was opened from another app.
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL expectedURL = self.testServer->GetURL("/echo");
  [ChromeEarlGrey
      simulateExternalAppURLOpeningAndWaitUntilOpenedWithGURL:expectedURL];
  // Verifies that the user is signed in and Settings have been dismissed.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests that opening and closing the sign-out confirmation dialog does
// not affect the user's sign-in state.
- (void)testDismissSignOutConfirmationTwice {
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Opens the sign out confirmation dialog.
  [ChromeEarlGreyUI
      tapAccountsMenuButton:chrome_test_util::SignOutAccountsButton()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Close the dialog.
  [[[EarlGrey
      selectElementWithMatcher:grey_anyOf(chrome_test_util::CancelButton(),
                                          SignOutAccountsButton(), nil)]
      atIndex:1] performAction:grey_tap()];

  // Opens the sign out confirmation dialog.
  [ChromeEarlGreyUI
      tapAccountsMenuButton:chrome_test_util::SignOutAccountsButton()];

  // Close the dialog.
  [[[EarlGrey
      selectElementWithMatcher:grey_anyOf(chrome_test_util::CancelButton(),
                                          SignOutAccountsButton(), nil)]
      atIndex:1] performAction:grey_tap()];

  // Verify that the user is still signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests to sign out with a non managed account without syncing.
- (void)testSignOutWithNonManagedAccountFromNoneSyncingAccount {
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];
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

  // Assert that the empty state background is absent.
  [BookmarkEarlGreyUI verifyEmptyBackgroundIsAbsent];
}

// Tests to sign out with a managed account without syncing.
- (void)testSignOutWithManagedAccountFromNoneSyncingAccount {
  // Sign In `fakeManagedIdentity`.
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeManagedIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  [BookmarkEarlGrey setupStandardBookmarks];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithConfirmationChoice:SignOutConfirmationChoiceNotSyncing];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Assert that the empty state background is absent.
  [BookmarkEarlGreyUI verifyEmptyBackgroundIsAbsent];
}

// Tests that the sign-out footer is not shown when the user is not syncing.
- (void)testSignOutFooterForSignInOnlyUser {
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                         IDS_IOS_DISCONNECT_DIALOG_SYNCING_FOOTER_INFO_MOBILE)),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

// Tests that the sign-out footer is shown when the user is syncing.
- (void)testSignOutFooterForSignInAndSyncUser {
  FakeChromeIdentity* fakeIdentity = [FakeChromeIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:YES];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_text(l10n_util::GetNSString(
                         IDS_IOS_DISCONNECT_DIALOG_SYNCING_FOOTER_INFO_MOBILE)),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

@end
