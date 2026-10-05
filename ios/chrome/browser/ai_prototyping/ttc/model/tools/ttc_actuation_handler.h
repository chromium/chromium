// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_H_

#import <Foundation/Foundation.h>

#import "components/sessions/core/session_id.h"
#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

class ProfileIOS;
class WebStateList;

namespace actor {
class ActorService;
}  // namespace actor

@class TTCActuationRequest;
@class TTCActuationResponse;

// The handler for TTC actuations, bridging incoming actuation
// requests to the Chromium Actor Service orchestration layer.
@interface TTCActuationHandler : NSObject

// Designated initializer with Profile, WebStateList, and Browser ID.
// @param profile The Profile associated with the browser session.
// @param webStateList The WebStateList associated with the browser.
// @param browserID The session ID representing the browser window.
// @return An initialized instance.
- (instancetype)initWithProfile:(ProfileIOS*)profile
                   webStateList:(WebStateList*)webStateList
                      browserID:(SessionID)browserID NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects active references, cancels in-flight tasks with `kShutdown`, and
// fails pending actuation callbacks with `kExecutorDestroyed`. Must be called
// before destruction.
- (void)disconnect;

// Creates a new Actor task titled `title` associated with the currently active
// WebState in the browser, and binds that WebState to the task.
// @param title The human-readable title describing the task.
// @return The allocated `ActorTaskId`, or a null `ActorTaskId` if there is no
// active WebState, if the active WebState is off-the-record, or if ActorService
// is unavailable.
- (actor::ActorTaskId)createTaskWithTitle:(NSString*)title;

// Dispatches `request` for execution under `taskID`. The underlying Actor task
// remains active to allow subsequent actuation requests until explicitly
// stopped or disconnected.
// The `completionBlock` is guaranteed to be invoked on the UI thread.
// @param request The actuation request containing action protos.
// @param taskID The ID of the task to execute actions within.
// @param completionBlock Invoked with the execution response.
- (void)dispatchActuationRequest:(TTCActuationRequest*)request
                       forTaskID:(actor::ActorTaskId)taskID
                 completionBlock:
                     (void (^)(TTCActuationResponse* response))completionBlock;

// Stops the task identified by `taskID` with the specified `reason` and
// releases its tab binding. If an actuation request is currently in progress
// for `taskID`, its completion block is invoked with `kTaskWentAway`.
// @param taskID The ID of the task to stop.
// @param reason The reason the task is being stopped.
- (void)stopTask:(actor::ActorTaskId)taskID
      withReason:(actor::ActorTaskStoppedReason)reason;

// Stops the task identified by `taskID` with `kTaskComplete` reason.
// @param taskID The ID of the task to stop.
- (void)stopTask:(actor::ActorTaskId)taskID;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_HANDLER_H_
