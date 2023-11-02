// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_USER_DEFAULTS_CREDENTIAL_STORE_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_USER_DEFAULTS_CREDENTIAL_STORE_H_

#import "ios/chrome/common/credential_provider/memory_credential_store.h"

// Credential store built on top of NSUserDefaults to persist data. Use
// `saveDataWithCompletion:` to update the data on disk. All operations will be
// held in memory until saved to disk, making it possible to batch multiple
// operations.
//
// Only supports `Credentials` of class `ArchivableCredential`.
@interface UserDefaultsCredentialStore
    : MemoryCredentialStore <MutableCredentialStore>

// Initializes the store. `userDefaults` is where the store will be persisted.
- (instancetype)initWithUserDefaults:(NSUserDefaults*)userDefaults
                                 key:(NSString*)key NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_USER_DEFAULTS_CREDENTIAL_STORE_H_
