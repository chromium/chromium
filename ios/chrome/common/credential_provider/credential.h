// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_H_

#import <Foundation/Foundation.h>

// Contains the data for a Credential that can be used with iOS AutoFill.
// Implementations must provide hash and equality methods.
@protocol Credential <NSObject>

// Associated favicon name. Used by passwords and passkeys.
@property(nonatomic, readonly) NSString* favicon;

// Identifier to use with ASCredentialIdentityStore. Used by passwords and
// passkeys.
@property(nonatomic, readonly) NSString* recordIdentifier;

// Username of the service. Used by passwords and passkeys.
@property(nonatomic, readonly) NSString* username;

// Plain text password. Used by passwords only.
@property(nonatomic, readonly) NSString* password;

// Importance ranking of this credential. Used by passwords only.
@property(nonatomic, readonly) int64_t rank;

// Service identifier of this credential. Should match
// ASCredentialServiceIdentifier. Used by passwords only.
@property(nonatomic, readonly) NSString* serviceIdentifier;

// Human readable name of the associated service. Used by passwords only.
@property(nonatomic, readonly) NSString* serviceName;

// Attached note to the credential. Used by passwords only.
@property(nonatomic, readonly) NSString* note;

// Passkey sync id (hex encoded). Used by passkeys only.
@property(nonatomic, readonly) NSString* syncId;

// Passkey user display name. Used by passkeys only.
@property(nonatomic, readonly) NSString* userDisplayName;

// Passkey user id (hex encoded). Used by passkeys only.
@property(nonatomic, readonly) NSString* userId;

// Passkey credential id (hex encoded). Used by passkeys only.
@property(nonatomic, readonly) NSString* credentialId;

// Passkey rp id. Used by passkeys only.
@property(nonatomic, readonly) NSString* rpId;

// Passkey private key (hex encoded). Used by passkeys only.
@property(nonatomic, readonly) NSString* privateKey;

// Passkey encrypted (hex encoded). Used by passkeys only.
@property(nonatomic, readonly) NSString* encrypted;

// Passkey creation time. Used by passkeys only.
@property(nonatomic, readonly) int64_t creationTime;

// Whether the credential is a passkey.
- (BOOL)isPasskey;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_H_
