// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_metrics_app_state_agent.h"

#import "base/test/task_environment.h"
#import "ios/chrome/app/application_delegate/app_init_stage_test_utils.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/metrics_mediator.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

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

    TestProfileIOS::Builder test_profile_builder;
    test_profile_builder.AddTestingFactory(
        IOSProfileSessionDurationsServiceFactory::GetInstance(),
        base::BindRepeating(&FakeProfileSessionDurationsService::Create));
    profile_ =
        profile_manager_.AddProfileWithBuilder(std::move(test_profile_builder));

    app_state_ = [[FakeAppState alloc] initWithStartupInformation:nil];
  }

  void SetUp() override {
    PlatformTest::SetUp();
    app_state_.initStageForTesting = GetMinimalInitStageThatAllowsLogging();
    [agent_ setAppState:app_state_];
  }

  FakeProfileSessionDurationsService* getProfileSessionDurationsService() {
    return static_cast<FakeProfileSessionDurationsService*>(
        IOSProfileSessionDurationsServiceFactory::GetForProfile(
            profile_.get()));
  }

  void SimulateTransitionToCurrentStage() {
    AppInitStage previousStage =
        app_state_.initStage == AppInitStage::kStart
            ? AppInitStage::kStart
            : PreviousAppInitStage(app_state_.initStage);
    [agent_ appState:app_state_ didTransitionFromInitStage:previousStage];
  }

  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  AppMetricsAppStateAgent* agent_;
  raw_ptr<ProfileIOS> profile_;
  FakeAppState* app_state_;
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
  app_state_.initStageForTesting = AppInitStage::kSafeMode;
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
TEST_F(AppMetricsAppStateAgentTest, logStartupDurationWhenSafeMode) {
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
