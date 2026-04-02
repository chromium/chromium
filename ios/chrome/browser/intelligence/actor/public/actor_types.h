// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TYPES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TYPES_H_

#import "base/functional/callback_forward.h"
#import "base/token.h"
#import "base/types/strong_alias.h"

namespace actor {

// Strongly typed, performant unique token representing an ActorTask.
using ActorTaskId = base::StrongAlias<class ActorTaskIdMarker, base::Token>;

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
