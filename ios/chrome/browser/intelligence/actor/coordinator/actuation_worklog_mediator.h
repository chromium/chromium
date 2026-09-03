// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"

namespace actor {
class ActorService;
}  // namespace actor

@protocol ActuationWorklogConsumer;

// Translates `ActorTask` execution updates into displayable timeline items and
// action chips for an `ActuationWorklogConsumer`.
@interface ActuationWorklogMediator : NSObject <ActorTaskUpdatesObserver>

// The consumer that receives formatted worklog updates.
@property(nonatomic, weak) id<ActuationWorklogConsumer> consumer;

// Designated initializer with task update service to observe.
- (instancetype)initWithActorService:(actor::ActorService*)actorService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Starts observing task updates from `ActorService`.
- (void)connect;

// Disconnects the mediator and cleans up references.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_MEDIATOR_H_
