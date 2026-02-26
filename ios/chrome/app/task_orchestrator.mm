// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_orchestrator.h"

#import <map>
#import <string>

#import "ios/chrome/browser/shared/public/features/features.h"
#import "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace {
struct SceneInfo {
  // Current stage of a scene.
  TaskExecutionStage current_stage;
  // Tasks to be executed on a scene.
  NSMutableArray<TaskRequest*>* pending_tasks;

  // Adds a task to pending_tasks.
  void AddTask(TaskRequest* task) {
    if (!pending_tasks) {
      pending_tasks = [NSMutableArray array];
    }
    [pending_tasks addObject:task];
  }
};
}  // namespace

@interface TaskOrchestrator () {
  // SceneInfo needed to execute tasks per scene.
  // TODO(crbug.com/462018636): Add implementation to handle the case where a
  // task can be executed on any scene.
  absl::flat_hash_map<std::string, SceneInfo> _tasksPerScene;
}

@end

@implementation TaskOrchestrator

- (instancetype)init {
  if ((self = [super init])) {
    CHECK(IsEnableNewStartupFlowEnabled());
  }
  return self;
}

- (void)addTaskRequest:(TaskRequest*)task {
  // Don't add the task if it requires an account/profile change and there is
  // already a task in a queue requiring a change to a different account.
  if ([self shouldDropTaskRequest:task]) {
    return;
  }

  SceneInfo& sceneInfo = _tasksPerScene[task.sceneSessionID];
  sceneInfo.AddTask(task);
  [self executeTasksForScene:task.sceneSessionID];
}

- (void)updateToStage:(TaskExecutionStage)stage
             forScene:(std::string_view)sceneSessionID {
  TaskExecutionStage& currentStage =
      _tasksPerScene[sceneSessionID].current_stage;
  if (currentStage < stage) {
    _tasksPerScene[sceneSessionID].current_stage = stage;
    [self executeTasksForScene:sceneSessionID];
  }
}

#pragma mark - Private

// Returns whether the task should be dropped.
- (BOOL)shouldDropTaskRequest:(TaskRequest*)task {
  NSString* taskGaiaID = task.gaiaID;
  if (!taskGaiaID) {
    return NO;
  }

  SceneInfo& sceneInfo = _tasksPerScene[task.sceneSessionID];
  for (TaskRequest* pendingTask in sceneInfo.pending_tasks) {
    NSString* pendingGaiaID = pendingTask.gaiaID;
    if (pendingGaiaID && ![pendingGaiaID isEqualToString:taskGaiaID]) {
      // TODO(crbug.com/462018636): Record metrics when this happens.
      return YES;
    }
  }
  return NO;
}

// Internal logic to filter and execute tasks based on the current stage.
- (void)executeTasksForScene:(std::string_view)sceneSessionID {
  SceneInfo& sceneInfo = _tasksPerScene[sceneSessionID];
  NSMutableArray<TaskRequest*>* pendingTasks =
      std::exchange(sceneInfo.pending_tasks, [NSMutableArray new]);
  for (TaskRequest* task in pendingTasks) {
    if (task.minimumStage <= sceneInfo.current_stage) {
      [task execute];
    } else {
      sceneInfo.AddTask(task);
    }
  }
}

@end
