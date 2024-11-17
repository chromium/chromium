// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_RESPONSE_HANDLER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_RESPONSE_HANDLER_H_

#import <AuthenticationServices/AuthenticationServices.h>

#import <vector>

@protocol Credential;

typedef void (^FetchSecurityDomainSecretCompletionBlock)(
    NSArray<NSData*>* security_domain_secrets);

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
    userVerificationRequired:(BOOL)userVerificationRequired;

- (void)userCancelledRequestWithErrorCode:(ASExtensionErrorCode)errorCode;

- (void)completeExtensionConfigurationRequest;

// Returns the gaia for the account used for credential creation.
- (NSString*)gaia;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_RESPONSE_HANDLER_H_
