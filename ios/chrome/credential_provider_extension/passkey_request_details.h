// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_REQUEST_DETAILS_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_REQUEST_DETAILS_H_

#import <Foundation/Foundation.h>

@class ASPasskeyCredentialRequestParameters;
@protocol ASCredentialRequest;

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

// Hash of client data for credential provider to sign as part of the operation.
@property(nonatomic, readonly) NSData* clientDataHash;

// A preference for whether the authenticator should attempt to verify that it
// is being used by its owner.
@property(nonatomic, readonly) BOOL userVerificationRequired;

// The relying party identifier for this request.
@property(nonatomic, readonly) NSString* relyingPartyIdentifier;

// A list of allowed credential IDs for this request. An empty list means all
// credentials are allowed.
@property(nonatomic, readonly) NSArray<NSData*>* allowedCredentials;

// Whether at least one signing algorithm is supported by the relying party.
// Unused by assertion requests.
@property(nonatomic, readonly) BOOL algorithmIsSupported;

// The user name of the passkey credential.
@property(nonatomic, readonly) NSString* userName;

// The user handle of the passkey credential.
@property(nonatomic, readonly) NSData* userHandle;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_PASSKEY_REQUEST_DETAILS_H_
