// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/incognito_usage_app_state_agent.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_mock_clock_override.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@interface IncognitoUsageAppStateAgent (Testing) <AppStateObserver,
                                                  SceneStateObserver>
- (void)applicationWillTerminate;
@property(nonatomic, assign) BOOL incognitoContentVisible;
@end

class IncognitoUsageAppStateAgentTest : public PlatformTest {
 public:
  void AdvanceClock(const base::TimeDelta& delay) {
    scoped_clock_.Advance(delay);
  }

  void SetScene1ActivationLevel(SceneActivationLevel activation_level) {
    scene1_activation_level_ = activation_level;
    [incognito_agent_ sceneState:mock_scene_state1_
        transitionedToActivationLevel:activation_level];
  }

  void SetScene2ActivationLevel(SceneActivationLevel activation_level) {
    scene2_activation_level_ = activation_level;
    [incognito_agent_ sceneState:mock_scene_state2_
        transitionedToActivationLevel:activation_level];
  }

  void SetScene1DisplaysIncognito(BOOL displays_incognito) {
    scene1_displays_incognito_ = displays_incognito;
    [incognito_agent_ sceneState:mock_scene_state1_
        isDisplayingIncognitoContent:displays_incognito];
  }
  void SetScene2DisplaysIncognito(BOOL displays_incognito) {
    scene2_displays_incognito_ = displays_incognito;
    [incognito_agent_ sceneState:mock_scene_state2_
        isDisplayingIncognitoContent:displays_incognito];
  }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    incognito_agent_ = [[IncognitoUsageAppStateAgent alloc] init];
    mock_app_state_ = OCMClassMock([AppState class]);
    mock_scene_state1_ = OCMClassMock([SceneState class]);
    mock_scene_state2_ = OCMClassMock([SceneState class]);

    NSArray* connected_scenes = @[ mock_scene_state1_, mock_scene_state2_ ];
    OCMStub([mock_app_state_ connectedScenes]).andReturn(connected_scenes);
    [incognito_agent_ setAppState:mock_app_state_];

    OCMStub([mock_scene_state1_ incognitoContentVisible])
        .andDo(^(NSInvocation* invocation) {
          [invocation setReturnValue:&scene1_displays_incognito_];
        });
    OCMStub([mock_scene_state2_ incognitoContentVisible])
        .andDo(^(NSInvocation* invocation) {
          [invocation setReturnValue:&scene2_displays_incognito_];
        });
    OCMStub([mock_scene_state1_ activationLevel])
        .andDo(^(NSInvocation* invocation) {
          [invocation setReturnValue:&scene1_activation_level_];
        });
    OCMStub([mock_scene_state2_ activationLevel])
        .andDo(^(NSInvocation* invocation) {
          [invocation setReturnValue:&scene2_activation_level_];
        });
    [incognito_agent_ appState:mock_app_state_
                sceneConnected:mock_scene_state1_];
    [incognito_agent_ appState:mock_app_state_
                sceneConnected:mock_scene_state2_];
  }

  void TearDown() override {
    SetScene1DisplaysIncognito(NO);
    SetScene2DisplaysIncognito(NO);
    PlatformTest::TearDown();
  }

  base::HistogramTester histogram_tester_;
  base::ScopedMockClockOverride scoped_clock_;
  IncognitoUsageAppStateAgent* incognito_agent_;
  id mock_app_state_;
  id mock_scene_state1_;
  id mock_scene_state2_;
  __block BOOL scene1_displays_incognito_ = NO;
  __block BOOL scene2_displays_incognito_ = NO;
  __block SceneActivationLevel scene1_activation_level_ =
      SceneActivationLevelBackground;
  __block SceneActivationLevel scene2_activation_level_ =
      SceneActivationLevelBackground;
};

// Tests metrics that a session of 1 minute is recorded
TEST_F(IncognitoUsageAppStateAgentTest, NormalIncognitoSession) {
  // One scene Foregrounded, no Incognito
  SetScene1ActivationLevel(SceneActivationLevelForegroundActive);

  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  AdvanceClock(base::Minutes(1));

  // Display one incognito for 1 minute
  SetScene1DisplaysIncognito(YES);
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);
  AdvanceClock(base::Minutes(1));

  // Back to normal
  SetScene1DisplaysIncognito(NO);
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  // Metrics is still not logged.
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  // Back to incognito
  AdvanceClock(base::Minutes(1));
  SetScene1DisplaysIncognito(YES);
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  // Metrics from previous time should be logged.
  histogram_tester_.ExpectUniqueTimeSample("IOS.Incognito.TimeSpent",
                                           base::Minutes(1), 1);
}

