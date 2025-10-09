// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CONSTANTS_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Path to the persisted file for the credential provider archivable store.
NSURL* CredentialProviderSharedArchivableStoreURL();

// Key for the app group user defaults containing the managed user ID, which can
// be validated in the extension.
// This is non-nil iff the user's account is managed (e.g. by an enterprise).
NSString* AppGroupUserDefaultsCredentialProviderManagedUserID();

// Key for the app group user defaults containing the current user ID.
NSString* AppGroupUserDefaultsCredentialProviderUserID();

// Key for the app group user defaults containing whether multiple profiles are
// currently in use.
NSString* AppGroupUserDefaultsCredentialProviderMultiProfileSetting();

// Key for the app group user defaults containing the current user email.
NSString* AppGroupUserDefaultsCredentialProviderUserEmail();

// Key for the app group user defaults containing the metadata for credentials
// created in the extension.
NSString* AppGroupUserDefaultsCredentialProviderNewCredentials();

// Key for the app group user defaults containing whether saving passwords and
// passkeys is currently enabled.
NSString* AppGroupUserDefaultsCredentialProviderSavingPasswordsEnabled();

// Key for the app group user defaults containing whether saving passwords is
// currently managed by an enterprise policy.
NSString* AppGroupUserDefaultsCredentialProviderSavingPasswordsManaged();

// Key for the app group user defaults indicating whether saving passkeys is
// allowed by enterprise policy. Even if this is set to `YES`, passkey creation
// could still be blocked by `...CredentialProviderSavingPasswordsEnabled`
// above. This pref is only configurable by enterprise policy, not by users, so
// if this is set to `NO` then it is because the behavior is managed.
NSString* AppGroupUserDefaultsCredentialProviderSavingPasskeysEnabled();

// Key for the app group user defaults containing whether syncing passwords is
// currently enabled.
NSString* AppGroupUserDefaultsCredentialProviderPasswordSyncSetting();

// Key for the app group user defaults containing whether automatic passkey
// upgrade is currently enabled.
NSString* AppGroupUserDefaulsCredentialProviderAutomaticPasskeyUpgradeEnabled();

// Key for the app group user defaults containing whether passkey PRF support is
// currently enabled.
NSString* AppGroupUserDefaulsCredentialProviderPasskeyPRFEnabled();

// Key for the app group user defaults containing whether passkey Large Blob
// support is currently enabled.
NSString* AppGroupUserDefaulsCredentialProviderPasskeyLargeBlobEnabled();

// Key for the app group user defaults containing whether signal API is
// currently enabled.
NSString* AppGroupUserDefaulsCredentialProviderSignalAPIEnabled();

// Key for the app group user defaults containing whether the button order in
// the confirmation alerts should be swapped.
NSString*
AppGroupUserDefaulsCredentialProviderConfirmationButtonSwapOrderEnabled();

// Key for the app group user defaults indicating if the credentials have been
// synced with iOS via AuthenticationServices.
extern NSString* const
    kUserDefaultsCredentialProviderASIdentityStoreSyncCompleted;

// Key for the app group user defaults indicating if the credentials have been
// sync for the first time. The defaults contain a Bool indicating if the first
// time sync have been completed. This value might change to force credentials
// to be sync once Chrome is updated.
extern NSString* const kUserDefaultsCredentialProviderFirstTimeSyncCompleted;

// Values of the UMA IOS.CredentialExtension.PasswordCreated. Must be kept up to
// date with IOSCredentialProviderPasswordCreated in enums.xml. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class CPEPasswordCreated {
  kPasswordManuallyEntered = 0,
  kPasswordSuggested = 1,
  kPasswordSuggestedAndChanged = 2,
  kMaxValue = kPasswordSuggestedAndChanged,
};

// Values of the UMA IOS.CredentialExtension.NewCredentialUsername. Must be kept
// up to date with IOSCredentialProviderNewCredentialUsername in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CPENewCredentialUsername {
  kCredentialWithUsername = 0,
  kCredentialWithoutUsername = 1,
  kMaxValue = kCredentialWithoutUsername,
};

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CONSTANTS_H_
