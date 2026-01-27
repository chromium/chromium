// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_ORCHESTRATOR_H_
#define IOS_CHROME_APP_TASK_ORCHESTRATOR_H_

#import <Foundation/Foundation.h>

#import <string_view>

#import "ios/chrome/app/task_request.h"

// Orchestrates the execution of TaskRequests by managing a queue of pending
// tasks and ensuring they only run when their required application lifecycle
// stage has been reached. It tracks the current execution stage for each
// connected scene and acts as the central coordinator for deferred startup and
// external-action tasks.
@interface TaskOrchestrator : NSObject

// Adds a new task for execution. Executes immediately if the minimum
// TaskExecutionStage is met; otherwise, queues the task in _pendingTasks
// for later execution.
- (void)addTaskRequest:(TaskRequest*)request;

// Called when the app progresses through lifecycle.
- (void)updateToStage:(TaskExecutionStage)stage
             forScene:(std::string_view)sceneSessionID;

@end

#endif  // IOS_CHROME_APP_TASK_ORCHESTRATOR_H_
