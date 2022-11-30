// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_MEMORY_CREDENTIAL_STORE_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_MEMORY_CREDENTIAL_STORE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/common/credential_provider/credential_store.h"

@class ArchivableCredential;

// Base Credential store, memory only and meant to be subclassed.
//
// Only supports `Credentials` of class `ArchivableCredential`.
@interface MemoryCredentialStore : NSObject <MutableCredentialStore>

// The in-memory storage.
@property(nonatomic, readonly)
    NSMutableDictionary<NSString*, ArchivableCredential*>* memoryStorage;

// Working queue used to sync the mutable set operations. Meant for use when
// subclassing.
@property(nonatomic, readonly) dispatch_queue_t workingQueue;

// The first time the storage is used, this method will be called to populate
// `memoryStorage`. Meant for subclassing.
- (NSMutableDictionary<NSString*, ArchivableCredential*>*)loadStorage;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_MEMORY_CREDENTIAL_STORE_H_
