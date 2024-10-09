// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_storage_type.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_earl_grey_ui.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_egtest_util.h"
#import "ios/chrome/browser/shared/ui/elements/elements_constants.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller_constants.h"
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
}  // namespace

// Integration tests using the Account Settings screen.
@interface LegacyAccountsTableTestCase : WebHttpServerChromeTestCase
@end

@implementation LegacyAccountsTableTestCase

- (void)tearDown {
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey clearBookmarks];
  [BookmarkEarlGrey clearBookmarksPositionCache];

  [ChromeEarlGrey clearFakeSyncServerData];
  [super tearDown];
}

- (void)setUp {
  [super setUp];

  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey clearBookmarks];
  GREYAssertEqual(
      [ChromeEarlGrey numberOfSyncEntitiesWithType:syncer::BOOKMARKS], 0,
      @"No bookmarks should exist befoe tests start.");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config = [super appConfigurationForTestCase];

  // TODO(b/298795580): Remove this once the User Policy field trial config is
  // merged.
  config.features_enabled.push_back(
      policy::kUserPolicyForSigninAndNoSyncConsentLevel);
  if ([self isRunningTest:@selector
            (testSignOutWithManagedAccountFromNoneSyncingAccount)]) {
    // Disable `kClearDeviceDataOnSignOutForManagedUsers` because the feature
    // shows a different dialog
    config.features_disabled.push_back(
        kClearDeviceDataOnSignOutForManagedUsers);
  }
  config.features_disabled.push_back(kIdentityDiscAccountMenu);

  return config;
}

- (void)openAccountSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Tap "Manage accounts on this device" to get to the accounts view.
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

// Tests that the Sync and Account Settings screen are correctly popped if the
// signed in account is removed.
- (void)testSignInPopUpAccountOnSyncSettings {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In `identity`, then open the Sync Settings.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [self openAccountSettings];

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

// Tests that the Account Settings screen is correctly popped if the signed in
// account is removed while the "Disconnect Account" dialog is up.
- (void)testSignInPopUpAccountOnDisconnectAccount {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In `fakeIdentity`, then open the Account Settings.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [self openAccountSettings];
  [ChromeEarlGreyUI tapAccountsMenuButton:SignOutAccountsButton()];
  [SigninEarlGreyUI dismissSignoutSnackbar];

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
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];
  [self openAccountSettings];

  [SigninEarlGreyUI tapRemoveAccountFromDeviceWithFakeIdentity:fakeIdentity2];

  // Check that `fakeIdentity2` isn't available anymore on the Account Settings.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(
                                              fakeIdentity2.userEmail),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kSettingsAccountsTableViewDoneButtonId)]
      performAction:grey_tap()];
}

// Opens the MyGoogleUI for the primary account, and then the primary account
// is removed while MyGoogle UI is still opened.
- (void)testRemoveAccountWithMyGoogleUIOpened {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In `fakeIdentity`, then open the Account Settings.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
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
  // TODO(crbug.com/335444727): Investigate why enableHistorySync:YES is needed
  // here. More details in the linked bug.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1 enableHistorySync:YES];
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
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [self openAccountSettings];

  [SigninEarlGreyUI tapRemoveAccountFromDeviceWithFakeIdentity:fakeIdentity];

  // Check that the user is signed out and the Main Settings screen is shown.
  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGrey verifySignedOut];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Tests to open the account details twice in a row.
// TODO(crbug.com/357145635): Test failing on builders.
- (void)testOpenTwiceAccountDetails {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In `fakeIdentity`, then open the Account Settings.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [self openAccountSettings];
  // Open the account details view twice.
  for (int i = 0; i < 2; ++i) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                     fakeIdentity.userEmail)]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabel(
                       l10n_util::GetNSString(
                           IDS_IOS_MANAGE_YOUR_GOOGLE_ACCOUNT_TITLE))]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kFakeAccountDetailsViewIdentifier)]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(
                                     kFakeAccountDetailsDoneButtonIdentifier)]
        performAction:grey_tap()];
  }
}

// Tests that selecting sign-out from a non-managed account keeps the user's
// local data.
- (void)testSignOutFromNonManagedAccountKeepsLocalData {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In `fakeIdentity`.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];

  [SigninEarlGreyUI signOut];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Assert that the empty state background is absent.
  [BookmarkEarlGreyUI verifyEmptyBackgroundIsAbsent];
}

