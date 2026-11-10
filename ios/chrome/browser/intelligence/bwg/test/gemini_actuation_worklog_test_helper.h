// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_TEST_GEMINI_ACTUATION_WORKLOG_TEST_HELPER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_TEST_GEMINI_ACTUATION_WORKLOG_TEST_HELPER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

@class ActuationWorklogCoordinator;
@protocol ActorTaskInterventionDelegate;

namespace actor {
class ActorService;
}  // namespace actor

// Bundles an `ActuationWorklogCoordinator` with the per-task
// `GeminiActorTaskStore` that feeds it, mirroring the production wiring in
// `GeminiContainerMediator` for integration tests.
@interface GeminiActuationWorklogTestHelper : NSObject

// The wrapped `ActuationWorklogCoordinator`.
@property(nonatomic, strong) ActuationWorklogCoordinator* coordinator;

// The intervention delegate of the currently observed task, or `nil` if no
// task is observed.
@property(nonatomic, readonly) id<ActorTaskInterventionDelegate>
    interventionDelegate;

// The ID of the current task.
@property(nonatomic, readonly) actor::ActorTaskId currentTaskId;

// Creates a `GeminiActorTaskStore` for `taskID` on `actorService` and attaches
// it to `coordinator`, replacing any previously observed task. `coordinator`
// must be started.
- (void)startObservingTaskWithID:(actor::ActorTaskId)taskID
                    actorService:(actor::ActorService*)actorService;

// Detaches the current task store from `coordinator` and disconnects it. No-op
// if no task is observed.
- (void)stopObservingTask;

// Stops observing any task and stops `coordinator`.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_TEST_GEMINI_ACTUATION_WORKLOG_TEST_HELPER_H_
