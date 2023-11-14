// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/i18n/message_formatter.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/common/storage_type.h"
#import "components/policy/policy_constants.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/policy/policy_earl_grey_utils.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_earl_grey.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_egtest_utils.h"
#import "ios/chrome/browser/ui/settings/password/password_settings_app_interface.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/web_http_server_chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util.h"

using chrome_test_util::ManageSyncSettingsButton;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsSignInRowMatcher;

namespace {

NSString* const kPassphrase = @"hello";

void SignInWithPromoFromAccountSettings(FakeSystemIdentity* fake_identity,
                                        BOOL expect_history_sync) {
  GREYAssertTrue(
      [ChromeEarlGrey isReplaceSyncWithSigninEnabled],
      @"Expected sign-in sheet to show but SyncToSignin flag is disabled");
  // Sign in with fake identity using the settings sign-in promo.
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::SettingsSignInRowMatcher()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::IdentityCellMatcherForEmail(
                                   fake_identity.userEmail)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::StaticTextWithAccessibilityLabel(
                  l10n_util::GetNSStringF(
                      IDS_IOS_FIRST_RUN_SIGNIN_CONTINUE_AS,
                      base::SysNSStringToUTF16(fake_identity.userGivenName))),
              grey_sufficientlyVisible(), nil)] performAction:grey_tap()];
  if (expect_history_sync) {
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::SigninScreenPromoPrimaryButtonMatcher()]
        performAction:grey_tap()];
  }
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fake_identity];

  // Close settings and re-open it to make sure the settings UI is updated to
  // avoid flakiness.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [ChromeEarlGreyUI openSettingsMenu];

  // Check that Settings is presented.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];
}

void SignOutFromAccountSettings() {
  // Scroll to the bottom to view the signout button.
  id<GREYMatcher> scroll_view_matcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:scroll_view_matcher]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "Sign out" button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_ITEM))]
      performAction:grey_tap()];
}

void DismissSignOutSnackbar() {
  // The tap checks the existence of the snackbar and also closes it.
  NSString* snackbar_label = l10n_util::GetNSString(
      IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_SIGN_OUT_SNACKBAR_MESSAGE);
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(snackbar_label)]
      performAction:grey_tap()];
}

// Adds a bookmark. The storage type is determined based on if the user is
// signed in or not.
void SaveBookmark(NSString* title, NSString* url) {
  bookmarks::StorageType storageType = bookmarks::StorageType::kAccount;
  if ([SigninEarlGreyAppInterface isSignedOut]) {
    storageType = bookmarks::StorageType::kLocalOrSyncable;
  }
  [BookmarkEarlGrey addBookmarkWithTitle:title URL:url inStorage:storageType];
}

// Expects a batch upload recommendation item on the current screen with
// `message_id` string formatted for `count` local items and `email` user email
// id.
void ExpectBatchUploadRecommendationItem(int message_id,
                                         int count,
                                         NSString* email) {
  NSString* text = base::SysUTF16ToNSString(
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(message_id), "count", count, "email",
          base::SysNSStringToUTF16(email)));
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kBatchUploadRecommendationItemAccessibilityIdentifier),
                     grey_accessibilityLabel(text), grey_sufficientlyVisible(),
                     nil)] assertWithMatcher:grey_notNil()];
}

// Waits for snackbar item to show up after pressing save on the batch upload
// page.
void ExpectBatchUploadConfirmationSnackbar(int count, NSString* email) {
  NSString* text = base::SysUTF16ToNSString(
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(IDS_IOS_BULK_UPLOAD_SNACKBAR_MESSAGE),
          "count", count, "email", base::SysNSStringToUTF16(email)));
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:grey_accessibilityLabel(
                                                       text)];
}

}  // namespace

// Integration tests using the Google services settings screen.
@interface ManageSyncSettingsTestCase : WebHttpServerChromeTestCase
@end

@implementation ManageSyncSettingsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  if ([self
          isRunningTest:@selector(testSignoutWhileManageSyncSettingsOpened)]) {
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  } else {
    config.features_enabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
    config.features_enabled.push_back(syncer::kSyncEnableBatchUploadLocalData);
  }
  if ([self
          isRunningTest:@selector(testDeCouplingOfAddressAndPaymentToggles)]) {
    config.features_enabled.push_back(
        syncer::kSyncDecoupleAddressPaymentSettings);
    config.features_enabled.push_back(
        syncer::kSyncEnableContactInfoDataTypeInTransportMode);
  }
  if ([self isRunningTest:@selector(testCouplingOfAddressAndPaymentToggles)]) {
    config.features_disabled.push_back(
        syncer::kSyncDecoupleAddressPaymentSettings);
    config.features_enabled.push_back(
        syncer::kSyncEnableContactInfoDataTypeInTransportMode);
  }
  return config;
}

