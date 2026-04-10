// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/public/actor_types.h"

// The handle the Actor layer provides to the UI for simple ActorTask lifecycle
// management.
@protocol ActorTaskController <NSObject>

// TODO(crbug.com/501043031): Remove @optional when API stabilizes.
@optional

// Pause the active task.
- (void)pauseTask;

// Resume a paused task.
- (void)resumeTask;

// Stop the task completely with a reason.
- (void)stopTaskWithReason:(actor::ActorTaskStoppedReason)reason;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_PUBLIC_ACTOR_TASK_CONTROLLER_H_