// Tests metrics that a session of 5 seconds is not recorded
TEST_F(IncognitoUsageAppStateAgentTest, ShortIncognitoSession) {
  // One scene Foregrounded, no Incognito
  SetScene1ActivationLevel(SceneActivationLevelForegroundActive);

  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  AdvanceClock(base::Minutes(1));

  // Display one incognito for 5 seconds
  SetScene1DisplaysIncognito(YES);
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);
  AdvanceClock(base::Seconds(5));

  // Back to normal
  SetScene1DisplaysIncognito(NO);
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  // Metrics is still not logged.
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  // Back to incognito
  AdvanceClock(base::Minutes(1));
  SetScene1DisplaysIncognito(YES);
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  // Metrics from previous time should not be logged.
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);
}

// Tests metrics that a short interruption of 5 seconds is not recorded.
TEST_F(IncognitoUsageAppStateAgentTest, ShortNormalSession) {
  // One scene Foregrounded, no Incognito
  SetScene1ActivationLevel(SceneActivationLevelForegroundActive);

  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  AdvanceClock(base::Minutes(1));

  // Display one incognito for 1 minute
  SetScene1DisplaysIncognito(YES);
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);
  AdvanceClock(base::Minutes(1));

  // Back to normal
  SetScene1DisplaysIncognito(NO);
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  // Metrics is still not logged.
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  // Back to incognito
  AdvanceClock(base::Seconds(5));
  SetScene1DisplaysIncognito(YES);
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  // Nothing logged yet
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  AdvanceClock(base::Seconds(55));
  SetScene1DisplaysIncognito(NO);
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  // Metrics is still not logged.
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  AdvanceClock(base::Minutes(1));
  SetScene1DisplaysIncognito(YES);
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  // Metrics from previous time should be logged.
  histogram_tester_.ExpectUniqueTimeSample("IOS.Incognito.TimeSpent",
                                           base::Minutes(2), 1);
}

// Tests metrics that the current incognito life time is reported.
TEST_F(IncognitoUsageAppStateAgentTest, ApplicationTerminatesInIncognito) {
  // One scene Foregrounded, no Incognito
  SetScene1ActivationLevel(SceneActivationLevelForegroundActive);

  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  AdvanceClock(base::Minutes(1));
  // Display one incognito for 1 minute
  SetScene1DisplaysIncognito(YES);
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
  // One scene Foregrounded, no Incognito
  SetScene1ActivationLevel(SceneActivationLevelForegroundActive);

  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);

  AdvanceClock(base::Minutes(1));
  // Display one incognito for 1 minute
  SetScene1DisplaysIncognito(YES);
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);
  histogram_tester_.ExpectTotalCount("IOS.Incognito.TimeSpent", 0);
  AdvanceClock(base::Minutes(1));
  SetScene1DisplaysIncognito(NO);
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
  SetScene1DisplaysIncognito(YES);
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);

  // Foreground incognito
  SetScene1ActivationLevel(SceneActivationLevelForegroundActive);
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);

  // Background it
  SetScene1ActivationLevel(SceneActivationLevelBackground);
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);

  // Foreground incognito
  SetScene1ActivationLevel(SceneActivationLevelForegroundActive);
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);

  // Switch to normal
  SetScene1DisplaysIncognito(NO);
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);

  // Foreground second scene
  SetScene2ActivationLevel(SceneActivationLevelForegroundActive);
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);

  // Incognito in foreground.
  SetScene1DisplaysIncognito(YES);
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);

  // Second Incognito in foreground.
  SetScene2DisplaysIncognito(YES);
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);

  // Background 1
  SetScene1ActivationLevel(SceneActivationLevelBackground);
  EXPECT_TRUE(incognito_agent_.incognitoContentVisible);

  // Switch the other to normal.
  SetScene2DisplaysIncognito(NO);
  EXPECT_FALSE(incognito_agent_.incognitoContentVisible);
}
