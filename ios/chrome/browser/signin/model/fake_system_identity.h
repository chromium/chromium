// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_H_

#include <string>

#include "ios/chrome/browser/signin/model/system_identity.h"

// Two fake identities are equal if and only if their `gaiaId` and
// `refreshToken` are equal.
// TODO(crbug.com/517249368): Change the implementation of system identities and
// fake system identities so that the equality only considers `gaiaId`.
@interface FakeSystemIdentity : NSObject <NSSecureCoding, SystemIdentity>

// Encodes `identities` into a string, using NSKeyedArchiver.
+ (std::string)encodeIdentitiesToBase64:
    (NSArray<FakeSystemIdentity*>*)identities;

// Returns a list of FakeSystemIdentity encoded using
// `encodeIdentitiesToBase64:`.
+ (NSArray<FakeSystemIdentity*>*)identitiesFromBase64String:
    (const std::string&)string;

// Returns a fake identity.
+ (instancetype)fakeIdentity1;

// Returns a second fake identity.
+ (instancetype)fakeIdentity2;

// Returns a third fake identity.
+ (instancetype)fakeIdentity3;

// Returns a forth fake identity.
+ (instancetype)fakeIdentity4;

// Returns a fake managed identity.
+ (instancetype)fakeManagedIdentity;

// Returns a SystemIdentity based on `email` with `name@example.com`.
// For simplicity, both `userGivenName` and `userFullName` properties use
// `name` from the email address. And GaiaID will be derived from `email`.
+ (instancetype)identityWithEmail:(NSString*)email;

// Returns a SystemIdentity based on `email` with `name@example.com`.
// For simplicity, both `userGivenName` and `userFullName` properties use
// `name` from the email address.
+ (instancetype)identityWithEmail:(NSString*)email gaiaID:(const GaiaId&)gaiaID;

// Returns a fake identity with given name as nil.
+ (instancetype)fakeIdentityWithMissingGivenName;

// Returns a fake identity with both names as nil.
+ (instancetype)fakeIdentityWithMissingNames;

- (instancetype)init NS_UNAVAILABLE;

// Redeclared as readwrite.
@property(nonatomic, readwrite) NSString* userEmail;
@property(nonatomic, readwrite) NSString* userFullName;
@property(nonatomic, readwrite) NSString* userGivenName;
@property(nonatomic, readwrite) BOOL hasValidAuth;
// This value is not used by Chrome. However, it’s used for the sake of
// `isEqual:` in GCRSSOIdentity internal code. Setting a value to this token
// allows to simulate having two identities that are not equal, while still
// having all relevant values equal.
// TODO(crbug.com/517249368): Remove this property when the internal
// implementation doesn’t use it either.
@property(nonatomic, readwrite, copy) NSString* refreshToken;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_H_
