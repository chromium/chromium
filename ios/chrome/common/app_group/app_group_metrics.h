// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_METRICS_H_
#define IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_METRICS_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/common/app_group/app_group_constants.h"

namespace app_group {

// Suffix to the name of file containing logs ready for upload.
extern NSString* const kPendingLogFileSuffix;

// Directory containing the logs produced by extensions that are ready for
// upload.
extern NSString* const kPendingLogFileDirectory;

// An app_group key to the number of times Search Extension was displayed since
// last Chrome launch.
extern NSString* const kSearchExtensionDisplayCount;

// An app_group key to the number of times Content Extension was displayed since
// last Chrome launch.
extern NSString* const kContentExtensionDisplayCount;

// An app_group key to the number of times Credential Extension was displayed
// since last Chrome launch.
extern NSString* const kCredentialExtensionDisplayCount;

// An app_group key to the number of times Credential Extension needed
// user reauthentication since last Chrome launch.
extern NSString* const kCredentialExtensionReauthCount;

// An app_group key to the number of times Credential Extension user
// copied a URL since last Chrome launch.
extern NSString* const kCredentialExtensionCopyURLCount;

// An app_group key to the number of times Credential Extension user
// copied a Username since last Chrome launch.
extern NSString* const kCredentialExtensionCopyUsernameCount;

// An app_group key to the number of times Credential Extension user
// copied a User Display Name since last Chrome launch.
extern NSString* const kCredentialExtensionCopyUserDisplayNameCount;

// An app_group key to the number of times Credential Extension user
// copied a Creation Date since last Chrome launch.
extern NSString* const kCredentialExtensionCopyCreationDateCount;

// An app_group key to the number of times Credential Extension user
// copied a Password since last Chrome launch.
extern NSString* const kCredentialExtensionCopyPasswordCount;

// An app_group key to the number of times Credential Extension user
// unobfuscated a Password since last Chrome launch.
extern NSString* const kCredentialExtensionShowPasswordCount;

// An app_group key to the number of times Credential Extension user
// searched for a Password since last Chrome launch.
extern NSString* const kCredentialExtensionSearchCount;

// An app_group key to the number of times Credential Extension user
// selected a Password from the list since last Chrome launch.
extern NSString* const kCredentialExtensionPasswordUseCount;

// An app_group key to the number of times Credential Extension user
// selected a Passkey from the list since last Chrome launch.
extern NSString* const kCredentialExtensionPasskeyUseCount;

// An app_group key to the number of times Credential Extension returned
// a Password to the context without direct user intervention.
extern NSString* const kCredentialExtensionQuickPasswordUseCount;

// An app_group key to the number of times Credential Extension returned
// a Passkey to the context without direct user intervention.
extern NSString* const kCredentialExtensionQuickPasskeyUseCount;

// An app_group key to the number of times Credential Extension couldn't
// find a password in the keychain.
extern NSString* const kCredentialExtensionFetchPasswordFailureCount;

// An app_group key to the number of times Credential Extension was queried
// for a password with a nil argument.
extern NSString* const kCredentialExtensionFetchPasswordNilArgumentCount;

// An app_group key for the number of times saving a newly generated password
// to the keychain failed.
extern NSString* const kCredentialExtensionKeychainSavePasswordFailureCount;

// An app_group key for the number of times saving a new credential failed.
extern NSString* const kCredentialExtensionSaveCredentialFailureCount;

// Returns the app_group key containing the number of times the given histogram
// bucket was fired.
NSString* HistogramCountKey(NSString* histogram, int bucket);

// Offsets the sessionID to avoid collision. The sessionID is limited to 1<<23.
int AppGroupSessionID(int sessionID, AppGroupApplications application);

}  // namespace app_group

#endif  // IOS_CHROME_COMMON_APP_GROUP_APP_GROUP_METRICS_H_
