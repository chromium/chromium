// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_orchestrator.h"

#import <UIKit/UIKit.h>

#import <map>
#import <string>

#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/task_scheduling_outcome.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
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

// Returns an identifier for `scene` that will be stable for the current
// execution of the application and is valid even during the early stage of the
// application startup (i.e. before -sceneSessionID is assigned to the
// SceneState). This uses the -persistentIdentifier of the UISceneSession.
std::string GetSceneIdentifier(UIScene* scene) {
  return base::SysNSStringToUTF8(scene.session.persistentIdentifier);
}

}  // namespace

@interface TaskOrchestrator () {
  // SceneInfo needed to execute tasks per scene.
  // It's okay to use persistentIdentifier here because we don't care if the ID
  // changes when the app is closed.
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

  const std::string sceneKey = GetSceneIdentifier(task.scene);
  CHECK(!sceneKey.empty());

  SceneInfo& sceneInfo = _tasksPerScene[sceneKey];
  sceneInfo.AddTask(task);
  [self executeTasksForScene:sceneKey];
}

- (void)updateToStage:(TaskExecutionStage)stage
             forScene:(SceneState*)sceneState {
  const std::string sceneKey = GetSceneIdentifier(sceneState.scene);
  if (sceneKey.empty()) {
    return;
  }

  TaskExecutionStage previousStage = _tasksPerScene[sceneKey].current_stage;
  _tasksPerScene[sceneKey].current_stage = stage;
  if (previousStage < stage) {
    [self executeTasksForScene:sceneKey];
  }
}

- (NSString*)gaiaIDForScene:(SceneState*)sceneState {
  const std::string sceneKey = GetSceneIdentifier(sceneState.scene);
  if (sceneKey.empty()) {
    return nil;
  }

  auto it = _tasksPerScene.find(sceneKey);
  if (it == _tasksPerScene.end()) {
    return nil;
  }

  SceneInfo& sceneInfo = it->second;
  for (TaskRequest* pendingTask in sceneInfo.pending_tasks) {
    if (pendingTask.gaiaID) {
      return pendingTask.gaiaID;
    }
  }
  return nil;
}

#pragma mark - Private

// Returns whether the task should be dropped.
- (BOOL)shouldDropTaskRequest:(TaskRequest*)task {
  NSString* taskGaiaID = task.gaiaID;
  if (!taskGaiaID) {
    base::UmaHistogramEnumeration("IOS.TaskOrchestrator.TaskSchedulingOutcome",
                                  TaskSchedulingOutcome::kScheduled);
    return NO;
  }

  const std::string sceneKey = GetSceneIdentifier(task.scene);
  SceneInfo& sceneInfo = _tasksPerScene[sceneKey];
  for (TaskRequest* pendingTask in sceneInfo.pending_tasks) {
    NSString* pendingGaiaID = pendingTask.gaiaID;
    if (pendingGaiaID && ![pendingGaiaID isEqualToString:taskGaiaID]) {
      base::UmaHistogramEnumeration(
          "IOS.TaskOrchestrator.TaskSchedulingOutcome",
          TaskSchedulingOutcome::kDroppedGaiaMismatch);
      return YES;
    }
  }
  base::UmaHistogramEnumeration("IOS.TaskOrchestrator.TaskSchedulingOutcome",
                                TaskSchedulingOutcome::kScheduled);
  return NO;
}

// Internal logic to filter and execute tasks based on the current stage.
- (void)executeTasksForScene:(std::string_view)sceneKey {
  SceneInfo& sceneInfo = _tasksPerScene[sceneKey];
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
