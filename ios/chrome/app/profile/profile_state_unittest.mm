// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_state.h"

#import "base/test/task_environment.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/test/test_profile_state_agent.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

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
  state.initStage = ProfileInitStage::InitStageFinal;
  for (ProfileInitStage stage = ProfileInitStage::InitStageLoadProfile;
       stage < ProfileInitStage::InitStageFinal;
       stage = static_cast<ProfileInitStage>(static_cast<int>(stage) + 1)) {
    EXPECT_NE(state.initStage, stage);
    state.initStage = stage;
    EXPECT_EQ(state.initStage, stage);
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
