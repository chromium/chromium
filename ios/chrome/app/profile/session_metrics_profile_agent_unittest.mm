// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/session_metrics_profile_agent.h"

#import "base/functional/bind.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

// A test double for IOSProfileSessionDurationsService.
class FakeProfileSessionDurationsService
    : public IOSProfileSessionDurationsService {
 public:
  FakeProfileSessionDurationsService() = default;
  ~FakeProfileSessionDurationsService() override = default;

  // IOSProfileSessionDurationsService implementation.
  void OnSessionStarted(base::TimeTicks session_start) override {
    ++session_started_count_;
  }

  void OnSessionEnded(base::TimeDelta session_length) override {
    ++session_ended_count_;
  }

  bool IsSessionActive() override {
    return session_started_count_ > session_ended_count_;
  }

  // Accessor for the tests.
  int session_started_count() const { return session_started_count_; }
  int session_ended_count() const { return session_ended_count_; }

 private:
  int session_started_count_ = 0;
  int session_ended_count_ = 0;
};

// Create a fake IOSProfileSessionDurationsService.
std::unique_ptr<KeyedService> CreateFakeProfileSessionDurationsService(
    ProfileIOS* profile) {
  return std::make_unique<FakeProfileSessionDurationsService>();
}

}  // namespace

class SessionMetricsProfileAgentTest : public PlatformTest {
 public:
  SessionMetricsProfileAgentTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IOSProfileSessionDurationsServiceFactory::GetInstance(),
        base::BindOnce(&CreateFakeProfileSessionDurationsService));
    profile_ = std::move(builder).Build();

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    profile_state_.profile = profile_.get();

    [profile_state_ addAgent:[[SessionMetricsProfileAgent alloc] init]];
  }

  ~SessionMetricsProfileAgentTest() override {
    profile_state_.profile = nullptr;
  }

  FakeProfileSessionDurationsService* GetProfileSessionDurationsService() {
    return static_cast<FakeProfileSessionDurationsService*>(
        IOSProfileSessionDurationsServiceFactory::GetForProfile(
            profile_.get()));
  }

  ProfileState* profile_state() { return profile_state_; }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  ProfileState* profile_state_;
};

// Tests that the session is correctly recorded if the Scenes connects
// after the Profile initialisation has advanced enough.
TEST_F(SessionMetricsProfileAgentTest, OneSceneConnectedAfterProfileReady) {
  SetProfileStateInitStage(profile_state(), ProfileInitStage::kUIReady);

  SceneState* scene = [[SceneState alloc] initWithAppState:nil];
  scene.profileState = profile_state();
  [profile_state() sceneStateConnected:scene];

  FakeProfileSessionDurationsService* service =
      GetProfileSessionDurationsService();

  // The Scene is unattached by default, so the session should not be started.
  EXPECT_EQ(0, service->session_started_count());
  EXPECT_EQ(0, service->session_ended_count());

  // Going to background should not start the session.
  scene.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(0, service->session_started_count());
  EXPECT_EQ(0, service->session_ended_count());

  // The session starts when the scene enters the foreground.
  scene.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_EQ(1, service->session_started_count());
  EXPECT_EQ(0, service->session_ended_count());

  // Going to background stops the session.
  scene.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(1, service->session_started_count());
  EXPECT_EQ(1, service->session_ended_count());
}

// Tests that the session is correctly recorded if the Scenes connects
// before the Profile initialisation has advanced enough.
TEST_F(SessionMetricsProfileAgentTest, OneSceneConnectedBeforeProfileReady) {
  SceneState* scene = [[SceneState alloc] initWithAppState:nil];
  scene.profileState = profile_state();
  [profile_state() sceneStateConnected:scene];

  FakeProfileSessionDurationsService* service =
      GetProfileSessionDurationsService();

  // The Scene is unattached by default, so the session should not be started.
  EXPECT_EQ(0, service->session_started_count());
  EXPECT_EQ(0, service->session_ended_count());

  // Going to background should not start the session.
  scene.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(0, service->session_started_count());
  EXPECT_EQ(0, service->session_ended_count());

  // The session should not start even though the scene is in the foreground
  // because the profile is not ready yet.
  scene.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_EQ(0, service->session_started_count());
  EXPECT_EQ(0, service->session_ended_count());

  // Once the profile is ready, the session should be considered as started.
  SetProfileStateInitStage(profile_state(), ProfileInitStage::kUIReady);
  EXPECT_EQ(1, service->session_started_count());
  EXPECT_EQ(0, service->session_ended_count());

  // Going to background stops the session.
  scene.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(1, service->session_started_count());
  EXPECT_EQ(1, service->session_ended_count());
}

// Tests that the session is correctly recorded if there are multiple scenes
// that switch between foreground and background.
TEST_F(SessionMetricsProfileAgentTest, MultipleScenes) {
  SetProfileStateInitStage(profile_state(), ProfileInitStage::kUIReady);

  SceneState* scene1 = [[SceneState alloc] initWithAppState:nil];
  scene1.profileState = profile_state();
  [profile_state() sceneStateConnected:scene1];

  SceneState* scene2 = [[SceneState alloc] initWithAppState:nil];
  scene2.profileState = profile_state();
  [profile_state() sceneStateConnected:scene2];

  FakeProfileSessionDurationsService* service =
      GetProfileSessionDurationsService();

  // No scene in the foreground, the session is not started yet.
  EXPECT_EQ(0, service->session_started_count());
  EXPECT_EQ(0, service->session_ended_count());

  // The session starts as soon as one scene enter the foreground.
  scene1.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_EQ(1, service->session_started_count());
  EXPECT_EQ(0, service->session_ended_count());

  // The session should only start once, and not be considered as ended
  // as long as there is at least one scene in the foreground.
  scene2.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_EQ(1, service->session_started_count());
  EXPECT_EQ(0, service->session_ended_count());

  scene1.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(1, service->session_started_count());
  EXPECT_EQ(0, service->session_ended_count());

  scene1.activationLevel = SceneActivationLevelForegroundInactive;
  EXPECT_EQ(1, service->session_started_count());
  EXPECT_EQ(0, service->session_ended_count());

  // The sessions should end when all scene are in the background.
  scene1.activationLevel = SceneActivationLevelBackground;
  scene2.activationLevel = SceneActivationLevelBackground;
  EXPECT_EQ(1, service->session_started_count());
  EXPECT_EQ(1, service->session_ended_count());
}
