// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CONSTANTS_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Path to the persisted file for the credential provider archivable store.
NSURL* CredentialProviderSharedArchivableStoreURL();

// Key for the app group user defaults containing the user ID, which can be
// validated in the extension.
NSString* AppGroupUserDefaultsCredentialProviderUserID();

// Key for the app group user defaults containing the current user email.
NSString* AppGroupUserDefaultsCredentialProviderUserEmail();

// Key for the app group user defaults containing the metadata for credentials
// created in the extension.
NSString* AppGroupUserDefaultsCredentialProviderNewCredentials();

// Key for the app group user defaults containing whether saving passwords is
// currently enabled.
NSString* AppGroupUserDefaulsCredentialProviderSavingPasswordsEnabled();

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
