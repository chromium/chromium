// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_TASK_MANAGEMENT_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_TASK_MANAGEMENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

@class TaskRequest;

// Methods to manage the `pendingTasks` of a `SceneState`, restricted to the
// classes owning the startup flow tasks.
@interface SceneState (TaskManagement)

// Appends `task` to `pendingTasks`.
- (void)addPendingTask:(TaskRequest*)task;

// Removes `task` from `pendingTasks`, if present.
- (void)removePendingTask:(TaskRequest*)task;

// Removes all the tasks from `pendingTasks`.
- (void)removeAllPendingTasks;

// Removes the tasks from `pendingTasks` for which `block` returns YES.
- (void)removeTasksWithBlock:(BOOL(NS_NOESCAPE ^)(TaskRequest*))block;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_TASK_MANAGEMENT_H_
