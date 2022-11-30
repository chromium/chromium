// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_MULTI_STORE_CREDENTIAL_STORE_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_MULTI_STORE_CREDENTIAL_STORE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/common/credential_provider/credential_store.h"

@protocol Credential;

// Store that can ingest credentials from more than one. This store doesn't
// support saving data to disk.
@interface MultiStoreCredentialStore : NSObject <CredentialStore>

// Initializes the store. `stores` are used as data providers. If 2, or more,
// credentials share the same unique identifier, the first stores will take
// precedence.
- (instancetype)initWithStores:(NSArray<id<CredentialStore>>*)stores
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_MULTI_STORE_CREDENTIAL_STORE_H_
