// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

@protocol ActorTaskUpdatesObserver;
@protocol ActorTaskInterventionDelegate;
@class GeminiActuationRequest;
@class GeminiActuationResponse;

// Protocol for the Gemini actor delegate. This protocol is implemented by a
// Gemini actuation handler passed downstream to enable it to communicate with
// the upstream ActorService.
@protocol GeminiActuationDelegate <NSObject>

// TODO(crbug.com/501043031): Remove @optional when API stabilizes.
@optional

// Creates a new task with the given title.
- (actor::ActorTaskId)createTaskWithTitle:(NSString*)title;

// Dispatches an actuation request for `taskID`.
//
// High-Level Flow:
// - Downstream callers dispatch a `GeminiActuationRequest` (such as executing
//   actions or yielding for user intervention).
// - For long-lived or asynchronous operations, the `completionBlock` is held in
//   memory per-task so that lifecycle events (such as external task stops, user
//   cancellation, or session disconnect) can immediately respond to and unblock
//   the model.
// - Responds with a `GeminiActuationResponse` containing execution results,
//   user feedback, or appropriate error status codes.
- (void)dispatchActuationRequest:(GeminiActuationRequest*)request
                       forTaskID:(actor::ActorTaskId)taskID
                 completionBlock:(void (^)(GeminiActuationResponse* response))
                                     completionBlock;

// Asynchronously register an updates observer (1-to-N).
- (void)addTaskUpdatesObserver:(id<ActorTaskUpdatesObserver>)observer
                     forTaskID:(actor::ActorTaskId)taskID;

// Asynchronously register an intervention delegate (1-to-1).
- (void)setTaskInterventionDelegate:(id<ActorTaskInterventionDelegate>)delegate
                          forTaskID:(actor::ActorTaskId)taskID;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_DELEGATE_H_
