// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

// Accessibility identifier for the Data from Chrome Sync cell.
extern NSString* const kDataFromChromeSyncAccessibilityIdentifier;

// Accessibility identifier for Manage Sync table view.
extern NSString* const kManageSyncTableViewAccessibilityIdentifier;

// Accessibility identifiers for sync switch items.
extern NSString* const kSyncEverythingItemAccessibilityIdentifier;
extern NSString* const kSyncBookmarksIdentifier;
extern NSString* const kSyncOmniboxHistoryIdentifier;
extern NSString* const kSyncPasswordsIdentifier;
extern NSString* const kSyncOpenTabsIdentifier;
extern NSString* const kSyncAutofillIdentifier;
extern NSString* const kSyncPreferencesIdentifier;
extern NSString* const kSyncReadingListIdentifier;

// Accessibility identifier for Encryption item.
extern NSString* const kEncryptionAccessibilityIdentifier;

// Sections used in Sync Settings page.
typedef NS_ENUM(NSInteger, SyncSettingsSectionIdentifier) {
  // Section for all the sync settings.
  SyncDataTypeSectionIdentifier = kSectionIdentifierEnumZero,
  // Sign out options.
  SignOutSectionIdentifier,
  // Advanced settings.
  AdvancedSettingsSectionIdentifier,
  // Sync errors.
  SyncErrorsSectionIdentifier,
  // Section to show the signed-in account.
  AccountSectionIdentifier,
};

// Item types used per Sync Setting section.
// Types are used to uniquely identify SyncSwitchItem items.
typedef NS_ENUM(NSInteger, SyncSettingsItemType) {
  // SyncDataTypeSectionIdentifier section.
  // Sync everything item.
  SyncEverythingItemType = kItemTypeEnumZero,
  // kSyncAutofill.
  AutofillDataTypeItemType,
  // kSyncBookmarks.
  BookmarksDataTypeItemType,
  // kSyncOmniboxHistory.
  HistoryDataTypeItemType,
  // kSyncOpenTabs.
  OpenTabsDataTypeItemType,
  // kSyncPasswords
  PasswordsDataTypeItemType,
  // kSyncReadingList.
  ReadingListDataTypeItemType,
  // kSyncPreferences.
  SettingsDataTypeItemType,
  // Item for kAutofillWalletImportEnabled.
  AutocompleteWalletItemType,
  // Sign out and turn off sync item,
  SignOutAndTurnOffSyncItemType,
  // Sign out item,
  SignOutItemType,
  // AdvancedSettingsSectionIdentifier section.
  // Encryption item.
  EncryptionItemType,
  // Google activity controls item.
  GoogleActivityControlsItemType,
  // Data from Chrome sync.
  DataFromChromeSync,
  // Sync errors.
  PrimaryAccountReauthErrorItemType,
  ShowPassphraseDialogErrorItemType,
  SyncNeedsTrustedVaultKeyErrorItemType,
  SyncTrustedVaultRecoverabilityDegradedErrorItemType,
  SyncDisabledByAdministratorErrorItemType,
  // Sign out item footer.
  SignOutItemFooterType,
  // Item for the header and the footer of the types list.
  TypesListHeaderOrFooterType,
  // Item for the signed in identity.
  IdentityAccountItemType,
};

// States for Sync Settings page to be in.
enum class SyncSettingsAccountState {
  // The user clicked "settings" in the Sync opt-in screen.
  kAdvancedInitialSyncSetup,
  // The user is viewing sync settings page when Sync-the-feature is on.
  kSyncing,
  // The user is viewing sync settings page when signed-in not syncing.
  kSignedIn,
  // The user signed out from the sync settings page, and the UI is being
  // dismissed.
  kSignedOut,
};

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_MANAGE_SYNC_SETTINGS_CONSTANTS_H_
