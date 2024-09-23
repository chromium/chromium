// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_MOCK_CREDENTIAL_RESPONSE_HANDLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_MOCK_CREDENTIAL_RESPONSE_HANDLER_H_

#import "ios/chrome/credential_provider_extension/ui/credential_response_handler.h"

// Mock implementation of CredentialResponseHandler for unit tests.
@interface MockCredentialResponseHandler : NSObject <CredentialResponseHandler>

@property(nonatomic, strong) ASPasswordCredential* passwordCredential;

@property(nonatomic, strong)
    ASPasskeyAssertionCredential* passkeyCredential API_AVAILABLE(ios(17.0));

@property(nonatomic, strong) void (^receivedCredentialBlock)();

@property(nonatomic, strong) void (^receivedPasskeyBlock)();

@property(nonatomic, assign) ASExtensionErrorCode errorCode;

@property(nonatomic, strong) void (^receivedErrorCodeBlock)();

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_MOCK_CREDENTIAL_RESPONSE_HANDLER_H_
