// Copyright 2020 The Chromium Authors. All rights reserved.
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

// Key for the app group user defaults containing the metadata for credentials
// created in the extension.
NSString* AppGroupUserDefaultsCredentialProviderNewCredentials();

// An array of deprecated keys to be removed if present.
NSArray<NSString*>* UnusedUserDefaultsCredentialProviderKeys();

// Key for the app group user defaults indicating if the credentials have been
// synced with iOS via AuthenticationServices.
extern NSString* const
    kUserDefaultsCredentialProviderASIdentityStoreSyncCompleted;

// Key for the app group user defaults indicating if the credentials have been
// sync for the first time. The defaults contain a Bool indicating if the first
// time sync have been completed. This value might change to force credentials
// to be sync once Chrome is updated.
extern NSString* const kUserDefaultsCredentialProviderFirstTimeSyncCompleted;

// Key for the app group user defaults indicating if the user has enabled and
// given consent for the credential provider extension.
extern NSString* const kUserDefaultsCredentialProviderConsentVerified;

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CONSTANTS_H_
