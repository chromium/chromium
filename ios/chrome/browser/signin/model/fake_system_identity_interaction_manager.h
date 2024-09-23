// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_INTERACTION_MANAGER_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_INTERACTION_MANAGER_H_

#import <Foundation/Foundation.h>

#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/signin/model/system_identity_interaction_manager.h"

class FakeSystemIdentityManager;

@interface FakeSystemIdentityInteractionManager
    : NSObject <SystemIdentityInteractionManager>

// Stores the identity and whether capabilities should not be set (i.e. unknown
// capabilities) to use when sign-in tap is simulated. The identity must be set
// before calling `-simulateDidTapAddAccount`.
+ (void)setIdentity:(id<SystemIdentity>)identity
    withUnknownCapabilities:(BOOL)usingUnknownCapabilities;

- (instancetype)initWithManager:
    (base::WeakPtr<FakeSystemIdentityManager>)manager NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Simulates the user tapping the sign-in button.
- (void)simulateDidTapAddAccount;

// Simulates the user tapping the cancel button.
- (void)simulateDidTapCancel;

// Simulates the user encountering an error not handled by SystemIdentity.
- (void)simulateDidThrowUnhandledError;

// Simulates the auth activity being interrupted.
- (void)simulateDidInterrupt;

// Returns whether the activity view is presented.
@property(nonatomic, readonly) BOOL isActivityViewPresented;

// The user email passed on the last call to
// `startAuthActivityWithViewController:userEmail:completion:`.
@property(nonatomic, strong, readonly) NSString* lastStartAuthActivityUserEmail;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_SYSTEM_IDENTITY_INTERACTION_MANAGER_H_
