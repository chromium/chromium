// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_orchestrator.h"

#import <UIKit/UIKit.h>

#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/application_delegate/app_init_stage.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/task_request+testing.h"
#import "ios/chrome/app/task_scheduling_outcome.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface FakeChangeProfileCommandsHandler : NSObject <ChangeProfileCommands>
@property(nonatomic, assign) BOOL changeProfileCalled;
@property(nonatomic, assign) std::string profileName;
@property(nonatomic, assign) ChangeProfileReason reason;
- (BOOL)hasContinuation;
- (void)runContinuationForSceneState:(SceneState*)sceneState;
@end

@implementation FakeChangeProfileCommandsHandler {
  ChangeProfileContinuation _continuation;
}
- (void)changeProfile:(std::string_view)profileName
             forScene:(SceneState*)sceneState
               reason:(ChangeProfileReason)reason
         continuation:(ChangeProfileContinuation)continuation {
  self.changeProfileCalled = YES;
  self.profileName = std::string(profileName);
  self.reason = reason;
  _continuation = std::move(continuation);
}
- (void)deleteProfile:(std::string_view)profileName {
}
- (BOOL)hasContinuation {
  return !_continuation.is_null();
}
- (void)runContinuationForSceneState:(SceneState*)sceneState {
  std::move(_continuation).Run(sceneState, base::DoNothing());
}
@end

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

// Tests that a task requiring a profile or account switch triggers
// ChangeProfileCommands at TaskExecutionProfileLoaded (before
// TaskExecutionUIReady) and only executes after the continuation completes and
// TaskExecutionUIReady is reached.
TEST_F(TaskOrchestratorTest,
       TestProfileSwitchStartedAtProfileLoadedBeforeUIReady) {
  IOSChromeScopedTestingLocalState scoped_testing_local_state;
  TestProfileManagerIOS profile_manager;

  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(AuthenticationServiceFactory::GetInstance(),
                            AuthenticationServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                            base::BindRepeating(&CreateTestSyncService));
  TestProfileIOS* profile =
      profile_manager.AddProfileWithBuilder(std::move(builder));

  FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(identity);

  CommandDispatcher* dispatcher = [[CommandDispatcher alloc] init];
  FakeChangeProfileCommandsHandler* change_profile_handler =
      [[FakeChangeProfileCommandsHandler alloc] init];
  [dispatcher startDispatchingToTarget:change_profile_handler
                           forProtocol:@protocol(ChangeProfileCommands)];

  id mock_app_state = OCMClassMock([AppState class]);
  OCMStub([mock_app_state initStage]).andReturn(AppInitStage::kFinal);
  OCMStub([mock_app_state appCommandDispatcher]).andReturn(dispatcher);

  ProfileState* profile_state =
      [[ProfileState alloc] initWithAppState:mock_app_state];
  profile_state.profile = profile;

  SceneState* scene_state = CreateFakeSceneState(@"scene_switch");
  scene_state.profileState = profile_state;
  id mock_scene_delegate = OCMClassMock([SceneDelegate class]);
  OCMStub([mock_scene_delegate sceneState]).andReturn(scene_state);
  OCMStub([scene_state.scene delegate]).andReturn(mock_scene_delegate);

  id mock_browser_provider = OCMProtocolMock(@protocol(BrowserProvider));
  TestBrowser browser(profile, scene_state);
  OCMStub([mock_browser_provider browser]).andReturn(&browser);
  id mock_provider_interface =
      OCMProtocolMock(@protocol(BrowserProviderInterface));
  OCMStub([mock_provider_interface mainBrowserProvider])
      .andReturn(mock_browser_provider);
  id partial_scene_state = OCMPartialMock(scene_state);
  OCMStub([partial_scene_state browserProviderInterface])
      .andReturn(mock_provider_interface);

  bool task_was_executed = false;
  bool* task_was_executed_ptr = &task_was_executed;
  TaskRequest* task = [TaskRequest taskForTestingWithScene:scene_state.scene
                                              executeBlock:^{
                                                *task_was_executed_ptr = true;
                                              }];
  task.minimumStage = TaskExecutionStage::TaskExecutionUIReady;
  task.gaiaID = identity.gaiaId.ToNSString();

  [orchestrator_ addTaskRequest:task];
  EXPECT_FALSE(change_profile_handler.changeProfileCalled);
  EXPECT_FALSE(task_was_executed);

  // Updating to TaskExecutionProfileLoaded should immediately initiate
  // changeProfile: before TaskExecutionUIReady is reached.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionProfileLoaded
                      forScene:scene_state];
  EXPECT_TRUE(change_profile_handler.changeProfileCalled);
  EXPECT_EQ(ChangeProfileReason::kSwitchAccountsFromWidget,
            change_profile_handler.reason);
  EXPECT_FALSE(task_was_executed);

  // Even if TaskExecutionUIReady is reached while the switch continuation is
  // still in progress, the task must not execute until the continuation runs.
  [orchestrator_ updateToStage:TaskExecutionStage::TaskExecutionUIReady
                      forScene:scene_state];
  EXPECT_FALSE(task_was_executed);

  // Running the ChangeProfileContinuation completes the switch and executes the
  // ready task.
  ASSERT_TRUE([change_profile_handler hasContinuation]);
  [change_profile_handler runContinuationForSceneState:scene_state];
  EXPECT_TRUE(base::test::RunUntil([&]() { return task_was_executed; }));
}
