// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_ACTOR_TASK_STORE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_ACTOR_TASK_STORE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/public/actor_task_updates_observer.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"
#import "ios/chrome/browser/intelligence/actor/public/actuation_worklog_data_source.h"

namespace actor {
class ActorService;
}  // namespace actor

// Keeps the worklog of a Gemini actor task, built from `ActorService` updates
// plus Gemini-specific steps.
@interface GeminiActorTaskStore
    : NSObject <ActorTaskUpdatesObserver, ActuationWorklogDataSource>

// ID of the observed task.
@property(nonatomic, readonly) actor::ActorTaskId taskID;

// Designated initializer. Starts observing `taskID` updates on `actorService`;
// the worklog stays empty if they can't be observed.
- (instancetype)initWithTaskID:(actor::ActorTaskId)taskID
                  actorService:(actor::ActorService*)actorService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Stops observing the task.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_ACTOR_TASK_STORE_H_
