// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_LIFECYCLE_OBSERVER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_LIFECYCLE_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

namespace actor {
struct TaskSourceInfo;
}  // namespace actor

// Observer of tasks lifecycle across the `ActorService`. Use it to discover
// tasks and filter them by provenance, then register with the task itself for
// fine-grained updates.
@protocol ActorTaskLifecycleObserver <NSObject>

// Called after a task is started.
- (void)actorServiceDidStartTaskWithID:(actor::ActorTaskId)taskID
                            sourceInfo:(const actor::TaskSourceInfo&)sourceInfo;

// Called when a task is stopped.
- (void)actorServiceDidStopTaskWithID:(actor::ActorTaskId)taskID
                           sourceInfo:(const actor::TaskSourceInfo&)sourceInfo
                           finalState:(actor::ActorTaskState)finalState;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_LIFECYCLE_OBSERVER_H_
