// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_orchestrator.h"

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/app/task_request+testing.h"
#import "ios/chrome/app/task_scheduling_outcome.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
// Creates a new SceneState with the given `persistentIdentifier`.
SceneState* CreateFakeSceneState(NSString* persistent_identifier) {
  id fake_scene = OCMClassMock([UIWindowScene class]);
  id fake_scene_session = OCMClassMock([UISceneSession class]);
  OCMStub([fake_scene_session persistentIdentifier])
      .andReturn(persistent_identifier);
  OCMStub([fake_scene session]).andReturn(fake_scene_session);
  SceneState* scene_state = [[SceneState alloc] init];
  scene_state.scene = fake_scene;
  return scene_state;
}
}  // namespace

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
  base::HistogramTester histogram_tester_;
};

// Tests that a task with minimum stage None is executed immediately.
TEST_F(TaskOrchestratorTest, TestAddTaskRequestExecuteImmediately) {
  NSString* scene_id = @"scene1";
  SceneState* scene_state = CreateFakeSceneState(scene_id);
  __block BOOL taskWasExecuted = NO;

  TaskRequest* task = [TaskRequest taskForTestingWithScene:scene_state.scene
                                              executeBlock:^{
                                                taskWasExecuted = YES;
                                              }];
  task.minimumStage = TaskExecutionStage::TaskExecutionStageNone;

  [orchestrator_ addTaskRequest:task];

  EXPECT_TRUE(taskWasExecuted);
}

// Tests that a task is queued if the stage is not met, and executed when it is.
TEST_F(TaskOrchestratorTest, TestAddTaskRequestQueueAndExecuteLater) {
  NSString* scene_id = @"scene1";
  SceneState* scene_state = CreateFakeSceneState(scene_id);
  __block BOOL taskWasExecuted = NO;

  TaskRequest* task = [TaskRequest taskForTestingWithScene:scene_state.scene
                                              executeBlock:^{
                                                taskWasExecuted = YES;
                                              }];
  task.minimumStage = TaskExecutionStage::TaskExecutionUIReady;

  [orchestrator_ addTaskRequest:task];

  // Make sure that block is not run.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionProfileLoaded
                      forScene:scene_state];
  EXPECT_FALSE(taskWasExecuted);

  // Update executionBlock to check that it's correctly called when updating to
  // the correct stage.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionUIReady
                      forScene:scene_state];
  EXPECT_TRUE(taskWasExecuted);
}

// Tests that tasks for different scenes are handled independently.
TEST_F(TaskOrchestratorTest, TestMultipleScenes) {
  NSString* scene_id1 = @"scene1";
  SceneState* scene_state1 = CreateFakeSceneState(scene_id1);
  __block BOOL task1WasExecuted = NO;
  TaskRequest* task1 = [TaskRequest taskForTestingWithScene:scene_state1.scene
                                               executeBlock:^{
                                                 task1WasExecuted = YES;
                                               }];
  task1.minimumStage = TaskExecutionStage::TaskExecutionUIReady;

  NSString* scene_id2 = @"scene2";
  SceneState* scene_state2 = CreateFakeSceneState(scene_id2);
  __block BOOL task2WasExecuted = NO;
  TaskRequest* task2 = [TaskRequest taskForTestingWithScene:scene_state2.scene
                                               executeBlock:^{
                                                 task2WasExecuted = YES;
                                               }];
  task2.minimumStage = TaskExecutionStage::TaskExecutionUIReady;

  [orchestrator_ addTaskRequest:task1];
  [orchestrator_ addTaskRequest:task2];

  // Check that only task1 is executed.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionUIReady
                      forScene:scene_state1];
  EXPECT_TRUE(task1WasExecuted);
  EXPECT_FALSE(task2WasExecuted);

  // Check that only task2 is executed.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionUIReady
                      forScene:scene_state2];
  EXPECT_TRUE(task2WasExecuted);
}

// Tests that multiple tasks for the same scene with different stages are
// executed at the correct time.
TEST_F(TaskOrchestratorTest, TestMultipleStagesSameScene) {
  NSString* scene_id = @"scene1";
  SceneState* scene_state = CreateFakeSceneState(scene_id);

  __block BOOL task1WasExecuted = NO;
  TaskRequest* task1 = [TaskRequest taskForTestingWithScene:scene_state.scene
                                               executeBlock:^{
                                                 task1WasExecuted = YES;
                                               }];
  task1.minimumStage = TaskExecutionStage::TaskExecutionProfileLoaded;

  __block BOOL task2WasExecuted = NO;
  TaskRequest* task2 = [TaskRequest taskForTestingWithScene:scene_state.scene
                                               executeBlock:^{
                                                 task2WasExecuted = YES;
                                               }];
  task2.minimumStage = TaskExecutionStage::TaskExecutionUIReady;

  [orchestrator_ addTaskRequest:task1];
  [orchestrator_ addTaskRequest:task2];

  // Transition to ProfileLoaded. Check that only task1 is executed.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionProfileLoaded
                      forScene:scene_state];
  EXPECT_TRUE(task1WasExecuted);
  EXPECT_FALSE(task2WasExecuted);

  // Transition to UIReady. Check that task2 is executed.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionUIReady
                      forScene:scene_state];
  EXPECT_TRUE(task2WasExecuted);
}

