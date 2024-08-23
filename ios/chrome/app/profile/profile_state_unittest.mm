// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_state.h"

#import <optional>

#import "base/test/task_environment.h"
#import "base/types/cxx23_to_underlying.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/app/profile/test/test_profile_state_agent.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface ObserverForProfileStateTest : NSObject <ProfileStateObserver>

// Records the last ProfileInitStage that was recorded during the call to
// profileState:didTransitionToInitStage:fromInitStage: method.
@property(nonatomic, readonly) std::optional<ProfileInitStage> lastStage;

@end

@implementation ObserverForProfileStateTest

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextStage
               fromInitStage:(ProfileInitStage)fromStage {
  CHECK_EQ(base::to_underlying(fromStage) + 1, base::to_underlying(nextStage));
  _lastStage = nextStage;
}

@end

using ProfileStateTest = PlatformTest;

// Tests that a newly created ProfileState has no -browserState.
TEST_F(ProfileStateTest, initializer) {
  ProfileState* state = [[ProfileState alloc] init];
  EXPECT_EQ(state.browserState, nullptr);
}

// Tests that -browserState uses a weak pointer.
TEST_F(ProfileStateTest, browserState) {
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<TestChromeBrowserState> browser_state =
      TestChromeBrowserState::Builder().Build();

  ProfileState* state = [[ProfileState alloc] init];
  EXPECT_EQ(state.browserState, nullptr);

  state.browserState = browser_state.get();
  EXPECT_EQ(state.browserState, browser_state.get());

  // Destroy the BrowserState and check that the property becomes null.
  browser_state.reset();
  EXPECT_EQ(state.browserState, nullptr);
}

// Tests that initStage can be set and get correctly.
TEST_F(ProfileStateTest, initStages) {
  ProfileState* state = [[ProfileState alloc] init];
  state.initStage = ProfileInitStage::InitStageLoadProfile;
  while (state.initStage != ProfileInitStage::InitStageFinal) {
    const ProfileInitStage nextStage =
        static_cast<ProfileInitStage>(base::to_underlying(state.initStage) + 1);

    EXPECT_NE(state.initStage, nextStage);
    state.initStage = nextStage;
    EXPECT_EQ(state.initStage, nextStage);
  }
}

// Tests adding and removing profile state agents.
TEST_F(ProfileStateTest, connectedAgents) {
  ProfileState* state = [[ProfileState alloc] init];
  EXPECT_NSEQ(state.connectedAgents, @[]);

  [state addAgent:[[TestProfileStateAgent alloc] init]];
  [state addAgent:[[TestProfileStateAgent alloc] init]];

  EXPECT_EQ(state.connectedAgents.count, 2u);
  for (TestProfileStateAgent* agent in state.connectedAgents) {
    EXPECT_EQ(agent.profileState, state);
  }

  TestProfileStateAgent* firstAgent = state.connectedAgents.firstObject;
  [state removeAgent:firstAgent];
  EXPECT_EQ(state.connectedAgents.count, 1u);
  EXPECT_EQ(firstAgent.profileState, nil);
}

// Tests that observers are correctly invoked when the ProfileInitStage changes
// (and that they are called on registration if the current stage is not
// InitStageLoadProfile).
TEST_F(ProfileStateTest, observers) {
  ProfileState* state = [[ProfileState alloc] init];

  ObserverForProfileStateTest* observer1 =
      [[ObserverForProfileStateTest alloc] init];
  [state addObserver:observer1];

  // The ProfileState is still in InitStageLoadProfile, so the observer must
  // not have been notified yet.
  EXPECT_EQ(observer1.lastStage, std::nullopt);

  state.initStage = ProfileInitStage::InitStageProfileLoaded;
  EXPECT_EQ(observer1.lastStage, ProfileInitStage::InitStageProfileLoaded);

  ObserverForProfileStateTest* observer2 =
      [[ObserverForProfileStateTest alloc] init];
  [state addObserver:observer2];

  // As the ProfileState was not InitStageLoadProfile, the observer must have
  // been notified during -addObserver:
  EXPECT_EQ(observer2.lastStage, ProfileInitStage::InitStageProfileLoaded);
}
