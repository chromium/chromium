// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_COORDINATOR_AGE_MISMATCH_SIGNOUT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_COORDINATOR_AGE_MISMATCH_SIGNOUT_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol AgeMismatchSignoutConsumer;
@protocol SystemIdentity;

namespace signin {
class AvatarProvider;
class IdentityManager;
}

// Mediator for the Age Mismatch prompt.
@interface AgeMismatchSignoutMediator : NSObject

- (instancetype)initWithIdentity:(id<SystemIdentity>)identity
          identityAvatarProvider:(signin::AvatarProvider*)identityAvatarProvider
                 identityManager:(signin::IdentityManager*)identityManager
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer for this mediator.
@property(nonatomic, weak) id<AgeMismatchSignoutConsumer> consumer;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_SIGNOUT_COORDINATOR_AGE_MISMATCH_SIGNOUT_MEDIATOR_H_
