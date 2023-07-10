// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_FAKE_SYSTEM_IDENTITY_DETAILS_H_
#define IOS_CHROME_BROWSER_SIGNIN_FAKE_SYSTEM_IDENTITY_DETAILS_H_

#import <UIKit/UIKit.h>

#include <string>

#include "base/containers/flat_map.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "ios/chrome/browser/signin/capabilities_types.h"

@class FakeRefreshAccessTokenError;
@protocol SystemIdentity;

using FakeSystemIdentityCapabilitiesMap = base::flat_map<std::string, bool>;

// Helper object used by FakeSystemIdentityManager to attach state to
// a SystemIdentity object via an association.
@interface FakeSystemIdentityDetails : NSObject

// The identity.
@property(nonatomic, readonly, strong) id<SystemIdentity> identity;

// The capabilities for the associated SystemIdentity.
@property(nonatomic, readonly)
    const FakeSystemIdentityCapabilitiesMap& capabilities;

// The avatar cached for the associated SystemIdentity. May be nil.
@property(nonatomic, strong) UIImage* cachedAvatar;

// If non-nil, fetching access token for the associated SystemIdentity
// will be considered as failing, and the `error` value will be passed
// to the observers.
@property(nonatomic, strong) FakeRefreshAccessTokenError* error;

// Allows callers to modify internal capability state mappings for tests.
@property(nonatomic, readonly)
    AccountCapabilitiesTestMutator* capabilitiesMutator;

// Designated initializer.
- (instancetype)initWithIdentity:(id<SystemIdentity>)identity
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_FAKE_SYSTEM_IDENTITY_DETAILS_H_
