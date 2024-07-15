// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_MOCK_CREDENTIAL_LIST_CONSUMER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_MOCK_CREDENTIAL_LIST_CONSUMER_H_

#include "ios/chrome/credential_provider_extension/ui/credential_list_consumer.h"

// Mock implementation of CredentialListConsumer for unit tests.
@interface MockCredentialListConsumer : NSObject <CredentialListConsumer>

@property(nonatomic, strong) void (^presentSuggestedCredentialsBlock)
    (NSArray<id<Credential>>*, NSArray<id<Credential>>*, BOOL, BOOL);

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_MOCK_CREDENTIAL_LIST_CONSUMER_H_