// Tests that a task is dropped if there is already a pending task for the same
// scene with a different Gaia ID.
TEST_F(TaskOrchestratorTest, TestDropTaskRequestWithDifferentGaiaID) {
  NSString* scene_id = @"scene1";
  SceneState* scene_state = CreateFakeSceneState(scene_id);
  NSString* gaia_id1 = @"gaia1";
  NSString* gaia_id2 = @"gaia2";

  __block BOOL task1WasExecuted = NO;
  TaskRequest* task1 = [TaskRequest taskForTestingWithScene:scene_state.scene
                                               executeBlock:^{
                                                 task1WasExecuted = YES;
                                               }];
  task1.minimumStage = TaskExecutionStage::TaskExecutionUIReady;
  task1.gaiaID = gaia_id1;

  __block BOOL task2WasExecuted = NO;
  TaskRequest* task2 = [TaskRequest taskForTestingWithScene:scene_state.scene
                                               executeBlock:^{
                                                 task2WasExecuted = YES;
                                               }];
  task2.minimumStage = TaskExecutionStage::TaskExecutionUIReady;
  task2.gaiaID = gaia_id2;

  [orchestrator_ addTaskRequest:task1];
  [orchestrator_ addTaskRequest:task2];

  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionUIReady
                      forScene:scene_state];

  // task1 should be executed, task2 should be dropped.
  EXPECT_TRUE(task1WasExecuted);
  EXPECT_FALSE(task2WasExecuted);

  // Make sure histogram is correctly updated.
  histogram_tester_.ExpectBucketCount(
      "IOS.TaskOrchestrator.TaskSchedulingOutcome",
      TaskSchedulingOutcome::kScheduled, 1);
  histogram_tester_.ExpectBucketCount(
      "IOS.TaskOrchestrator.TaskSchedulingOutcome",
      TaskSchedulingOutcome::kDroppedGaiaMismatch, 1);
}

// Tests that a task is not dropped if it has the same Gaia ID as already
// pending tasks for the same scene.
TEST_F(TaskOrchestratorTest, TestNotDropTaskRequestWithSameGaiaID) {
  NSString* scene_id = @"scene1";
  SceneState* scene_state = CreateFakeSceneState(scene_id);
  NSString* gaia_id = @"gaia";

  __block BOOL task1WasExecuted = NO;
  TaskRequest* task1 = [TaskRequest taskForTestingWithScene:scene_state.scene
                                               executeBlock:^{
                                                 task1WasExecuted = YES;
                                               }];
  task1.minimumStage = TaskExecutionStage::TaskExecutionUIReady;
  task1.gaiaID = gaia_id;

  __block BOOL task2WasExecuted = NO;
  TaskRequest* task2 = [TaskRequest taskForTestingWithScene:scene_state.scene
                                               executeBlock:^{
                                                 task2WasExecuted = YES;
                                               }];
  task2.minimumStage = TaskExecutionStage::TaskExecutionUIReady;
  task2.gaiaID = gaia_id;

  [orchestrator_ addTaskRequest:task1];
  [orchestrator_ addTaskRequest:task2];

  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionUIReady
                      forScene:scene_state];

  // Both tasks should be executed.
  EXPECT_TRUE(task1WasExecuted);
  EXPECT_TRUE(task2WasExecuted);

  // Make sure histogram is correctly updated.
  histogram_tester_.ExpectUniqueSample(
      "IOS.TaskOrchestrator.TaskSchedulingOutcome",
      TaskSchedulingOutcome::kScheduled, 2);
}

// Tests that gaiaIDForScene returns the Gaia ID of the first pending task for
// that scene, or nil if no tasks are pending.
TEST_F(TaskOrchestratorTest, TestGaiaIDForScene) {
  NSString* scene_id = @"scene1";
  SceneState* scene_state = CreateFakeSceneState(scene_id);

  // When no tasks are pending, gaiaIDForScene returns nil.
  EXPECT_NSEQ(nil, [orchestrator_ gaiaIDForScene:scene_state]);

  NSString* gaia_id = @"test_gaia_id";
  TaskRequest* task = [TaskRequest taskForTestingWithScene:scene_state.scene
                                              executeBlock:^{
                                              }];
  task.minimumStage = TaskExecutionStage::TaskExecutionUIReady;
  task.gaiaID = gaia_id;

  [orchestrator_ addTaskRequest:task];

  // While task is pending, gaiaIDForScene returns the task's Gaia ID.
  EXPECT_NSEQ(gaia_id, [orchestrator_ gaiaIDForScene:scene_state]);

  // When updating to UIReady, the task executes and is removed from pending.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionUIReady
                      forScene:scene_state];

  // Now no tasks are pending, returns nil.
  EXPECT_NSEQ(nil, [orchestrator_ gaiaIDForScene:scene_state]);
}