// Tests that Sync settings is dismissed when the primary account is removed and
// kReplaceSyncPromosWithSignInPromos is disabled.
- (void)testSignoutWhileManageSyncSettingsOpened {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:ManageSyncSettingsButton()];
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:scrollViewMatcher]
      assertWithMatcher:grey_notNil()];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey selectElementWithMatcher:scrollViewMatcher]
      assertWithMatcher:grey_nil()];
}

// Tests that unified account settings row is showing, and the Sync row is not
// showing when kReplaceSyncPromosWithSignInPromos is enabled.
- (void)testShowingUnifiedAccountSettings_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];

  // Sign in with fake identity using the settings sign-in promo.
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/YES);

  // Verify the Sync settings row is not showing.
  [SigninEarlGrey verifySyncUIIsHidden];

  // Verify the account settings row is showing.
  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      assertWithMatcher:grey_notNil()];
}

// Tests sign out from the unified account settings page when
// kReplaceSyncPromosWithSignInPromos is enabled.
- (void)testSignOutFromUnifiedAccountSettings_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];

  // Open the "manage sync" view.
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  SignOutFromAccountSettings();
  DismissSignOutSnackbar();
  [ChromeEarlGreyUI waitForAppToIdle];

  [SigninEarlGrey verifySignedOut];

  // Verify the "manage sync" view is popped.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Verify the account settings row is not showing in the settings menu.
  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests sign out from the manage accounts on device page when
// kReplaceSyncPromosWithSignInPromos is enabled.
- (void)testSignOutFromManageAccountsSettings_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];

  // Open the "manage sync" view.
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Scroll to the bottom to view the "manage accounts" button.
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:scrollViewMatcher]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Tap the "manage accounts on This Device" button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_MANAGE_ACCOUNTS_ITEM))]
      performAction:grey_tap()];

  // The "manage accounts" view should be shown, tap on "Sign Out" button on
  // that page.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE))]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedOut];

  // Verify the "manage accounts" view is popped.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSettingsAccountsTableViewId)]
      assertWithMatcher:grey_notVisible()];

  // Verify the "manage sync" view is popped.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Verify the account settings row is not showing in the settings menu.
  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests the unsynced data dialog shows when there are unsynced readinglist
// entries. Also verifies that the user is still signed in when the dialog
// Cancel button is tapped. kReplaceSyncPromosWithSignInPromos is enabled.
- (void)
    testUnsyncedDataDialogShowsInCaseOfUnsyncedReadingListEntry_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  [ChromeEarlGreyAppInterface disconnectFakeSyncServerNetwork];

  reading_list_test_utils::AddURLToReadingList(GURL("https://example.com"));

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncReadingListIdentifier,
                                          /*is_toggled_on=*/YES,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];

  SignOutFromAccountSettings();
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_CANCEL)] performAction:grey_tap()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Re-connect the fake sync server to the network to be able to sign-in when
  // the test gets run repeatedly.
  [ChromeEarlGreyAppInterface connectFakeSyncServerNetwork];
}

// Tests the unsynced data dialog shows when there are unsynced bookmarks. Also
// verifies that the user still signed in when the dialog Cancel button is
// tapped. kReplaceSyncPromosWithSignInPromos is enabled.
- (void)
    testCancelSigningOutFromUnsyncedDataDialogInCaseOfUnsyncedBookmarks_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncBookmarksIdentifier,
                                          /*is_toggled_on=*/YES,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [ChromeEarlGreyAppInterface disconnectFakeSyncServerNetwork];

  SaveBookmark(@"foo", @"https://www.foo.com");

  SignOutFromAccountSettings();
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_CANCEL)] performAction:grey_tap()];

  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  // Re-connect the fake sync server to the network to be able to sign-in when
  // the test gets run repeatedly.
  [ChromeEarlGreyAppInterface connectFakeSyncServerNetwork];
}

