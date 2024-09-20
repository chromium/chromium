// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_RESPONSE_HANDLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_RESPONSE_HANDLER_H_

#import <AuthenticationServices/AuthenticationServices.h>

@protocol Credential;

typedef void (^FetchKeyCompletionBlock)(NSData* security_domain_secret);

// A handler to allow children to communicate selected credentials back to the
// parent. This is essentially a wrapper for
// `ASCredentialProviderExtensionContext` to force all calls through the parent.
@protocol CredentialResponseHandler

- (void)userSelectedPassword:(ASPasswordCredential*)credential;

- (void)userSelectedPasskey:(ASPasskeyAssertionCredential*)credential
    API_AVAILABLE(ios(17.0));

- (void)userSelectedPasskey:(id<Credential>)passkey
             clientDataHash:(NSData*)clientDataHash
         allowedCredentials:(NSArray<NSData*>*)allowedCredentials
                 allowRetry:(BOOL)allowRetry;

- (void)userCancelledRequestWithErrorCode:(ASExtensionErrorCode)errorCode;

- (void)completeExtensionConfigurationRequest;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_RESPONSE_HANDLER_H_
