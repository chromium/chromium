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
  kUnknown = 0,
  kStoppedByUser = 1,
};

// Callback for when an action or set of actions finishes.
using PerformActionsCallback = base::OnceCallback<void(ActorTaskStoppedReason)>;

}  // namespace actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TYPES_H_
