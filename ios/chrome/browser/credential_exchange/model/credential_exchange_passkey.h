// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_EXCHANGE_PASSKEY_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_EXCHANGE_PASSKEY_H_

#import <Foundation/Foundation.h>

// Intermediate data model for a passkey credential, used to translate between
// password manager's C++ representations of it and the Swift struct used by the
// OS Credential Exchange library.
// (https://fidoalliance.org/specs/cx/cxf-v1.0-ps-20250814.html#dict-passkey)
@interface CredentialExchangePasskey : NSObject

// Probabilistically-unique identifier of a credential.
// (https://www.w3.org/TR/webauthn-2/#credential-id)
@property(nonatomic, copy) NSData* credentialId;

// Relying party identifier.
// (https://www.w3.org/TR/webauthn-2/#rp-id)
@property(nonatomic, copy) NSString* rpId;

// Human-readable account identifier.
// (https://www.w3.org/TR/webauthn-2/#dom-publickeycredentialentity-name)
@property(nonatomic, copy) NSString* userName;

// Human-readable name for the account, used for display.
// (https://www.w3.org/TR/webauthn-2/#dom-publickeycredentialuserentity-displayname)
@property(nonatomic, copy) NSString* userDisplayName;

// User identifier. Also called user handle in context of WebAuthn.
// (https://www.w3.org/TR/webauthn-2/#user-handle)
@property(nonatomic, copy) NSData* userId;

// Private key of a passkey, unencrypted.
@property(nonatomic, copy) NSData* privateKey;

- (instancetype)initWithCredentialId:(NSData*)credentialId
                                rpId:(NSString*)rpId
                            userName:(NSString*)userName
                     userDisplayName:(NSString*)userDisplayName
                              userId:(NSData*)userId
                          privateKey:(NSData*)privateKey
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_EXCHANGE_MODEL_CREDENTIAL_EXCHANGE_PASSKEY_H_
