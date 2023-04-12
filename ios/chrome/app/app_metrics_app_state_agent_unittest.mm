// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_metrics_app_state_agent.h"

#import "base/test/task_environment.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/metrics/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/metrics/ios_profile_session_durations_service_factory.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class FakeProfileSessionDurationsService
    : public IOSProfileSessionDurationsService {
 public:
  FakeProfileSessionDurationsService() = default;

  FakeProfileSessionDurationsService(
      const FakeProfileSessionDurationsService&) = delete;
  FakeProfileSessionDurationsService& operator=(
      const FakeProfileSessionDurationsService&) = delete;

  ~FakeProfileSessionDurationsService() override = default;

  static std::unique_ptr<KeyedService> Create(
      web::BrowserState* browser_state) {
    return std::make_unique<FakeProfileSessionDurationsService>();
  }

  void OnSessionStarted(base::TimeTicks session_start) override {
    ++session_started_count_;
  }
  void OnSessionEnded(base::TimeDelta session_length) override {
    ++session_ended_count_;
  }

  bool IsSessionActive() override {
    return session_started_count_ > session_ended_count_;
  }

  // IOSProfileSessionDurationsService:
  int session_started_count() const { return session_started_count_; }
  int session_ended_count() const { return session_ended_count_; }

 private:
  int session_started_count_ = 0;
  int session_ended_count_ = 0;
};
}  // namespace

// A fake that allows overriding connectedScenes.
@interface FakeAppState : AppState
@property(nonatomic, strong) NSArray<SceneState*>* connectedScenes;
// Init stage that will be returned by the initStage getter when testing.
@property(nonatomic, assign) InitStage initStageForTesting;
@end

@implementation FakeAppState

- (InitStage)initStage {
  return self.initStageForTesting;
}

@end

InitStage GetMinimalInitStageThatAllowsLogging() {
  return static_cast<InitStage>(InitStageSafeMode + 1);
}

InitStage GetMaximalInitStageThatDontAllowLogging() {
  return static_cast<InitStage>(InitStageSafeMode);
}

class AppMetricsAppStateAgentTest : public PlatformTest {
 protected:
  AppMetricsAppStateAgentTest() {
    agent_ = [[AppMetricsAppStateAgent alloc] init];

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        IOSProfileSessionDurationsServiceFactory::GetInstance(),
        base::BindRepeating(&FakeProfileSessionDurationsService::Create));
    browser_state_ = test_cbs_builder.Build();

    app_state_ = [[FakeAppState alloc] initWithBrowserLauncher:nil
                                            startupInformation:nil
                                           applicationDelegate:nil];
  }

  void SetUp() override {
    PlatformTest::SetUp();
    app_state_.mainBrowserState = browser_state_.get();
    app_state_.initStageForTesting = GetMinimalInitStageThatAllowsLogging();
    [agent_ setAppState:app_state_];
  }

  FakeProfileSessionDurationsService* getProfileSessionDurationsService() {
    return static_cast<FakeProfileSessionDurationsService*>(
        IOSProfileSessionDurationsServiceFactory::GetForBrowserState(
            browser_state_.get()));
  }

  void SimulateTransitionToCurrentStage() {
    InitStage previousStage =
        app_state_.initStage == InitStageStart
            ? InitStageStart
            : static_cast<InitStage>(app_state_.initStage - 1);
    [agent_ appState:app_state_ didTransitionFromInitStage:previousStage];
  }

  AppMetricsAppStateAgent* agent_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  FakeAppState* app_state_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AppMetricsAppStateAgentTest, CountSessionDuration) {
  SceneState* scene = [[SceneState alloc] initWithAppState:app_state_];
  app_state_.connectedScenes = @[ scene ];
  [agent_ appState:app_state_ sceneConnected:scene];

  EXPECT_EQ(0, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Going to background at app start doesn't log anything.
  scene.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  SimulateTransitionToCurrentStage();

  // Going foreground starts the session.
  scene.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Going to background stops the session.
  scene.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_ended_count());
}

