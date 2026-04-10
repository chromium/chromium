// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_DELEGATE_H_

#import <Foundation/Foundation.h>

#import <vector>

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

@protocol ActorTaskUpdatesObserver;
@protocol ActorTaskInterventionDelegate;

// Protocol for the Gemini actor delegate. This protocol is implemented by a
// Gemini actuation handler passed downstream to enable it to communicate with
// the upstream ActorService.
@protocol GeminiActuationDelegate <NSObject>

// Creates a new task with the given title.
- (actor::ActorTaskId)createTaskWithTitle:(NSString*)title;

// Asynchronously register an updates observer (1-to-N).
- (void)addTaskUpdatesObserver:(id<ActorTaskUpdatesObserver>)observer
                     forTaskID:(actor::ActorTaskId)taskID;

// Asynchronously register an intervention delegate (1-to-1).
- (void)setTaskInterventionDelegate:(id<ActorTaskInterventionDelegate>)delegate
                          forTaskID:(actor::ActorTaskId)taskID;

// Request to perform actions.
- (void)performActionsWithTaskID:(actor::ActorTaskId)taskID
                      taskUpdate:(NSString*)taskUpdate
          serializedActionProtos:(NSArray<NSData*>*)serializedActionProtos
                      completion:
                          (void (^)(BOOL success,
                                    std::vector<bool> results))completionBlock;

// Request to pause the task.
- (void)pauseTaskWithID:(actor::ActorTaskId)taskID;

// Request to stop the task.
- (void)stopTaskWithID:(actor::ActorTaskId)taskID
                reason:(actor::ActorTaskStoppedReason)reason;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_DELEGATE_H_
