// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_STORE_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_STORE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/common/credential_provider/memory_credential_store.h"

// Credential store built on top of keyed archiver and unarchiver to persist
// data. Use `saveDataWithCompletion:` to update the data on disk. All
// operations will be held in memory until saved to disk, making it possible to
// batch multiple operations.
@interface ArchivableCredentialStore : MemoryCredentialStore

// Initializes the store. `fileURL` is where the store should live in disk. If
// the file doesn't exist, it will be created on first save. If the file was not
// created by this class the store won't be able to load it and the behavior
// will be unexpected. In general `fileURL` can be a temp file for testing, or a
// shared resource path in order to be used in an extension.
- (instancetype)initWithFileURL:(NSURL*)fileURL NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_ARCHIVABLE_CREDENTIAL_STORE_H_
