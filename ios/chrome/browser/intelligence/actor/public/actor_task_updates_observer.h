// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_UPDATES_OBSERVER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_UPDATES_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

namespace web {
class WebStateID;
}

// The ActorTask updates observer protocol (1-to-N). Used for passive
// state broadcasts.
@protocol ActorTaskUpdatesObserver <NSObject>

// TODO(crbug.com/501043031): Remove @optional when API stabilizes.
@optional

// Fired immediately after the UI successfully registers as an observer.
// Provides the initial status so the UI knows what to draw right away.
- (void)didRegisterAsObserverForTaskID:(actor::ActorTaskId)taskID
                             taskTitle:(NSString*)taskTitle
                            taskUpdate:(NSString*)taskUpdate
                          currentState:(actor::ActorTaskState)state
                             webStates:(NSArray<NSNumber*>*)webStatesIDs;

// Called when a WebState is added to an active task.
- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didAddWebState:(web::WebStateID)webStateID;

// Called when an existing task's state changes.
- (void)actorTaskWithID:(actor::ActorTaskId)taskID
         didChangeState:(actor::ActorTaskState)newState
              fromState:(actor::ActorTaskState)oldState;

// Called just before a tool is about to be executed.
- (void)actorTaskWithID:(actor::ActorTaskId)taskID
        // TODO(crbug.com/514742306): Remove proto dependency.
        willExecuteTool:(optimization_guide::proto::Action::ActionCase)toolCase
             taskUpdate:(NSString*)taskUpdate
             onWebState:(web::WebStateID)webStateID;

// Called when a task stops.
- (void)actorTaskDidStopWithID:(actor::ActorTaskId)taskID
                    finalState:(actor::ActorTaskState)finalState;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_UPDATES_OBSERVER_H_
