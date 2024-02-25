// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_IDENTITY_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_IDENTITY_H_

#import <Foundation/Foundation.h>

// Protocol representing a single identity as known to the system.
// A user may have multiple identities associated with a single device.
@protocol SystemIdentity <NSObject>

// The unique GAIA user identifier for this identity. Can be used as a
// unique and stable identifier to remember a particular identity.
@property(nonatomic, readonly) NSString* gaiaID;

// The identity email address. This can be shown to the user, but is
// not a unique identifier.
@property(nonatomic, readonly) NSString* userEmail;

// The full name of the identity. May be nil if no full name has been
// fetched for this account yet.
@property(nonatomic, readonly) NSString* userFullName;

// The primary given name of the identity, usually the user's first name.
// May be nil if no name has been fetched for this account yet.
@property(nonatomic, readonly) NSString* userGivenName;

// Cached hashed Gaia ID. Used to pass the currently signed in identity
// between application.
@property(nonatomic, readonly) NSString* hashedGaiaID;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_SYSTEM_IDENTITY_H_
