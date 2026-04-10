// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TYPES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TYPES_H_

#import "base/functional/callback_forward.h"
#import "base/types/id_type.h"

namespace actor {

// Strongly typed, performant unique ID representing an ActorTask.
using ActorTaskId = base::IdType32<class ActorTaskIdMarker>;
static_assert(ActorTaskId(0).is_null(), "0 must be a null ActorTaskId");

// Represents the high-level orchestration state of an ActorTask.
enum class ActorTaskState {
  // Task is initialized but has not started executing tools.
  kInit = 0,
  // Task is actively executing through its engine.
  kActing = 1,
  // Task is waiting for AI provider to reflect on next actions to execute.
  kReflecting = 2,
  // Task execution was paused by the actor.
  kPausedByActor = 3,
  // Task execution was paused by the user.
  kPausedByUser = 4,
  // Task execution was cancelled or aborted.
  kCancelled = 5,
  // Task successfully completed.
  kFinished = 6,
  // Task is currently waiting for input or confirmation from the user.
  kWaitingOnUser = 7,
  // Task execution encountered a terminal failure.
  kFailed = 8
};

// Reasons why an ActorTask was stopped.
enum class ActorTaskStoppedReason {
  // Task was explicitly stopped by the user.
  kStoppedByUser = 0,
  // Task successfully completed its execution.
  kTaskComplete = 1,
  // The underlying model encountered an error during the task.
  kModelError = 2,
  // A browser failure occurred.
  kBrowserFailure = 3,
  // One of the tabs executing the task was detached or destroyed.
  kTabDetached = 4,
  // System or browser is shutting down.
  kShutdown = 5,
  // User started a new chat session, aborting the current task.
  kUserStartedNewChat = 6
};

// Callback for when a tool or set of tools finishes.
using ExecuteToolsCallback = base::OnceCallback<void(ActorTaskStoppedReason)>;

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TYPES_H_
