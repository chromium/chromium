// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_H_

#include <string>

#include "ios/chrome/browser/signin/model/system_identity.h"

// A fake SystemIdentity used for testing.
@interface FakeSystemIdentity : NSObject <SystemIdentity, NSSecureCoding>

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

- (instancetype)init NS_UNAVAILABLE;

// Redeclared as readwrite.
@property(nonatomic, readwrite) NSString* userEmail;
@property(nonatomic, readwrite) NSString* userFullName;
@property(nonatomic, readwrite) NSString* userGivenName;
@property(nonatomic, readwrite) BOOL hasValidAuth;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_H_
