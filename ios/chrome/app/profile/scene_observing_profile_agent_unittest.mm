// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/scene_observing_profile_agent.h"

#import "base/types/cxx23_to_underlying.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

// Increment the ProfileState's initStage to `init_stage`. It is an error
// to call this method with an `init_stage` that is smaller than the actual
// ProfileState's initStage.
void ProgressInitStage(ProfileState* state, ProfileInitStage init_stage) {
  ProfileInitStage curr_stage = state.initStage;
  DCHECK_GE(init_stage, curr_stage);

  while (init_stage != curr_stage) {
    ProfileInitStage next_stage =
        static_cast<ProfileInitStage>(base::to_underlying(curr_stage) + 1);
    state.initStage = next_stage;
    curr_stage = next_stage;
  }
}

}  // namespace

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
  AppState* app_state = OCMClassMock([AppState class]);
  ProfileState* profile_state = [[ProfileState alloc] init];
  ProgressInitStage(profile_state, ProfileInitStage::InitStageUIReady);

  SampleSceneObservingProfileAgent* agent =
      [[SampleSceneObservingProfileAgent alloc] init];
  [profile_state addAgent:agent];
  ASSERT_EQ(agent.profileState, profile_state);

  SceneState* scene_state = [[SceneState alloc] initWithAppState:app_state];
  [profile_state sceneStateConnected:scene_state];

  scene_state.activationLevel = SceneActivationLevelBackground;
  EXPECT_TRUE(agent.notifiedSceneTransitioned);
}

// Tests that if a scene is connected while the ProfileState is not ready
// then the agent is not notified about the scene.
TEST_F(SceneObservingProfileAgentTest, sceneConnected_NotReady) {
  AppState* app_state = OCMClassMock([AppState class]);
  ProfileState* profile_state = [[ProfileState alloc] init];

  SampleSceneObservingProfileAgent* agent =
      [[SampleSceneObservingProfileAgent alloc] init];
  [profile_state addAgent:agent];
  ASSERT_EQ(agent.profileState, profile_state);

  SceneState* scene_state = [[SceneState alloc] initWithAppState:app_state];
  [profile_state sceneStateConnected:scene_state];

  scene_state.activationLevel = SceneActivationLevelBackground;
  EXPECT_FALSE(agent.notifiedSceneTransitioned);
}
