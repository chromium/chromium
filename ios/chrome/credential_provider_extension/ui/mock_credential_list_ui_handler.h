// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_MOCK_CREDENTIAL_LIST_UI_HANDLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_MOCK_CREDENTIAL_LIST_UI_HANDLER_H_

#import "ios/chrome/credential_provider_extension/ui/credential_list_ui_handler.h"

// Mock implementation of CredentialListUIHandler for unit tests.
@interface MockCredentialListUIHandler : NSObject <CredentialListUIHandler>

@property(nonatomic, readonly) NSArray<NSData*>* allowedCredentials;

@property(nonatomic, readonly) BOOL isRequestingPasskey;

- (instancetype)initWithAllowedCredentials:(NSArray<NSData*>*)allowedCredentials
                       isRequestingPasskey:(BOOL)isRequestingPasskey;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_MOCK_CREDENTIAL_LIST_UI_HANDLER_H_
