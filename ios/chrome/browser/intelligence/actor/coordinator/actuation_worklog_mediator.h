// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/public/actor_task_intervention_delegate.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_mutator.h"

namespace actor {
class ActorService;
}  // namespace actor

@protocol ActuationWorklogConsumer;

// Translates `ActorTask` execution updates into displayable timeline items and
// action chips for an `ActuationWorklogConsumer`.
@interface ActuationWorklogMediator : NSObject <ActorTaskInterventionDelegate,
                                                ActorTaskUpdatesObserver,
                                                ActuationWorklogMutator>

// The consumer that receives formatted worklog updates.
@property(nonatomic, weak) id<ActuationWorklogConsumer> consumer;

// Designated initializer with task update service to observe.
- (instancetype)initWithActorService:(actor::ActorService*)actorService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Starts displaying the updates of the task identified by `taskID`, replacing
// any previously observed task.
- (void)startObservingTaskWithID:(actor::ActorTaskId)taskID;

// Stops observing the current task and resets the worklog. No-op if no task is
// observed.
- (void)stopObservingTask;

// Disconnects the mediator and cleans up references.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_MEDIATOR_H_
