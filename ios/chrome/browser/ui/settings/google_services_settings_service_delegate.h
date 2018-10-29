// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_SERVICE_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_SERVICE_DELEGATE_H_

// List of Google Services Settings commands.
typedef NS_ENUM(NSInteger, GoogleServicesSettingsCommandID) {
  // Does nothing.
  GoogleServicesSettingsCommandIDNoOp,

  // Restarts the sign-in authentication flow. Related to error:
  // SyncSetupService::kSyncServiceUnrecoverableError.
  GoogleServicesSettingsCommandIDRestartAuthenticationFlow,
  // Opens the reauth sync dialog. Related to error:
  // SyncSetupService::kSyncServiceNeedsPassphrase.
  GoogleServicesSettingsReauthDialogAsSyncIsInAuthError,
  // Opens the passphrase dialog. Related to error:
  // SyncSetupService::kSyncServiceNeedsPassphrase.
  GoogleServicesSettingsCommandIDShowPassphraseDialog,

  // Enabble/disable all the Google services.
  GoogleServicesSettingsCommandIDToggleSyncEverything,

  // Personalized section.
  // Enable/disabble bookmark sync.
  GoogleServicesSettingsCommandIDToggleDataTypeSync,
  // Enable/disable autocomplete wallet for Google Pay.
  GoogleServicesSettingsCommandIDAutocompleteWalletService,
  // Opens the Google activity controls dialog.
  GoogleServicesSettingsCommandIDOpenGoogleActivityControlsDialog,
  // Opens the encryption dialog.
  GoogleServicesSettingsCommandIDOpenEncryptionDialog,
  // Opens manage synced data web page.
  GoogleServicesSettingsCommandIDOpenManageSyncedDataWebPage,

  // Non-personalized section.
  // Enable/disabble autocomplete searches service.
  GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService,
  // Enable/disabble preload pages service.
  GoogleServicesSettingsCommandIDTogglePreloadPagesService,
  // Enable/disabble improve chrome service.
  GoogleServicesSettingsCommandIDToggleImproveChromeService,
  // Enable/disabble better search and browsing service.
  GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService,
};

// Protocol to handle Google services settings commands.
@protocol GoogleServicesSettingsServiceDelegate<NSObject>

// Called when GoogleServicesSettingsCommandIDToggleSyncEverything is triggered.
- (void)toggleSyncEverythingWithValue:(BOOL)value;

// Personalized section.
// Called when GoogleServicesSettingsCommandIDToggleDataTypeSync is triggered.
- (void)toggleSyncDataSync:(NSInteger)dataType withValue:(BOOL)value;
// Called when GoogleServicesSettingsCommandIDAutocompleteWalletService is
// triggered.
- (void)toggleAutocompleteWalletServiceWithValue:(BOOL)value;

// Non-personalized section.
// Called when GoogleServicesSettingsCommandIDToggleAutocompleteSearchesService
// is triggered.
- (void)toggleAutocompleteSearchesServiceWithValue:(BOOL)value;
// Called when GoogleServicesSettingsCommandIDTogglePreloadPagesService is
// triggered.
- (void)togglePreloadPagesServiceWithValue:(BOOL)value;
// Called when GoogleServicesSettingsCommandIDToggleImproveChromeService is
// triggered.
- (void)toggleImproveChromeServiceWithValue:(BOOL)value;
// Called when
// GoogleServicesSettingsCommandIDToggleBetterSearchAndBrowsingService is
// triggered.
- (void)toggleBetterSearchAndBrowsingServiceWithValue:(BOOL)value;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_SETTINGS_SERVICE_DELEGATE_H_
