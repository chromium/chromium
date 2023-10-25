// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/bookmarks/common/storage_type.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_egtest_util.h"
#import "ios/chrome/browser/shared/ui/elements/elements_constants.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::Omnibox;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsCollectionView;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsSignInRowMatcher;
using chrome_test_util::SignOutAccountsButton;

namespace {

// Constant for timeout while waiting for asynchronous sync operations.
constexpr base::TimeDelta kSyncOperationTimeout = base::Seconds(10);
}

// TODO(crbug.com/1403825): Fails on device.
// Note: Defined here instead of with the test case because it's needed in
// appConfigurationForTestCase.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testInterruptDuringSignOutConfirmation \
  testInterruptDuringSignOutConfirmation
#else
#define MAYBE_testInterruptDuringSignOutConfirmation \
  DISABLED_testInterruptDuringSignOutConfirmation
#endif

// Integration tests using the Account Settings screen.
@interface AccountsTableTestCase : WebHttpServerChromeTestCase
@end

@implementation AccountsTableTestCase

- (void)tearDown {
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];

  [ChromeEarlGrey clearSyncServerData];
  [super tearDown];
}

- (void)setUp {
  [super setUp];

  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGrey clearBookmarks];
  GREYAssertEqual(
      [ChromeEarlGrey numberOfSyncEntitiesWithType:syncer::BOOKMARKS], 0,
      @"No bookmarks should exist before tests start.");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  // With kReplaceSyncPromosWithSignInPromos enabled, several of the tests here
  // don't apply anymore.
  if (  // There is no signout confirmation anymore.
      [self isRunningTest:@selector(testSignOutCancelled)] ||
      [self isRunningTest:@selector
            (testRemoveSecondaryAccountWhileSignOutConfirmation)] ||
      [self isRunningTest:@selector
            (MAYBE_testInterruptDuringSignOutConfirmation)] ||
      [self isRunningTest:@selector(testDismissSignOutConfirmationTwice)] ||
      // Data (of a managed account) is not cleared on signout anymore.
      [self isRunningTest:@selector
            (testsManagedAccountRemovedFromAnotherGoogleApp)] ||
      // Sync can't be turned on anymore.
      [self isRunningTest:@selector(testSignOutFooterForSignInAndSyncUser)]) {
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  } else {
    config.features_enabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }

  return config;
}

- (void)openAccountSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  if ([ChromeEarlGrey isReplaceSyncWithSigninEnabled]) {
    // If ReplaceSyncWithSignin is enabled, we're now on the unified settings
    // page, and need to tap "Manage accounts on this device" to get to the
    // accounts view.
    // First scroll down so that the button is visible.
    id<GREYMatcher> scrollViewMatcher =
        grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
    [[EarlGrey selectElementWithMatcher:scrollViewMatcher]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

    // Now tab the "Manage accounts on this device" button.
    id<GREYMatcher> manageAccountsButtonMatcher =
        grey_allOf(grey_text(l10n_util::GetNSString(
                       IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_ACCOUNTS_ITEM)),
                   grey_sufficientlyVisible(), nil);
    [[EarlGrey selectElementWithMatcher:manageAccountsButtonMatcher]
        performAction:grey_tap()];
  }
}

// Tests that the Sync and Account Settings screen are correctly popped if the
// signed in account is removed.
- (void)testSignInPopUpAccountOnSyncSettings {
  // TODO(crbug.com/1493677): Test fails on iPad.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Fails on iPad.");
  }

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In `identity`, then open the Sync Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [self openAccountSettings];

  // Forget `fakeIdentity`, screens should be popped back to the Main Settings.
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGrey verifySignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is correctly popped if the signed in
// account is removed while the "Disconnect Account" dialog is up.
- (void)testSignInPopUpAccountOnDisconnectAccount {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In `fakeIdentity`, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [self openAccountSettings];
  [ChromeEarlGreyUI tapAccountsMenuButton:SignOutAccountsButton()];

  // Forget `fakeIdentity`, screens should be popped back to the Main Settings.
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:SettingsSignInRowMatcher()];
  [SigninEarlGrey verifySignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that the Account Settings screen is correctly reloaded when one of
// the non-primary account is removed.
- (void)testSignInReloadOnRemoveAccount {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  // Sign In `fakeIdentity`, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [self openAccountSettings];

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
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In `fakeIdentity`, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [self openAccountSettings];

  // Open MyGoogleUI.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                   fakeIdentity.userEmail)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ButtonWithAccessibilityLabel(
              l10n_util::GetNSString(IDS_IOS_MANAGE_YOUR_GOOGLE_ACCOUNT_TITLE))]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

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
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  FakeSystemIdentity* fakeIdentity3 = [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity3];

  // Sign In `fakeIdentity`, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [self openAccountSettings];

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
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In `fakeIdentity`, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [self openAccountSettings];

  [SigninEarlGreyUI tapRemoveAccountFromDeviceWithFakeIdentity:fakeIdentity];

  // Check that the user is signed out and the Main Settings screen is shown.
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGrey verifySignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests that selecting sign-out from a non-managed account keeps the user's
// synced data.
- (void)testSignOutFromNonManagedAccountKeepsData {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In `fakeIdentity`.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];

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
// TODO(crbug.com/1377798): fails on iPhone SE because the screen is too small
// to present both the prompt to select and account and the background view.
- (void)DISABLED_testSignOutAndClearDataFromNonManagedAccountClearsData {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In `fakeIdentity`.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];

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
// TODO(crbug.com/1377800): fails on iPhone SE because the screen is too small
// to present both the prompt to select and account and the background view.
- (void)DISABLED_testsSignOutFromManagedAccount {
  // Sign In `fakeManagedIdentity`.
  [SigninEarlGreyUI
      signinWithFakeIdentity:[FakeSystemIdentity fakeManagedIdentity]];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];

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
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
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
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In `fakeIdentity`, then open the Account Settings.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [self openAccountSettings];

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
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelsLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];

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
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [self openAccountSettings];

  // Opens the sign out confirmation dialog.
  [ChromeEarlGreyUI
      tapAccountsMenuButton:chrome_test_util::SignOutAccountsButton()];
  // Wait until the sheet is fully presented before removing the identity.
  [ChromeEarlGreyUI waitForAppToIdle];
  // Remove the primary accounts.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Closes the settings.
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];
  [SigninEarlGrey verifySignedOut];
}

// Tests to open the sign-out confirmation dialog, and then remove a secondary
// account while the dialog is still opened.
- (void)testRemoveSecondaryAccountWhileSignOutConfirmation {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];

  [self openAccountSettings];

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
// Note: The MAYBE_ macro is defined at the top of the file because it's needed
// in appConfigurationForTestCase.
- (void)MAYBE_testInterruptDuringSignOutConfirmation {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [self openAccountSettings];

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
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [self openAccountSettings];

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
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  // Add a bookmark.
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];

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
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];

  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:bookmarks::StorageType::kLocalOrSyncable];

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
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:NO];
  [self openAccountSettings];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                         IDS_IOS_DISCONNECT_DIALOG_SYNCING_FOOTER_INFO_MOBILE)),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

// Tests that the sign-out footer is shown when the user is syncing.
- (void)testSignOutFooterForSignInAndSyncUser {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity enableSync:YES];
  [self openAccountSettings];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_text(l10n_util::GetNSString(
                         IDS_IOS_DISCONNECT_DIALOG_SYNCING_FOOTER_INFO_MOBILE)),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

@end
