// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/incognito_usage_app_state_agent.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_mock_clock_override.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface IncognitoUsageAppStateAgent (Testing)
- (void)applicationWillTerminate;
@property(nonatomic, assign) BOOL incognitoContentVisible;
@end

// A fake AppState that allows overriding connectedScenes.
@interface FakeAppState : AppState
@property(nonatomic, strong) NSArray<SceneState*>* connectedScenes;
@end

class IncognitoUsageAppStateAgentTest : public PlatformTest {
 public:
  void AdvanceClock(const base::TimeDelta& delay) {
    scoped_clock_.Advance(delay);
  }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    incognito_agent_ = [[IncognitoUsageAppStateAgent alloc] init];
    app_state_ = [[FakeAppState alloc] initWithStartupInformation:nil];

    scene_state1_ = [[SceneState alloc] initWithAppState:nil];
    scene_state1_.incognitoContentVisible = NO;
    scene_state1_.activationLevel = SceneActivationLevelBackground;

    scene_state2_ = [[SceneState alloc] initWithAppState:nil];
    scene_state2_.incognitoContentVisible = NO;
    scene_state2_.activationLevel = SceneActivationLevelBackground;

    app_state_.connectedScenes = @[ scene_state1_, scene_state2_ ];
    [incognito_agent_ setAppState:app_state_];
  }

  void TearDown() override {
    scene_state1_.incognitoContentVisible = NO;
    scene_state2_.incognitoContentVisible = NO;
    PlatformTest::TearDown();
  }

  base::HistogramTester histogram_tester_;
  base::ScopedMockClockOverride scoped_clock_;
  IncognitoUsageAppStateAgent* incognito_agent_;
  FakeAppState* app_state_;
  SceneState* scene_state1_;
  SceneState* scene_state2_;
};

// Tests metrics that a session of 1 minute is recorded.
TEST_F(IncognitoUsageAppStateAgentTest, NormalIncognitoSession) {
  // One scene Foregrounded, no Incognito.
  scene_state1_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  AdvanceClock(base::Minutes(1));

  // Display one incognito for 1 minute.
  scene_state1_.incognitoContentVisible = YES;
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);
  AdvanceClock(base::Minutes(1));

  // Back to normal.
  scene_state1_.incognitoContentVisible = NO;
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  // Metrics is still not logged.
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  // Back to incognito.
  AdvanceClock(base::Minutes(1));
  scene_state1_.incognitoContentVisible = YES;
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  // Metrics from previous time should be logged.
  histogram_tester_.ExpectUniqueTimeSample("IOS.Incognito.TimeSpent",
                                           base::Minutes(1), 1);
}

// Tests metrics that a session of 5 seconds is not recorded.
TEST_F(IncognitoUsageAppStateAgentTest, ShortIncognitoSession) {
  // One scene Foregrounded, no Incognito.
  scene_state1_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  AdvanceClock(base::Minutes(1));

  // Display one incognito for 5 seconds.
  scene_state1_.incognitoContentVisible = YES;
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);
  AdvanceClock(base::Seconds(5));

  // Back to normal.
  scene_state1_.incognitoContentVisible = NO;
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  // Metrics is still not logged.
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  // Back to incognito.
  AdvanceClock(base::Minutes(1));
  scene_state1_.incognitoContentVisible = YES;
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  // Metrics from previous time should not be logged.
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);
}

// Tests metrics that a short interruption of 5 seconds is not recorded.
TEST_F(IncognitoUsageAppStateAgentTest, ShortNormalSession) {
  // One scene Foregrounded, no Incognito.
  scene_state1_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  AdvanceClock(base::Minutes(1));

  // Display one incognito for 1 minute.
  scene_state1_.incognitoContentVisible = YES;
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);
  AdvanceClock(base::Minutes(1));

  // Back to normal.
  scene_state1_.incognitoContentVisible = NO;
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  // Metrics is still not logged.
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  // Back to incognito.
  AdvanceClock(base::Seconds(5));
  scene_state1_.incognitoContentVisible = YES;
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  // Nothing logged yet.
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  AdvanceClock(base::Seconds(55));
  scene_state1_.incognitoContentVisible = NO;
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  // Metrics is still not logged.
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  AdvanceClock(base::Minutes(1));
  scene_state1_.incognitoContentVisible = YES;
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  // Metrics from previous time should be logged.
  histogram_tester_.ExpectUniqueTimeSample("IOS.Incognito.TimeSpent",
                                           base::Minutes(2), 1);
}

// Tests metrics that the current incognito life time is reported.
TEST_F(IncognitoUsageAppStateAgentTest, ApplicationTerminatesInIncognito) {
  // One scene Foregrounded, no Incognito.
  scene_state1_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  AdvanceClock(base::Minutes(1));
  // Display one incognito for 1 minute.
  scene_state1_.incognitoContentVisible = YES;
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);
  AdvanceClock(base::Minutes(1));

  [incognito_agent_ applicationWillTerminate];
  // Metrics from previous time should be logged.
  histogram_tester_.ExpectUniqueTimeSample("IOS.Incognito.TimeSpent",
                                           base::Minutes(1), 1);
}

// Tests metrics that the last incognito life time is reported.
TEST_F(IncognitoUsageAppStateAgentTest, ApplicationTerminatesInNormal) {
  // One scene Foregrounded, no Incognito.
  scene_state1_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  AdvanceClock(base::Minutes(1));
  // Display one incognito for 1 minute.
  scene_state1_.incognitoContentVisible = YES;
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);
  AdvanceClock(base::Minutes(1));
  scene_state1_.incognitoContentVisible = NO;
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  [incognito_agent_ applicationWillTerminate];
  // Metrics from previous time should be logged.
  histogram_tester_.ExpectUniqueTimeSample("IOS.Incognito.TimeSpent",
                                           base::Minutes(1), 1);
}

// Tests incognitoContentVisible in various scenarios.
TEST_F(IncognitoUsageAppStateAgentTest, IncognitoContentVisibleValue) {
  // Two scenes background normal.
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);

  // Incognito in background.
  scene_state1_.incognitoContentVisible = YES;
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);

  // Foreground incognito.
  scene_state1_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);

  // Background it.
  scene_state1_.activationLevel = SceneActivationLevelBackground;
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);

  // Foreground incognito.
  scene_state1_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);

  // Switch to normal.
  scene_state1_.incognitoContentVisible = NO;
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);

  // Foreground second scene.
  scene_state2_.activationLevel = SceneActivationLevelForegroundActive;
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);

  // Incognito in foreground.
  scene_state1_.incognitoContentVisible = YES;
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);

  // Second Incognito in foreground.
  scene_state2_.incognitoContentVisible = YES;
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);

  // Background 1.
  scene_state1_.activationLevel = SceneActivationLevelBackground;
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);

  // Switch the other to normal.
  scene_state2_.incognitoContentVisible = NO;
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
}
