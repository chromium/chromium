// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_H_

#include "ios/chrome/browser/signin/model/system_identity.h"

#include <string>

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

// Returns a fake managed identity.
+ (instancetype)fakeManagedIdentity;

// Returns a SystemIdentity based on `email`, `gaiaID` and `name`.
// The `hashedGaiaID` property will be derived from `name`.
// For simplicity, both `userGivenName` and `userFullName` properties use
// `name`.
+ (instancetype)identityWithEmail:(NSString*)email
                           gaiaID:(NSString*)gaiaID
                             name:(NSString*)name;

// Returns a SystemIdentity based on `name` and `domain`. All other
// values will be derived from those.
+ (instancetype)identityWithName:(NSString*)name domain:(NSString*)domain;

// Redeclared as readwrite.
@property(strong, nonatomic, readwrite) NSString* userEmail;
@property(strong, nonatomic, readwrite) NSString* gaiaID;
@property(strong, nonatomic, readwrite) NSString* userFullName;
@property(strong, nonatomic, readwrite) NSString* userGivenName;
@property(strong, nonatomic, readwrite) NSString* hashedGaiaID;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_H_
