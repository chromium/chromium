// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_MANAGER_STORAGE_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_MANAGER_STORAGE_H_

#import <Foundation/Foundation.h>

@protocol SystemIdentity;
@class FakeSystemIdentityDetails;

// Stores identities.  The key is the identity's gaia id, and identities
// are not stored directly, but a wrapper object is used, allowing to
// store extra information about the identity.
//
// This class remembers the insertion order as unit tests depends on the
// order being stable and deterministic.
@interface FakeSystemIdentityManagerStorage : NSObject <NSFastEnumeration>

// Returns whether `identity` is contained in the store.
- (BOOL)containsIdentity:(id<SystemIdentity>)identity;

// Returns details for `identity` or nil if the identity is unknown.
- (FakeSystemIdentityDetails*)detailsForIdentity:(id<SystemIdentity>)identity;

// Adds `identity`. Does nothing if `identity` is already stored.
- (void)addIdentity:(id<SystemIdentity>)identity;

// Removes `identity`. Does nothing if `identity` is not stored.
- (void)removeIdentity:(id<SystemIdentity>)identity;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_MANAGER_STORAGE_H_
