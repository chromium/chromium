// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_orchestrator.h"

#import "base/ios/block_types.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/app/task_request+testing.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class TaskOrchestratorTest : public PlatformTest {
 protected:
  TaskOrchestratorTest() {
    ResetEnableNewStartupFlowEnabledForTesting();
    scoped_feature_list_.InitAndEnableFeature(kEnableNewStartupFlow);
    SaveEnableNewStartupFlowForNextStart();
  }

  ~TaskOrchestratorTest() override {
    ResetEnableNewStartupFlowEnabledForTesting();
  }

  void SetUp() override {
    PlatformTest::SetUp();
    orchestrator_ = [[TaskOrchestrator alloc] init];
  }

  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TaskOrchestrator* orchestrator_;
};

// Tests that a task with minimum stage None is executed immediately.
TEST_F(TaskOrchestratorTest, TestAddTaskRequestExecuteImmediately) {
  std::string scene_id = "scene1";
  __block BOOL taskWasExecuted = NO;

  TaskRequest* task = [TaskRequest taskForTestingWithSceneID:scene_id
                                                executeBlock:^{
                                                  taskWasExecuted = YES;
                                                }];
  task.minimumStage = TaskExecutionStage::TaskExecutionStageNone;

  [orchestrator_ addTaskRequest:task];

  EXPECT_TRUE(taskWasExecuted);
}

// Tests that a task is queued if the stage is not met, and executed when it is.
TEST_F(TaskOrchestratorTest, TestAddTaskRequestQueueAndExecuteLater) {
  std::string scene_id = "scene1";
  __block BOOL taskWasExecuted = NO;

  TaskRequest* task = [TaskRequest taskForTestingWithSceneID:scene_id
                                                executeBlock:^{
                                                  taskWasExecuted = YES;
                                                }];
  task.minimumStage = TaskExecutionStage::TaskExecutionUIReady;

  [orchestrator_ addTaskRequest:task];

  // Make sure that block is not run.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionProfileLoaded
                      forScene:scene_id];
  EXPECT_FALSE(taskWasExecuted);

  // Update executionBlock to check that it's correctly called when updating to
  // the correct stage.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionUIReady
                      forScene:scene_id];
  EXPECT_TRUE(taskWasExecuted);
}

// Tests that tasks for different scenes are handled independently.
TEST_F(TaskOrchestratorTest, TestMultipleScenes) {
  std::string scene_id1 = "scene1";
  __block BOOL task1WasExecuted = NO;
  TaskRequest* task1 = [TaskRequest taskForTestingWithSceneID:scene_id1
                                                 executeBlock:^{
                                                   task1WasExecuted = YES;
                                                 }];
  task1.minimumStage = TaskExecutionStage::TaskExecutionUIReady;

  std::string scene_id2 = "scene2";
  __block BOOL task2WasExecuted = NO;
  TaskRequest* task2 = [TaskRequest taskForTestingWithSceneID:scene_id2
                                                 executeBlock:^{
                                                   task2WasExecuted = YES;
                                                 }];
  task2.minimumStage = TaskExecutionStage::TaskExecutionUIReady;

  [orchestrator_ addTaskRequest:task1];
  [orchestrator_ addTaskRequest:task2];

  // Check that only task1 is executed.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionUIReady
                      forScene:scene_id1];
  EXPECT_TRUE(task1WasExecuted);
  EXPECT_FALSE(task2WasExecuted);

  // Check that only task2 is executed.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionUIReady
                      forScene:scene_id2];
  EXPECT_TRUE(task2WasExecuted);
}

// Tests that multiple tasks for the same scene with different stages are
// executed at the correct time.
TEST_F(TaskOrchestratorTest, TestMultipleStagesSameScene) {
  std::string scene_id = "scene1";

  __block BOOL task1WasExecuted = NO;
  TaskRequest* task1 = [TaskRequest taskForTestingWithSceneID:scene_id
                                                 executeBlock:^{
                                                   task1WasExecuted = YES;
                                                 }];
  task1.minimumStage = TaskExecutionStage::TaskExecutionProfileLoaded;

  __block BOOL task2WasExecuted = NO;
  TaskRequest* task2 = [TaskRequest taskForTestingWithSceneID:scene_id
                                                 executeBlock:^{
                                                   task2WasExecuted = YES;
                                                 }];
  task2.minimumStage = TaskExecutionStage::TaskExecutionUIReady;

  [orchestrator_ addTaskRequest:task1];
  [orchestrator_ addTaskRequest:task2];

  // Transition to ProfileLoaded. Check that only task1 is executed.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionProfileLoaded
                      forScene:scene_id];
  EXPECT_TRUE(task1WasExecuted);
  EXPECT_FALSE(task2WasExecuted);

  // Transition to UIReady. Check that task2 is executed.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionUIReady
                      forScene:scene_id];
  EXPECT_TRUE(task2WasExecuted);
}

// Tests that a task is dropped if there is already a pending task for the same
// scene with a different Gaia ID.
TEST_F(TaskOrchestratorTest, TestDropTaskRequestWithDifferentGaiaID) {
  std::string scene_id = "scene1";
  NSString* gaia_id1 = @"gaia1";
  NSString* gaia_id2 = @"gaia2";

  __block BOOL task1WasExecuted = NO;
  TaskRequest* task1 = [TaskRequest taskForTestingWithSceneID:scene_id
                                                 executeBlock:^{
                                                   task1WasExecuted = YES;
                                                 }];
  task1.minimumStage = TaskExecutionStage::TaskExecutionUIReady;
  task1.gaiaID = gaia_id1;

  __block BOOL task2WasExecuted = NO;
  TaskRequest* task2 = [TaskRequest taskForTestingWithSceneID:scene_id
                                                 executeBlock:^{
                                                   task2WasExecuted = YES;
                                                 }];
  task2.minimumStage = TaskExecutionStage::TaskExecutionUIReady;
  task2.gaiaID = gaia_id2;

  [orchestrator_ addTaskRequest:task1];
  [orchestrator_ addTaskRequest:task2];

  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionUIReady
                      forScene:scene_id];

  // task1 should be executed, task2 should be dropped.
  EXPECT_TRUE(task1WasExecuted);
  EXPECT_FALSE(task2WasExecuted);
}

// Tests that a task is not dropped if it has the same Gaia ID as already
// pending tasks for the same scene.
TEST_F(TaskOrchestratorTest, TestNotDropTaskRequestWithSameGaiaID) {
  std::string scene_id = "scene1";
  NSString* gaia_id = @"gaia";

  __block BOOL task1WasExecuted = NO;
  TaskRequest* task1 = [TaskRequest taskForTestingWithSceneID:scene_id
                                                 executeBlock:^{
                                                   task1WasExecuted = YES;
                                                 }];
  task1.minimumStage = TaskExecutionStage::TaskExecutionUIReady;
  task1.gaiaID = gaia_id;

  __block BOOL task2WasExecuted = NO;
  TaskRequest* task2 = [TaskRequest taskForTestingWithSceneID:scene_id
                                                 executeBlock:^{
                                                   task2WasExecuted = YES;
                                                 }];
  task2.minimumStage = TaskExecutionStage::TaskExecutionUIReady;
  task2.gaiaID = gaia_id;

  [orchestrator_ addTaskRequest:task1];
  [orchestrator_ addTaskRequest:task2];

  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionUIReady
                      forScene:scene_id];

  // Both tasks should be executed.
  EXPECT_TRUE(task1WasExecuted);
  EXPECT_TRUE(task2WasExecuted);
}
