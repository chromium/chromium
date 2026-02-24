// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/session_metrics_profile_agent.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/run_loop.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "base/time/time_override.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

const char kActiveDays7Histogram[] = "IOS.PreviousActiveDays7";
const char kActiveDays14Histogram[] = "IOS.PreviousActiveDays14";
const char kActiveDays28Histogram[] = "IOS.PreviousActiveDays28";

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
    RegisterLocalStatePrefs(local_state_.registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state());

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
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
  }

  FakeProfileSessionDurationsService* GetProfileSessionDurationsService() {
    return static_cast<FakeProfileSessionDurationsService*>(
        IOSProfileSessionDurationsServiceFactory::GetForProfile(
            profile_.get()));
  }

  PrefService* local_state() { return &local_state_; }

  ProfileIOS* profile() { return profile_.get(); }

  ProfileState* profile_state() { return profile_state_; }

  static base::Time NowOverride() { return now_override_; }

  static void SetNowOverride(base::Time now_override) {
    now_override_ = now_override;
  }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  ProfileState* profile_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple local_state_;
  static base::Time now_override_;
};

base::Time SessionMetricsProfileAgentTest::now_override_;

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

// Tests that the number of recent active days is emitted on session start and
// end.
TEST_F(SessionMetricsProfileAgentTest, ActiveDaysRecordedOnSessionStateChange) {
  base::HistogramTester histogram_tester;
  SetProfileStateInitStage(profile_state(), ProfileInitStage::kUIReady);

  SceneState* scene = [[SceneState alloc] initWithAppState:nil];
  scene.profileState = profile_state();
  [profile_state() sceneStateConnected:scene];

  // The session starts when the scene enters the foreground. The number of
  // recent active days should be recorded if it hasn't been recorded today.
  local_state()->SetTime(prefs::kLastRecordedActiveDay, base::Time());
  scene.activationLevel = SceneActivationLevelForegroundInactive;

  // Wait for the Feature Engagement Tracker to call its initialization
  // callbacks.
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(profile());
  base::RunLoop run_loop;
  tracker->AddOnInitializedCallback(
      base::IgnoreArgs<bool>(run_loop.QuitClosure()));
  run_loop.Run();

  histogram_tester.ExpectTotalCount(kActiveDays7Histogram, 1);
  histogram_tester.ExpectTotalCount(kActiveDays14Histogram, 1);
  histogram_tester.ExpectTotalCount(kActiveDays28Histogram, 1);

  // Going to background stops the session. The number of recent active days
  // should be recorded if it hasn't been recorded today.
  local_state()->SetTime(prefs::kLastRecordedActiveDay, base::Time());
  scene.activationLevel = SceneActivationLevelBackground;

  // Wait for the Feature Engagement Tracker to call its initialization
  // callbacks.
  base::RunLoop run_loop2;
  tracker->AddOnInitializedCallback(
      base::IgnoreArgs<bool>(run_loop2.QuitClosure()));
  run_loop2.Run();

  histogram_tester.ExpectTotalCount(kActiveDays7Histogram, 2);
  histogram_tester.ExpectTotalCount(kActiveDays14Histogram, 2);
  histogram_tester.ExpectTotalCount(kActiveDays28Histogram, 2);
}

// Tests that the number of recent active days is emitted to at most once per
// day.
TEST_F(SessionMetricsProfileAgentTest, ActiveDaysRecordedOncePerDay) {
  base::subtle::ScopedTimeClockOverrides time_override(
      &SessionMetricsProfileAgentTest::NowOverride, nullptr, nullptr);
  base::HistogramTester histogram_tester;
  SetProfileStateInitStage(profile_state(), ProfileInitStage::kUIReady);

  SceneState* scene = [[SceneState alloc] initWithAppState:nil];
  scene.profileState = profile_state();
  [profile_state() sceneStateConnected:scene];

  // The session starts when the scene enters the foreground. The number of
  // recent active days should not be recorded if it has already been recorded
  // today.
  base::Time last_recorded_time;
  EXPECT_TRUE(base::Time::FromUTCString("2025-01-15", &last_recorded_time));
  last_recorded_time = last_recorded_time.LocalMidnight();
  local_state()->SetTime(prefs::kLastRecordedActiveDay, last_recorded_time);
  SetNowOverride(last_recorded_time + base::Hours(3));
  scene.activationLevel = SceneActivationLevelForegroundInactive;

  // Wait for the Feature Engagement Tracker to call its initialization
  // callbacks.
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(profile());
  base::RunLoop run_loop;
  tracker->AddOnInitializedCallback(
      base::IgnoreArgs<bool>(run_loop.QuitClosure()));
  run_loop.Run();

  histogram_tester.ExpectTotalCount(kActiveDays7Histogram, 0);
  histogram_tester.ExpectTotalCount(kActiveDays14Histogram, 0);
  histogram_tester.ExpectTotalCount(kActiveDays28Histogram, 0);
  EXPECT_EQ(local_state()->GetTime(prefs::kLastRecordedActiveDay),
            last_recorded_time);

  // Going to background stops the session. The number of recent active days
  // should be recorded if it has been more than a day since last recorded.
  base::Time next_day = last_recorded_time + base::Hours(30);
  SetNowOverride(next_day);
  scene.activationLevel = SceneActivationLevelBackground;

  // Wait for the Feature Engagement Tracker to call its initialization
  // callbacks.
  base::RunLoop run_loop2;
  tracker->AddOnInitializedCallback(
      base::IgnoreArgs<bool>(run_loop2.QuitClosure()));
  run_loop2.Run();

  histogram_tester.ExpectTotalCount(kActiveDays7Histogram, 1);
  histogram_tester.ExpectTotalCount(kActiveDays14Histogram, 1);
  histogram_tester.ExpectTotalCount(kActiveDays28Histogram, 1);
  next_day = next_day.LocalMidnight();
  EXPECT_EQ(local_state()->GetTime(prefs::kLastRecordedActiveDay), next_day);
}