// Tests the unsynced data dialog shows when there are unsynced bookmarks. Also
// verifies that the user is signed out when the dialog Delete and Sign Out
// button is tapped. kReplaceSyncPromosWithSignInPromos is enabled.
- (void)
    testDeleteAndSignOutFromUnsyncedDataDialogInCaseOfUnsyncedBookmarks_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncBookmarksIdentifier,
                                          /*is_toggled_on=*/YES,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [ChromeEarlGreyAppInterface disconnectFakeSyncServerNetwork];

  SaveBookmark(@"foo", @"https://www.foo.com");

  SignOutFromAccountSettings();
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_AND_DELETE_BUTTON)]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedOut];

  // Re-connect the fake sync server to the network to be able to sign-in when
  // the test gets run repeatedly.
  [ChromeEarlGreyAppInterface connectFakeSyncServerNetwork];
}

// Tests that data type settings carry over signing out.
- (void)testDataTypeSettingsCarryOverSignOut_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];

  // Open the "manage sync" view.
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Change one of the toggles; say turn off Passwords.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncPasswordsIdentifier)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(/*on=*/NO)];

  SignOutFromAccountSettings();
  DismissSignOutSnackbar();

  [SigninEarlGrey verifySignedOut];

  // Sign back in with the same identity using the settings sign-in promo.
  // The history sync opt-in was accepted in the first sign-in earlier in this
  // test.
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/NO);

  // Verify the account settings row is showing in the settings menu.
  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Verify the account settings did not change with a signout; Passwords is
  // off.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncPasswordsIdentifier,
                                          /*is_toggled_on=*/NO,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that data type settings do not carry over from one user to another.
- (void)
    testDataTypeSettingsDoNotCarryOverDifferentAccounts_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Change one of the toggles; say turn off Passwords.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncPasswordsIdentifier)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(/*on=*/NO)];

  SignOutFromAccountSettings();
  DismissSignOutSnackbar();

  [SigninEarlGrey verifySignedOut];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::SettingsDoneButton(),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Sign in with another identity.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity2];
  [ChromeEarlGreyUI openSettingsMenu];

  // Verify the account settings row is showing in the settings menu.
  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Verify the account settings have the default value; Passwords is on.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncPasswordsIdentifier,
                                          /*is_toggled_on=*/YES,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that removing account from device clears the data type settings.
- (void)testDataTypeSettingsAreClearedOnAccountRemoval_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Change one of the toggles; say turn off Passwords.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncPasswordsIdentifier)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(/*on=*/NO)];

  // Remove fakeIdentity from device.
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];

  [[EarlGrey selectElementWithMatcher:SettingsSignInRowMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [SigninEarlGrey verifySignedOut];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::SettingsDoneButton(),
                                   grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Sign in with the same identity.
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];

  // Verify the account settings row is showing in the settings menu.
  [[EarlGrey selectElementWithMatcher:SettingsAccountButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Verify the account settings are cleared and have the default value;
  // Passwords is on.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncPasswordsIdentifier,
                                          /*is_toggled_on=*/YES,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the account settings is reflecting the SyncTypesListDisabled
// policy.
- (void)testAccountSettingsWithSyncTypesListDisabled_SyncToSigninEnabled {
  base::Value::List list;
  list.Append("passwords");
  policy_test_utils::SetPolicy(base::Value(std::move(list)),
                               policy::key::kSyncTypesListDisabled);
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Verify that for Passwords an "Off" button is shown instead of a toggle.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   l10n_util::GetNSString(IDS_IOS_SETTING_OFF))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the account settings is reflecting the SyncDisabled policy.
- (void)testAccountSettingsWithSyncDisabled_SyncToSigninEnabled {
  policy_test_utils::SetPolicy(true, policy::key::kSyncDisabled);
  [ChromeEarlGreyUI waitForAppToIdle];

  // Dismiss the sync disabled popup.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                                IDS_IOS_SYNC_SYNC_DISABLED_CONTINUE)),
                            grey_userInteractionEnabled(), nil)]
      performAction:grey_tap()];

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/NO);
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Scroll to the bottom to view all section.
  id<GREYMatcher> scroll_view_matcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:scroll_view_matcher]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Verify that the advanced settings items are not shown.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_MANAGE_SYNC_ENCRYPTION))]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                   IDS_IOS_MANAGE_DATA_IN_YOUR_ACCOUNT_TITLE))]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_MANAGE_SYNC_GOOGLE_ACTIVITY_CONTROLS_TITLE))]
      assertWithMatcher:grey_notVisible()];
}

