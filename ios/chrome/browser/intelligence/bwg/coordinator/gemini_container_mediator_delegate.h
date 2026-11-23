// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

@class GeminiContainerMediator;
@protocol ActorTaskActionHandler;
@protocol ActorTaskDataSource;

// Delegate for `GeminiContainerMediator`, handling container presentation and
// Gemini actuation task transitions.
@protocol GeminiContainerMediatorDelegate <NSObject>

// Called when the mediator has finished preparing and is ready for the
// container to be presented.
- (void)geminiContainerMediatorReadyForPresentation:
    (GeminiContainerMediator*)mediator;

// Called when the Gemini actuation task identified by `taskID` starts.
// `dataSource` publishes its worklog and `actionHandler` handles the user
// actions on it until the task stops.
- (void)geminiContainerMediator:(GeminiContainerMediator*)mediator
    didStartActuationTaskWithID:(actor::ActorTaskId)taskID
                     dataSource:(id<ActorTaskDataSource>)dataSource
                  actionHandler:(id<ActorTaskActionHandler>)actionHandler;

// Called when the Gemini actuation task identified by `taskID` stops.
- (void)geminiContainerMediator:(GeminiContainerMediator*)mediator
     didStopActuationTaskWithID:(actor::ActorTaskId)taskID;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_DELEGATE_H_