// Tests that selecting sign-out from a non-managed user account clears the
// user's account data.
- (void)testSignOutFromNonManagedAccountClearsAccountData {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  // Sign In `fakeIdentity`.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kAccount];

  [SigninEarlGreyUI signOut];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];

  // Assert that there are no bookmarks. The empty background appears in the
  // root directory if the leaf folders are empty.
  [BookmarkEarlGreyUI verifyEmptyBackgroundAppears];
}

// Tests that signing out from a managed user account keeps the user's local
// data.
// TODO(crbug.com/331778550): Flaky. Re-enable when fixed.
- (void)DISABLED_testSignOutFromManagedAccountKeepsLocalData {
  // Sign In `fakeManagedIdentity`.
  [SigninEarlGreyUI
      signinWithFakeIdentity:[FakeSystemIdentity fakeManagedIdentity]];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];

  [SigninEarlGreyUI signOut];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];

  // Assert that there are no bookmarks. The empty background appears in the
  // root directory if the leaf folders are empty.
  [BookmarkEarlGreyUI verifyEmptyBackgroundIsAbsent];
}

// Tests that signing out from a managed user account clears the user's account
// data.
// TODO(crbug.com/331778550): Flaky. Re-enable when fixed.
- (void)DISABLED_testSignOutFromManagedAccountClearsAccountData {
  // Sign In `fakeManagedIdentity`.
  [SigninEarlGreyUI
      signinWithFakeIdentity:[FakeSystemIdentity fakeManagedIdentity]];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kAccount];

  [SigninEarlGreyUI signOut];

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

  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUI signOut];

  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity2];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity2];
}

// Tests that users data is not cleared when the signed in account disappear and
// it is a managed account.
// TODO(crbug.com/355243751): This test is flaky on simulator.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testManagedAccountRemovedFromAnotherGoogleApp \
  FLAKY_testManagedAccountRemovedFromAnotherGoogleApp
#else
#define MAYBE_testManagedAccountRemovedFromAnotherGoogleApp \
  testManagedAccountRemovedFromAnotherGoogleApp
#endif
- (void)MAYBE_testManagedAccountRemovedFromAnotherGoogleApp {
  // Sign In `fakeManagedIdentity`.
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark after sync is initialized.
  [ChromeEarlGrey waitForSyncEngineInitialized:YES
                                   syncTimeout:kSyncOperationTimeout];
  [BookmarkEarlGrey waitForBookmarkModelLoaded];
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];

  // Simulate that the user remove their primary account from another Google
  // app.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];

  // Wait until sign-out and the overlay disappears.
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey verifySignedOut];
  WaitForActivityOverlayToDisappear();

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];

  // Assert that the bookmarks are still there.
  [BookmarkEarlGreyUI verifyEmptyBackgroundIsAbsent];
}

// Tests to open the sign-out confirmation dialog, and then remove the primary
// account while the dialog is still opened.
- (void)testRemovePrimaryAccountWhileSignOutConfirmation {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  [self openAccountSettings];

  // Opens the sign out confirmation dialog.
  [ChromeEarlGreyUI
      tapAccountsMenuButton:chrome_test_util::SignOutAccountsButton()];
  [SigninEarlGreyUI dismissSignoutSnackbar];
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

// Tests to sign out with a non managed account without syncing.
- (void)testSignOutWithNonManagedAccountFromNoneSyncingAccount {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Add a bookmark.
  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];

  [SigninEarlGreyUI signOut];

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
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  [BookmarkEarlGrey
      setupStandardBookmarksInStorage:BookmarkStorageType::kLocalOrSyncable];

  [SigninEarlGreyUI signOut];

  // Open the Bookmarks screen on the Tools menu.
  [BookmarkEarlGreyUI openBookmarks];
  [BookmarkEarlGreyUI openMobileBookmarks];

  // Assert that the empty state background is absent.
  [BookmarkEarlGreyUI verifyEmptyBackgroundIsAbsent];
}

// Tests that the sign-out footer is not shown when the user is not syncing.
- (void)testSignOutFooterForSignInOnlyUser {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];
  [self openAccountSettings];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                         IDS_IOS_DISCONNECT_DIALOG_SYNCING_FOOTER_INFO_MOBILE)),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

@end