// Tests the account settings is with a user actionable error; enter
// passphrase error.
- (void)testAccountSettingsWithError_SyncToSigninEnabled {
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Verify the error section is showing.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_BUTTON))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap "Enter Passphrase" button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_BUTTON))]
      performAction:grey_tap()];

  // Enter the passphrase.
  [SigninEarlGreyUI submitSyncPassphrase:kPassphrase];

  // Verify it goes back to "manage sync" UI.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify the error section is not showing anymore.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_BUTTON))]
      assertWithMatcher:grey_notVisible()];
}

// Tests the "History and Tabs" toggle manages both types. When both types
// are disabled by policy their toggle should be off.
- (void)
    testAccountSettingsWithHistoryAndTabsDisabledByPolicy_SyncToSigninEnabled {
  base::Value::List list;
  list.Append("typedUrls");
  list.Append("tabs");
  policy_test_utils::SetPolicy(base::Value(std::move(list)),
                               policy::key::kSyncTypesListDisabled);
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/NO);
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Verify that for "History and Tabs" an "Off" button is shown instead of a
  // toggle.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityLabel(
                                   l10n_util::GetNSString(IDS_IOS_SETTING_OFF))]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the "History and Tabs" toggle manages both types. When History
// is only disabled by policy their toggle should be active.
- (void)testAccountSettingsWithHistoryDisabledByPolicy_SyncToSigninEnabled {
  base::Value::List list;
  list.Append("typedUrls");
  policy_test_utils::SetPolicy(base::Value(std::move(list)),
                               policy::key::kSyncTypesListDisabled);
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/NO);
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Verify that for "History and Tabs" a toggle shows.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncHistoryAndTabsIdentifier,
                                          /*is_toggled_on=*/NO,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the "History and Tabs" toggle manages both types. When Tabs
// is only disabled by policy their toggle should be active.
- (void)testAccountSettingsWithTabsDisabledByPolicy_SyncToSigninEnabled {
  base::Value::List list;
  list.Append("tabs");
  policy_test_utils::SetPolicy(base::Value(std::move(list)),
                               policy::key::kSyncTypesListDisabled);
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/NO);
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Verify that for "History and Tabs" a toggle shows.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncHistoryAndTabsIdentifier,
                                          /*is_toggled_on=*/NO,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests closing the account settings with a remote signout.
- (void)testAccountSettingsWithRemoteSignout_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Remove fakeIdentity from device.
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey verifySignedOut];

  // Check that Settings is presented.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];

  // Verify the error section is not showing.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_BUTTON))]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the batch upload button description in the account settings
// contains the correct string for passwords.
- (void)testBulkUploadDescriptionTextForPasswords {
  // Add local data.
  password_manager_test_utils::SavePasswordForm(@"password1", @"user1",
                                                @"https://example1.com");
  password_manager_test_utils::SavePasswordForm(@"password2", @"user2",
                                                @"https://example2.com");

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  // Sign in with fake identity using the settings sign-in promo.
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/YES);

  // Open the "manage sync" view.
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Find and match the batch upload recommendation item text.
  ExpectBatchUploadRecommendationItem(
      IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_BATCH_UPLOAD_PASSWORDS_ITEM, 2,
      fakeIdentity.userEmail);
}

// Tests that the batch upload button description in the account settings
// contains the correct string for bookmarks.
- (void)testBulkUploadDescriptionTextForBookmarks {
  // Add local data.
  SaveBookmark(@"foo", @"https://www.foo.com");
  SaveBookmark(@"bar", @"https://www.bar.com");

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  // Sign in with fake identity using the settings sign-in promo.
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/YES);

  // Open the "manage sync" view.
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Find and match the batch upload recommendation item text.
  ExpectBatchUploadRecommendationItem(
      IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_BATCH_UPLOAD_ITEMS_ITEM, 2,
      fakeIdentity.userEmail);
}

// Tests that the batch upload button description in the account settings
// contains the correct string for reading list.
- (void)testBulkUploadDescriptionTextForReadingList {
  // Add local data.
  reading_list_test_utils::AddURLToReadingList(GURL("https://example.com"));

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  // Sign in with fake identity using the settings sign-in promo.
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/YES);

  // Open the "manage sync" view.
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Find and match the batch upload recommendation item text.
  ExpectBatchUploadRecommendationItem(
      IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_BATCH_UPLOAD_ITEMS_ITEM, 1,
      fakeIdentity.userEmail);
}

