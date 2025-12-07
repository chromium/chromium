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

// User's account identifier. Used by passwords and passkeys.
@property(nonatomic, readonly) NSString* gaia;

// Username of the service. Used by passwords and passkeys.
@property(nonatomic, copy) NSString* username;

// Plain text password. Used by passwords only.
@property(nonatomic, readonly) NSString* password;

// Importance ranking of this credential. Used by passwords only.
@property(nonatomic, readonly) int64_t rank;

// Service identifier of this credential. Should match
// ASCredentialServiceIdentifier. Used by passwords only.
@property(nonatomic, readonly) NSString* serviceIdentifier;

// Human readable name of the associated service. Used by passwords only.
@property(nonatomic, readonly) NSString* serviceName;

// eTLD+1 of the `serviceIdentifier`. Used by passwords only. It is only
// populated by the browser, hence it might be nil until the next credential
// migration runs in the browser.
@property(nonatomic, readonly) NSString* registryControlledDomain;

// Attached note to the credential. Used by passwords only.
@property(nonatomic, readonly) NSString* note;

// Passkey sync id. Used by passkeys only.
@property(nonatomic, readonly) NSData* syncId;

// Passkey user display name. Used by passkeys only.
@property(nonatomic, readonly) NSString* userDisplayName;

// Passkey user id. Used by passkeys only.
@property(nonatomic, readonly) NSData* userId;

// Passkey credential id. Used by passkeys only.
@property(nonatomic, readonly) NSData* credentialId;

// Passkey rp id. Used by passkeys only.
@property(nonatomic, readonly) NSString* rpId;

// Passkey private key. Used by passkeys only.
@property(nonatomic, readonly) NSData* privateKey;

// Passkey encrypted. Used by passkeys only.
@property(nonatomic, readonly) NSData* encrypted;

// Passkey creation time in milliseconds. Used by passkeys only.
@property(nonatomic, readonly) int64_t creationTime;

// Passkey last used time in microseconds. Used by passkeys only.
@property(nonatomic, assign) int64_t lastUsedTime;

// Whether credential should be shown to the user in authentication surfaces,
// e.g. because a site marked it as obsolete. Used by passkeys only.
@property(nonatomic, assign) BOOL hidden;

// Local time on device when the credential was `hidden`. Represented in
// milliseconds since the UNIX epoch. Used to permanently delete credentials
// after they've been hidden for a while. Used by passkeys only.
@property(nonatomic, assign) int64_t hiddenTime;

// Whether `userDisplayName` or passkey `username` was ever manually edited by
// the user. Used by passkeys only.
@property(nonatomic, readonly) BOOL editedByUser;

// Whether the credential is a passkey.
- (BOOL)isPasskey;

// Converts and returns the creation time as an NSDate.
- (NSDate*)creationDate;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_H_
