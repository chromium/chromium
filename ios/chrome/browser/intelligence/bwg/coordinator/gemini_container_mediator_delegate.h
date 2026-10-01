// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

@class GeminiContainerMediator;

// Delegate notified of Gemini actuation task transitions tracked by
// `GeminiContainerMediator`.
@protocol GeminiContainerMediatorDelegate <NSObject>

// Called when the Gemini actuation task identified by `taskID` starts.
- (void)geminiContainerMediator:(GeminiContainerMediator*)mediator
    didStartActuationTaskWithID:(actor::ActorTaskId)taskID;

// Called when the Gemini actuation task identified by `taskID` stops.
- (void)geminiContainerMediator:(GeminiContainerMediator*)mediator
     didStopActuationTaskWithID:(actor::ActorTaskId)taskID;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_DELEGATE_H_