// Tests that the batch upload button description in the account settings
// contains the correct string for passwords and other data type.
- (void)testBulkUploadDescriptionTextForPasswordsAndOthers {
  // Add local data.
  password_manager_test_utils::SavePasswordForm(@"password", @"user",
                                                @"https://example.com");
  reading_list_test_utils::AddURLToReadingList(GURL("https://example.com"));
  SaveBookmark(@"foo", @"https://www.foo.com");

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  // Sign in with fake identity using the settings sign-in promo.
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/YES);

  // Open the "manage sync" view.
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Find and match the batch upload recommendation item text.
  ExpectBatchUploadRecommendationItem(
      IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_BATCH_UPLOAD_PASSWORDS_AND_ITEMS_ITEM, 1,
      fakeIdentity.userEmail);
}

// Tests that the batch upload page contains the correct listed data types:
// - Passwords
// - Bookmarks
// - Reading list
- (void)testBulkUploadPageForAllDataTypes {
  // Add local data.
  password_manager_test_utils::SavePasswordForm(@"password", @"user",
                                                @"https://example.com");
  reading_list_test_utils::AddURLToReadingList(GURL("https://example.com"));
  SaveBookmark(@"foo", @"https://www.foo.com");

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  // Sign in with fake identity using the settings sign-in promo.
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/YES);

  // Open the "manage sync" view.
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Tap on the batch upload button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBatchUploadAccessibilityIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify the bulk upload view is popped.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBulkUploadTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that only rows for the correct data types exist.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBulkUploadTableViewPasswordsItemAccessibilityIdentifer)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBulkUploadTableViewBookmarksItemAccessibilityIdentifer)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBulkUploadTableViewReadingListItemAccessibilityIdentifer)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the batch upload page contains the correct listed data types:
// - Passwords
- (void)testBulkUploadPageForPasswordsOnly {
  // Add local data.
  password_manager_test_utils::SavePasswordForm(@"password", @"user",
                                                @"https://example.com");

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  // Sign in with fake identity using the settings sign-in promo.
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/YES);

  // Open the "manage sync" view.
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Tap on the batch upload button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBatchUploadAccessibilityIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify the bulk upload view is popped.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBulkUploadTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that only rows for the correct data types exist.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBulkUploadTableViewPasswordsItemAccessibilityIdentifer)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBulkUploadTableViewBookmarksItemAccessibilityIdentifer)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBulkUploadTableViewReadingListItemAccessibilityIdentifer)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the batch upload page contains the correct listed data types:
// - Passwords
// - Bookmarks
- (void)testBulkUploadPageForPasswordsAndBookmarks {
  // Add local data.
  password_manager_test_utils::SavePasswordForm(@"password", @"user",
                                                @"https://example.com");
  SaveBookmark(@"foo", @"https://www.foo.com");

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  // Sign in with fake identity using the settings sign-in promo.
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/YES);

  // Open the "manage sync" view.
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Tap on the batch upload button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBatchUploadAccessibilityIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify the bulk upload view is popped.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBulkUploadTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that only rows for the correct data types exist.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBulkUploadTableViewPasswordsItemAccessibilityIdentifer)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBulkUploadTableViewBookmarksItemAccessibilityIdentifer)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBulkUploadTableViewReadingListItemAccessibilityIdentifer)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that bulk upload moves the following data types to account:
// - Passwords
- (void)testBulkUploadForPasswords {
  // Add local data.
  password_manager_test_utils::SavePasswordForm(@"password", @"user",
                                                @"https://example.com");
  reading_list_test_utils::AddURLToReadingList(GURL("https://example.com"));
  SaveBookmark(@"foo", @"https://www.foo.com");

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  // Sign in with fake identity using the settings sign-in promo.
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/YES);

  // Open the "manage sync" view.
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Tap on the batch upload button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBatchUploadAccessibilityIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify the bulk upload view is popped.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBulkUploadTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that switches for Passwords is ON.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::TableViewSwitchCell(
                     kBulkUploadTableViewPasswordsItemAccessibilityIdentifer,
                     YES)] assertWithMatcher:grey_sufficientlyVisible()];
  // Turn switches for Bookmarks and Reading List to OFF.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBulkUploadTableViewBookmarksItemAccessibilityIdentifer)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBulkUploadTableViewReadingListItemAccessibilityIdentifer)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Mock reauth since passwords needs upload.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  // Delay the auth result to be able to validate that the passwords are not
  // visible until the result is emitted.
  [PasswordSettingsAppInterface
      mockReauthenticationModuleShouldReturnSynchronously:NO];

  // Tap on the save button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBulkUploadSaveButtonAccessibilityIdentifer)]
      performAction:grey_tap()];

  // Verify the "manage sync" view is not visible yet.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Successful auth should remove blocking view and "manage sync" view should
  // be visible.
  [PasswordSettingsAppInterface mockReauthenticationModuleReturnMockedResult];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier)];

  // Remove mock to keep the app in the same state as before running the test.
  [PasswordSettingsAppInterface removeMockReauthenticationModule];

  // Ensure the correct snackbar appears.
  ExpectBatchUploadConfirmationSnackbar(1, fakeIdentity.userEmail);

  [ChromeEarlGreyUI waitForAppToIdle];

  // Ensure that the batch upload dialog section has been modified on the
  // account settings page.
  ExpectBatchUploadRecommendationItem(
      IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_BATCH_UPLOAD_ITEMS_ITEM, 2,
      fakeIdentity.userEmail);

  // TODO(crbug.com/1482823): Test that items were actually moved.
}

