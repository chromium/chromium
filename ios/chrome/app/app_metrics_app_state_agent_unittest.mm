// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_metrics_app_state_agent.h"

#import "base/test/task_environment.h"
#import "ios/chrome/app/application_delegate/app_init_stage_test_utils.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

// A fake that allows overriding connectedScenes.
@interface FakeAppState : AppState
@property(nonatomic, strong) NSArray<SceneState*>* connectedScenes;
// Init stage that will be returned by the initStage getter when testing.
@property(nonatomic, assign) AppInitStage initStageForTesting;
@end

@implementation FakeAppState

- (AppInitStage)initStage {
  return self.initStageForTesting;
}

@end

AppInitStage GetMinimalInitStageThatAllowsLogging() {
  return AppInitStage::kBrowserObjectsForBackgroundHandlers;
}

AppInitStage GetMaximalInitStageThatDontAllowLogging() {
  return PreviousAppInitStage(GetMinimalInitStageThatAllowsLogging());
}

class AppMetricsAppStateAgentTest : public PlatformTest {
 protected:
  AppMetricsAppStateAgentTest() {
    agent_ = [[AppMetricsAppStateAgent alloc] init];

    app_state_ = [[FakeAppState alloc] initWithStartupInformation:nil];
  }

  void SetUp() override {
    PlatformTest::SetUp();
    app_state_.initStageForTesting = GetMinimalInitStageThatAllowsLogging();
    [agent_ setAppState:app_state_];
  }

  void SimulateTransitionToCurrentStage() {
    AppInitStage previousStage =
        app_state_.initStage == AppInitStage::kStart
            ? AppInitStage::kStart
            : PreviousAppInitStage(app_state_.initStage);
    [agent_ appState:app_state_ didTransitionFromInitStage:previousStage];
  }

  AppMetricsAppStateAgent* agent_;
  FakeAppState* app_state_;
};

// Tests that -logStartupDuration: and -createStartupTrackingTask are called
// once and in the right order for a regular startup (no safe mode).
TEST_F(AppMetricsAppStateAgentTest, LogStartupDuration) {
  base::test::SingleThreadTaskEnvironment task_environment;
  id metricsMediator = [OCMockObject mockForClass:[MetricsMediator class]];

  [[metricsMediator expect] createStartupTrackingTask];
  [[metricsMediator expect] logStartupDuration:nil];
  [metricsMediator setExpectationOrderMatters:YES];

  SceneState* sceneA = [[SceneState alloc] initWithAppState:app_state_];
  SceneState* sceneB = [[SceneState alloc] initWithAppState:app_state_];
  app_state_.connectedScenes = @[ sceneA, sceneB ];
  [agent_ appState:app_state_ sceneConnected:sceneA];
  [agent_ appState:app_state_ sceneConnected:sceneB];

  // Simulate transitioning to the current app init stage before scenes going on
  // the foreground.
  AppInitStage previousInitStage = PreviousAppInitStage(app_state_.initStage);
  [agent_ appState:app_state_ didTransitionFromInitStage:previousInitStage];

  // Should not log startup duration until the scene is active.
  sceneA.activationLevel = SceneActivationLevelForegroundInactive;
  sceneB.activationLevel = SceneActivationLevelForegroundInactive;

  // Should only log startup once when the first scene becomes active.
  sceneA.activationLevel = SceneActivationLevelForegroundActive;
  sceneB.activationLevel = SceneActivationLevelForegroundActive;

  // Should not log startup when scene becomes active again.
  sceneA.activationLevel = SceneActivationLevelBackground;
  sceneB.activationLevel = SceneActivationLevelBackground;
  sceneA.activationLevel = SceneActivationLevelForegroundActive;
  sceneB.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_OCMOCK_VERIFY(metricsMediator);
}

// Tests that -logStartupDuration: and  and -createStartupTrackingTask are not
// called when there is safe mode during startup.
TEST_F(AppMetricsAppStateAgentTest, LogStartupDurationWhenSafeMode) {
  id metricsMediator = [OCMockObject mockForClass:[MetricsMediator class]];

  [[metricsMediator reject] createStartupTrackingTask];
  [[metricsMediator reject] logStartupDuration:nil];

  app_state_.initStageForTesting = GetMaximalInitStageThatDontAllowLogging();

  SceneState* sceneA = [[SceneState alloc] initWithAppState:app_state_];
  app_state_.connectedScenes = @[ sceneA ];
  [agent_ appState:app_state_ sceneConnected:sceneA];

  // Simulate transitioning to the current app init stage before scenes going on
  // the foreground.
  AppInitStage previousInitStage = PreviousAppInitStage(app_state_.initStage);
  [agent_ appState:app_state_ didTransitionFromInitStage:previousInitStage];

  // This would normally log startup information, but not when in safe mode.
  sceneA.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_OCMOCK_VERIFY(metricsMediator);
}
