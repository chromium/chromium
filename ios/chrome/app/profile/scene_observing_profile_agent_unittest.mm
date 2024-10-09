// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/scene_observing_profile_agent.h"

#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface SampleSceneObservingProfileAgent : SceneObservingProfileAgent
@property(nonatomic, assign) BOOL notifiedSceneTransitioned;
@end

@implementation SampleSceneObservingProfileAgent

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  _notifiedSceneTransitioned = YES;
}

@end

using SceneObservingProfileAgentTest = PlatformTest;

// Tests that if a scene is connected while the ProfileState is ready then
// the agent is notified about the scene.
TEST_F(SceneObservingProfileAgentTest, sceneConnected) {
  ProfileState* profile_state = [[ProfileState alloc] initWithAppState:nil];
  SetProfileStateInitStage(profile_state, ProfileInitStage::kUIReady);

  SampleSceneObservingProfileAgent* agent =
      [[SampleSceneObservingProfileAgent alloc] init];
  [profile_state addAgent:agent];
  ASSERT_EQ(agent.profileState, profile_state);

  SceneState* scene_state = [[SceneState alloc] initWithAppState:nil];
  [profile_state sceneStateConnected:scene_state];

  scene_state.activationLevel = SceneActivationLevelBackground;
  EXPECT_TRUE(agent.notifiedSceneTransitioned);
}

// Tests that if a scene is connected while the ProfileState is not ready
// then the agent is not notified about the scene.
TEST_F(SceneObservingProfileAgentTest, sceneConnected_NotReady) {
  ProfileState* profile_state = [[ProfileState alloc] initWithAppState:nil];

  SampleSceneObservingProfileAgent* agent =
      [[SampleSceneObservingProfileAgent alloc] init];
  [profile_state addAgent:agent];
  ASSERT_EQ(agent.profileState, profile_state);

  SceneState* scene_state = [[SceneState alloc] initWithAppState:nil];
  [profile_state sceneStateConnected:scene_state];

  scene_state.activationLevel = SceneActivationLevelBackground;
  EXPECT_FALSE(agent.notifiedSceneTransitioned);
}
