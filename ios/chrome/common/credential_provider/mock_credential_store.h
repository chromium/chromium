// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_MOCK_CREDENTIAL_STORE_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_MOCK_CREDENTIAL_STORE_H_

#import "ios/chrome/common/credential_provider/credential_store.h"

// Mock implementation of CredentialStore for unit tests.
@interface MockCredentialStore : NSObject <CredentialStore>

- (instancetype)initWithCredentials:(NSArray<id<Credential>>*)credentials;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_MOCK_CREDENTIAL_STORE_H_