// Tests that bulk upload moves the following data types to account:
// - Bookmarks
// - Reading List
- (void)testBulkUploadForBookmarksAndReadingList {
  // Add local data.
  password_manager_test_utils::SavePasswordForm(@"password", @"user",
                                                @"https://example.com");
  reading_list_test_utils::AddURLToReadingList(GURL("https://example.com"));
  SaveBookmark(@"foo", @"https://www.foo.com");

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  // Sign in with fake identity using the settings sign-in promo.
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/YES);

  // Open the "manage sync" view.
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Tap on the batch upload button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBatchUploadAccessibilityIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify the bulk upload view is popped.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBulkUploadTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that switches for Bookmarks and Reading List are ON.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::TableViewSwitchCell(
                     kBulkUploadTableViewBookmarksItemAccessibilityIdentifer,
                     YES)] assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::TableViewSwitchCell(
                     kBulkUploadTableViewReadingListItemAccessibilityIdentifer,
                     YES)] assertWithMatcher:grey_sufficientlyVisible()];
  // Turn switch for Passwords to OFF.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBulkUploadTableViewPasswordsItemAccessibilityIdentifer)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Tap on the save button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBulkUploadSaveButtonAccessibilityIdentifer)]
      performAction:grey_tap()];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier)];
  // Ensure the correct snackbar appears.
  ExpectBatchUploadConfirmationSnackbar(2, fakeIdentity.userEmail);

  [ChromeEarlGreyUI waitForAppToIdle];

  // Ensure that the batch upload dialog section has been modified on the
  // account settings page.
  ExpectBatchUploadRecommendationItem(
      IDS_IOS_GOOGLE_ACCOUNT_SETTINGS_BATCH_UPLOAD_PASSWORDS_ITEM, 1,
      fakeIdentity.userEmail);

  // TODO(crbug.com/1482823): Test that items were actually moved.
}

