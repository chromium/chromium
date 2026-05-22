// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_MEDIATOR_H_

#import <Foundation/Foundation.h>

class AuthenticationService;
@protocol LevelUpConsumer;
@protocol LevelUpProfileConsumer;

// Mediator for the Level Up feature.
@interface LevelUpMediator : NSObject

// The consumer for this mediator.
@property(nonatomic, weak) id<LevelUpConsumer> consumer;
// The consumer for user profile credentials updates.
@property(nonatomic, weak) id<LevelUpProfileConsumer> profileConsumer;

// Initializes this mediator with the authentication service.
- (instancetype)initWithAuthenticationService:
    (AuthenticationService*)authService NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_MEDIATOR_H_
