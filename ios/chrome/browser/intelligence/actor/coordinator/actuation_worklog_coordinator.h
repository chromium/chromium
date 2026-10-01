// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_COORDINATOR_H_

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ActuationWorklogViewController;

// Coordinator managing the actuation worklog lifecycle and bridging task
// updates from `ActorService` to the worklog UI.
@interface ActuationWorklogCoordinator : ChromeCoordinator

// The view controller displaying the actuation worklog.
@property(nonatomic, readonly) ActuationWorklogViewController* viewController;

// Starts displaying the updates of the task identified by `taskID`, replacing
// any previously observed task. Must be called after `start`.
- (void)startObservingTaskWithID:(actor::ActorTaskId)taskID;

// Stops observing the current task and resets the worklog. No-op if no task is
// observed.
- (void)stopObservingTask;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_COORDINATOR_H_