// Tests that bulk upload moves the following data types to account:
// - Passwords
// - Bookmarks
// - Reading List
- (void)testBulkUploadForAllDataTypes {
  // Add local data.
  password_manager_test_utils::SavePasswordForm(@"password", @"user",
                                                @"https://example.com");
  reading_list_test_utils::AddURLToReadingList(GURL("https://example.com"));
  SaveBookmark(@"foo", @"https://www.foo.com");

  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  // Sign in with fake identity using the settings sign-in promo.
  SignInWithPromoFromAccountSettings(fakeIdentity, /*expect_history_sync=*/YES);

  // Open the "manage sync" view.
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Tap on the batch upload button.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBatchUploadAccessibilityIdentifier)]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];

  // Verify the bulk upload view is popped.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBulkUploadTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Verify that switches for all data types are ON.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::TableViewSwitchCell(
                     kBulkUploadTableViewPasswordsItemAccessibilityIdentifer,
                     YES)] assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::TableViewSwitchCell(
                     kBulkUploadTableViewBookmarksItemAccessibilityIdentifer,
                     YES)] assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::TableViewSwitchCell(
                     kBulkUploadTableViewReadingListItemAccessibilityIdentifer,
                     YES)] assertWithMatcher:grey_sufficientlyVisible()];

  // Mock reauth since passwords needs upload.
  [PasswordSettingsAppInterface setUpMockReauthenticationModule];
  [PasswordSettingsAppInterface mockReauthenticationModuleExpectedResult:
                                    ReauthenticationResult::kSuccess];
  // Delay the auth result to be able to validate that the passwords are not
  // visible until the result is emitted.
  [PasswordSettingsAppInterface
      mockReauthenticationModuleShouldReturnSynchronously:NO];

  // Tap on the save button.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kBulkUploadSaveButtonAccessibilityIdentifer)]
      performAction:grey_tap()];

  // Verify the "manage sync" view is not visible yet.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // Successful auth should remove blocking view and "manage sync" view should
  // be visible.
  [PasswordSettingsAppInterface mockReauthenticationModuleReturnMockedResult];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:
          grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier)];

  // Remove mock to keep the app in the same state as before running the test.
  [PasswordSettingsAppInterface removeMockReauthenticationModule];

  // Ensure the correct snackbar appears.
  ExpectBatchUploadConfirmationSnackbar(3, fakeIdentity.userEmail);

  [ChromeEarlGreyUI waitForAppToIdle];

  // Ensure that the batch upload dialog section does not exist anymore.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kBatchUploadRecommendationItemAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kBatchUploadAccessibilityIdentifier)]
      assertWithMatcher:grey_notVisible()];

  // TODO(crbug.com/1482823): Test that items were actually moved.
}

// Runs only when `kSyncDecoupleAddressPaymentSettings` is disabled. Tests that
// the payments and address toggles are coupled together.
// TODO(crbug.com/1435431): Remove the test once
// `kSyncDecoupleAddressPaymentSettings` gets launched.
- (void)testCouplingOfAddressAndPaymentToggles {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Toggle off the address.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncAutofillIdentifier)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(/*on=*/NO)];

  // Verify that the Payments is not enabled.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncPaymentsIdentifier,
                                          /*is_toggled_on=*/NO,
                                          /*enabled=*/NO)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Toggle on the address.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncAutofillIdentifier)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(/*on=*/YES)];

  // Verify that the Payments is enabled.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncPaymentsIdentifier,
                                          /*is_toggled_on=*/YES,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Runs only when `kSyncDecoupleAddressPaymentSettings` is enabled. Tests that
// the payments and address toggles have been decoupled.
- (void)testDeCouplingOfAddressAndPaymentToggles {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Toggle off the address.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kSyncAutofillIdentifier)]
      performAction:chrome_test_util::TurnTableViewSwitchOn(/*on=*/NO)];

  // Verify that the Payments is still enabled.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TableViewSwitchCell(
                                          kSyncPaymentsIdentifier,
                                          /*is_toggled_on=*/YES,
                                          /*enabled=*/YES)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the account settings and the user actionable error view are dismissed
// on account removal.
- (void)testAccountSettingsWithErrorDismissed_SyncToSigninEnabled {
  [ChromeEarlGrey addBookmarkWithSyncPassphrase:kPassphrase];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Verify the error section is showing.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_BUTTON))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap "Enter Passphrase" button.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_TABLE_ERROR_ENTER_PASSPHRASE_BUTTON))]
      performAction:grey_tap()];

  // Remove fakeIdentity from device.
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];

  // Check that user is signed out and back to Settings main view.
  [SigninEarlGrey verifySignedOut];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];
}

// Tests the account settings and the encryption view are dismissed
// on account removal.
- (void)testAccountSettingsAndEncryptionDismissed_SyncToSigninEnabled {
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  // Scroll to the bottom to view all section.
  id<GREYMatcher> scroll_view_matcher =
      grey_accessibilityID(kManageSyncTableViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:scroll_view_matcher]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  // Verify the encryption item is shown.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEncryptionAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on the encryption item.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kEncryptionAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Remove fakeIdentity from device.
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey forgetFakeIdentity:fakeIdentity];

  // Check that user is signed out and back to Settings main view.
  [ChromeEarlGreyUI waitForAppToIdle];
  [SigninEarlGrey verifySignedOut];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];
}

@end