TEST_F(AppMetricsAppStateAgentTest, CountSessionDurationMultiwindow) {
  SceneState* sceneA = [[SceneState alloc] initWithAppState:app_state_];
  SceneState* sceneB = [[SceneState alloc] initWithAppState:app_state_];
  app_state_.connectedScenes = @[ sceneA, sceneB ];
  [agent_ appState:app_state_ sceneConnected:sceneA];
  [agent_ appState:app_state_ sceneConnected:sceneB];

  EXPECT_EQ(0, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  SimulateTransitionToCurrentStage();

  // One scene is enough to start a session.
  sceneA.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Two scenes at the same time, still the session goes on.
  sceneB.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Only scene B in foreground, session still going.
  sceneA.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // No sessions in foreground, session is over.
  sceneB.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_ended_count());
}

TEST_F(AppMetricsAppStateAgentTest, CountSessionDurationSafeMode) {
  SceneState* scene = [[SceneState alloc] initWithAppState:app_state_];
  app_state_.connectedScenes = @[ scene ];
  app_state_.initStageForTesting = InitStageSafeMode;
  [agent_ appState:app_state_ sceneConnected:scene];

  EXPECT_EQ(0, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  SimulateTransitionToCurrentStage();

  // Going to background at app start doesn't log anything.
  scene.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Going foreground doesn't start the session while in safe mode.
  scene.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Session starts when safe mode completes.
  app_state_.initStageForTesting = GetMinimalInitStageThatAllowsLogging();
  SimulateTransitionToCurrentStage();
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(0, getProfileSessionDurationsService()->session_ended_count());

  // Going to background stops the session.
  scene.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_started_count());
  EXPECT_EQ(1, getProfileSessionDurationsService()->session_ended_count());
}

// Tests that -logStartupDuration: and -createStartupTrackingTask are called
// once and in the right order for a regular startup (no safe mode).
TEST_F(AppMetricsAppStateAgentTest, logStartupDuration) {
  id metricsMediator = [OCMockObject mockForClass:[MetricsMediator class]];

  [[metricsMediator expect] createStartupTrackingTask];
  [[metricsMediator expect] logStartupDuration:nil connectionInformation:nil];
  [metricsMediator setExpectationOrderMatters:YES];

  SceneState* sceneA = [[SceneState alloc] initWithAppState:app_state_];
  SceneState* sceneB = [[SceneState alloc] initWithAppState:app_state_];
  app_state_.connectedScenes = @[ sceneA, sceneB ];
  [agent_ appState:app_state_ sceneConnected:sceneA];
  [agent_ appState:app_state_ sceneConnected:sceneB];

  // Simulate transitioning to the current app init stage before scenes going on
  // the foreground.
  [agent_ appState:app_state_
      didTransitionFromInitStage:static_cast<InitStage>(app_state_.initStage -
                                                        1)];

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
TEST_F(AppMetricsAppStateAgentTest, logStartupDurationWhenSafeMode) {
  id metricsMediator = [OCMockObject mockForClass:[MetricsMediator class]];

  [[metricsMediator reject] createStartupTrackingTask];
  [[metricsMediator reject] logStartupDuration:nil connectionInformation:nil];

  app_state_.initStageForTesting = GetMaximalInitStageThatDontAllowLogging();

  SceneState* sceneA = [[SceneState alloc] initWithAppState:app_state_];
  app_state_.connectedScenes = @[ sceneA ];
  [agent_ appState:app_state_ sceneConnected:sceneA];

  // Simulate transitioning to the current app init stage before scenes going on
  // the foreground.
  [agent_ appState:app_state_
      didTransitionFromInitStage:static_cast<InitStage>(app_state_.initStage -
                                                        1)];

  // This would normally log startup information, but not when in safe mode.
  sceneA.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_OCMOCK_VERIFY(metricsMediator);
}
