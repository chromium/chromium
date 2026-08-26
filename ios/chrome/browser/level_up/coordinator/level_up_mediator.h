// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_MEDIATOR_H_

#import <Foundation/Foundation.h>

class AuthenticationService;
@protocol LevelUpConsumer;
@class LevelUpMediator;
@protocol LevelUpProfileConsumer;
class LevelUpService;
class PrefService;
namespace signin {
class IdentityManager;
}

// Delegate for the Level Up mediator.
@protocol LevelUpMediatorDelegate <NSObject>

// Called when the mediator wants to dismiss the Level Up view.
- (void)levelUpMediatorWantsToBeDismissed:(LevelUpMediator*)mediator;

@end

// Mediator for the Level Up feature.
@interface LevelUpMediator : NSObject

// The delegate for this mediator.
@property(nonatomic, weak) id<LevelUpMediatorDelegate> delegate;
// The consumer for this mediator.
@property(nonatomic, weak) id<LevelUpConsumer> consumer;
// The consumer for user profile credentials updates.
@property(nonatomic, weak) id<LevelUpProfileConsumer> profileConsumer;

// Initializes this mediator with the authentication service, identity manager,
// level up service, and pref service.
- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authService
                  identityManager:(signin::IdentityManager*)identityManager
                   levelUpService:(LevelUpService*)levelUpService
                      prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Configures the consumer for all tasks.
- (void)configureAllTasksConsumer:(id<LevelUpConsumer>)allTasksConsumer;

// Toggles the progress updates enabled status.
- (void)toggleProgressUpdates;

// Resets task status and turns off Level Up.
- (void)turnOffLevelUp;

// Disconnects the mediator by releasing observed objects and pointers.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_COORDINATOR_LEVEL_UP_MEDIATOR_H_
