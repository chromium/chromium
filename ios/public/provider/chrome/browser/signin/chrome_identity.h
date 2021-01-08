// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_H_

#import <Foundation/Foundation.h>

// A single identity used for signing in.
// Identity is the equivalent of an account. A user may have multiple identities
// or accounts associated with a single device.
@interface ChromeIdentity : NSObject

// Identity/account email address. This can be shown to the user, but is not a
// unique identifier (@see gaiaID).
@property(strong, nonatomic, readonly) NSString* userEmail;

// The unique GAIA user identifier for this identity/account.
// You may use this as a unique identifier to remember a particular identity.
@property(strong, nonatomic, readonly) NSString* gaiaID;

// Returns the full name of the identity.
// Could be nil if no full name has been fetched for this account yet.
@property(strong, nonatomic, readonly) NSString* userFullName;

// Returns the primary given name of the identity, usually the user's first
// name. Could be nil if no name has been fetched for this account yet.
@property(strong, nonatomic, readonly) NSString* userGivenName;

// Cached Hashed Gaia ID. This is used to pass the currently signed in account
// between apps.
@property(strong, nonatomic, readonly) NSString* hashedGaiaID;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_H_
