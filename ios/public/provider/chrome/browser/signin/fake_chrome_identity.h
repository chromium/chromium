// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_H_

#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

#include <string>

// A fake ChromeIdentity used for testing.
@interface FakeChromeIdentity : ChromeIdentity <NSSecureCoding>

// Encodes `identities` into a string, using NSKeyedArchiver.
+ (std::string)encodeIdentitiesToBase64:
    (NSArray<FakeChromeIdentity*>*)identities;

// Returns a list of FakeChromeIdentity encoded using
// `encodeIdentitiesToBase64:`.
+ (NSArray<FakeChromeIdentity*>*)identitiesFromBase64String:
    (const std::string&)string;

// Returns a fake identity.
+ (FakeChromeIdentity*)fakeIdentity1;

// Returns a second fake identity.
+ (FakeChromeIdentity*)fakeIdentity2;

// Returns a fake managed identity.
+ (FakeChromeIdentity*)fakeManagedIdentity;

// Returns a ChromeIdentity based on `email`, `gaiaID` and `name`.
// The `hashedGaiaID` property will be derived from `name`.
// For simplicity, both `userGivenName` and `userFullName` properties use
// `name`.
+ (FakeChromeIdentity*)identityWithEmail:(NSString*)email
                                  gaiaID:(NSString*)gaiaID
                                    name:(NSString*)name;

// Redeclared as readwrite.
@property(strong, nonatomic, readwrite) NSString* userEmail;
@property(strong, nonatomic, readwrite) NSString* gaiaID;
@property(strong, nonatomic, readwrite) NSString* userFullName;
@property(strong, nonatomic, readwrite) NSString* userGivenName;
@property(strong, nonatomic, readwrite) NSString* hashedGaiaID;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_H_
