// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_REQUEST_DETAILS_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_REQUEST_DETAILS_H_

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

@protocol Credential;

// This class represents a passkey credential request (attestation or
// registration).
@interface PasskeyRequestDetails : NSObject

- (instancetype)initWithParameters:(ASPasskeyCredentialRequestParameters*)
                                       passkeyCredentialRequestParameters
    isBiometricAuthenticationEnabled:(BOOL)isBiometricAuthenticationEnabled
    API_AVAILABLE(ios(17.0));

- (instancetype)initWithRequest:(id<ASCredentialRequest>)credentialRequest
    isBiometricAuthenticationEnabled:(BOOL)isBiometricAuthenticationEnabled
    API_AVAILABLE(ios(17.0));

- (instancetype)init NS_UNAVAILABLE;

// Performs passkey creation and returns the new credential.
- (ASPasskeyRegistrationCredential*)createPasskeyForGaia:(NSString*)gaia
                                   securityDomainSecrets:
                                       (NSArray<NSData*>*)securityDomainSecrets
    API_AVAILABLE(ios(17.0));

// Performs passkey assertion and returns the assertion response.
- (ASPasskeyAssertionCredential*)
    assertPasskeyCredential:(id<Credential>)credential
      securityDomainSecrets:(NSArray<NSData*>*)securityDomainSecrets
    API_AVAILABLE(ios(17.0));

// Returns whether the list of credentials contains a password of the same
// domain and username as the passkey request.
- (BOOL)hasMatchingPassword:(NSArray<id<Credential>>*)credentials;

// Returns whether a passkey from the excluded passkeys list is both in the
// credentials list and is for the same rpId as the current request.
- (BOOL)hasExcludedPasskey:(NSArray<id<Credential>>*)credentials;

// A preference for whether the authenticator should attempt to verify that it
// is being used by its owner.
@property(nonatomic, readonly) BOOL userVerificationRequired;

// The relying party identifier for this request.
@property(nonatomic, readonly) NSString* relyingPartyIdentifier;

// The user name for this request.
@property(nonatomic, readonly) NSString* userName;

// A list of allowed credential IDs for this request. An empty list means all
// credentials are allowed.
@property(nonatomic, readonly) NSArray<NSData*>* allowedCredentials;

// Whether at least one signing algorithm is supported by the relying party.
// Unused by assertion requests.
@property(nonatomic, readonly) BOOL algorithmIsSupported;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_REQUEST_DETAILS_H_
