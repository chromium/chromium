// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_state.h"

#import <optional>

#import "base/test/task_environment.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/app/profile/test/test_profile_state_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
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

// Tests that a newly created ProfileState has no -profile.
TEST_F(ProfileStateTest, initializer) {
  ProfileState* state = [[ProfileState alloc] initWithAppState:nil];
  EXPECT_EQ(state.profile, nullptr);
}

// Tests that -profile uses a weak pointer.
TEST_F(ProfileStateTest, profile) {
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  ProfileState* state = [[ProfileState alloc] initWithAppState:nil];
  EXPECT_EQ(state.profile, nullptr);

  state.profile = profile.get();
  EXPECT_EQ(state.profile, profile.get());

  // Destroy the profile and check that the property becomes null.
  profile.reset();
  EXPECT_EQ(state.profile, nullptr);
}

// Tests that initStage can be set and get correctly.
TEST_F(ProfileStateTest, initStages) {
  ProfileState* state = [[ProfileState alloc] initWithAppState:nil];
  state.initStage = ProfileInitStage::kStart;
  while (state.initStage != ProfileInitStage::kFinal) {
    const ProfileInitStage nextStage =
        static_cast<ProfileInitStage>(base::to_underlying(state.initStage) + 1);

    EXPECT_NE(state.initStage, nextStage);
    state.initStage = nextStage;
    EXPECT_EQ(state.initStage, nextStage);
  }
}

// Tests adding and removing profile state agents.
TEST_F(ProfileStateTest, connectedAgents) {
  ProfileState* state = [[ProfileState alloc] initWithAppState:nil];
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
  ProfileState* state = [[ProfileState alloc] initWithAppState:nil];

  ObserverForProfileStateTest* observer1 =
      [[ObserverForProfileStateTest alloc] init];
  [state addObserver:observer1];

  // The ProfileState is still in kStart, so the observer must not have been
  // notified yet.
  EXPECT_EQ(observer1.lastStage, std::nullopt);

  SetProfileStateInitStage(state, ProfileInitStage::kLoadProfile);
  EXPECT_EQ(observer1.lastStage, ProfileInitStage::kLoadProfile);

  ObserverForProfileStateTest* observer2 =
      [[ObserverForProfileStateTest alloc] init];
  [state addObserver:observer2];

  // As the ProfileState was not kStart, the observer must have been notified
  // during -addObserver:
  EXPECT_EQ(observer2.lastStage, ProfileInitStage::kLoadProfile);
}

// Tests that -connectedSceneStates returns all the connected scenes if the
// init stage is at least ProfileInitStage::kUIReady or nil.
TEST_F(ProfileStateTest, connectedSceneStates) {
  ProfileState* state = [[ProfileState alloc] initWithAppState:nil];
  ASSERT_NSEQ(state.connectedScenes, nil);

  // Connect a scene before reaching the kUIReady stage. The scene must not be
  // returned by -connectedScenes.
  SceneState* scene1 = [[SceneState alloc] initWithAppState:nil];
  [state sceneStateConnected:scene1];
  EXPECT_NSEQ(state.connectedScenes, nil);

  // Progress the init stage to kUIReady, the scene should be returned now.
  SetProfileStateInitStage(state, ProfileInitStage::kUIReady);
  EXPECT_NSEQ(state.connectedScenes, @[ scene1 ]);

  // Connect another scene. It should be visible in -connectedScenes.
  SceneState* scene2 = [[SceneState alloc] initWithAppState:nil];
  [state sceneStateConnected:scene2];

  EXPECT_NSEQ(state.connectedScenes, (@[ scene1, scene2 ]));
}

// Tests that scenes are removed from -connectedScenes when they reach the
// SceneActivationLevelDisconnected activation level.
TEST_F(ProfileStateTest, connectedSceneStates_scenesRemovedWhenDisconnected) {
  ProfileState* state = [[ProfileState alloc] initWithAppState:nil];
  ASSERT_NSEQ(state.connectedScenes, nil);

  // Connect a scene before reaching the kUIReady stage. The scene must not be
  // returned by -connectedScenes.
  SceneState* scene1 = [[SceneState alloc] initWithAppState:nil];
  [state sceneStateConnected:scene1];
  EXPECT_NSEQ(state.connectedScenes, nil);

  // Progress the init stage to kUIReady, the scene should be returned now.
  SetProfileStateInitStage(state, ProfileInitStage::kUIReady);
  EXPECT_NSEQ(state.connectedScenes, @[ scene1 ]);

  // Pretend the scene transitioned to SceneActivationLevelDisconnected, it
  // should no longer be returned by -connectedScenes.
  scene1.activationLevel = SceneActivationLevelDisconnected;
  EXPECT_NSEQ(state.connectedScenes, @[]);
}

// Tests that -foregroundActiveScene returns the first foreground active scene
// when the stage is at least ProfileInitStage::kUIReady or nil.
TEST_F(ProfileStateTest, foregroundActiveScene) {
  ProfileState* state = [[ProfileState alloc] initWithAppState:nil];
  ASSERT_NSEQ(state.foregroundActiveScene, nil);

  // Connect two scenes, one that is in foreground and active, before the
  // stage kUIReady. No scene should be returned by -foregroundActiveScene.
  SceneState* scene1 = [[SceneState alloc] initWithAppState:nil];
  [state sceneStateConnected:scene1];

  SceneState* scene2 = [[SceneState alloc] initWithAppState:nil];
  scene2.activationLevel = SceneActivationLevelForegroundActive;
  [state sceneStateConnected:scene2];

  EXPECT_NSEQ(state.foregroundActiveScene, nil);

  // The -foregroundActiveScene should not return scene2.
  SetProfileStateInitStage(state, ProfileInitStage::kUIReady);
  EXPECT_NSEQ(state.foregroundActiveScene, scene2);

  // Pretend that scene1 became foreground active. It should now be returned
  // by -foregroundActiveScene (since it is before in the -connectedScenes).
  // This is here to check that the scene returned may change over time.
  scene1.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_NSEQ(state.foregroundActiveScene, scene1);
}

// Tests that -foregroundScenes returns all the foreground (but maybe unactive)
// scenes when the stage is at least ProfileInitStage::kUIReady or nil
TEST_F(ProfileStateTest, foregroundScenes) {
  ProfileState* state = [[ProfileState alloc] initWithAppState:nil];
  ASSERT_NSEQ(state.foregroundActiveScene, nil);

  // Connect three scenes, one that is in the foreground and active, one that
  // is in foreground but inactive, and one that is in the background. No scene
  // should be returned by -foregroundScenes.
  SceneState* scene1 = [[SceneState alloc] initWithAppState:nil];
  [state sceneStateConnected:scene1];

  SceneState* scene2 = [[SceneState alloc] initWithAppState:nil];
  scene2.activationLevel = SceneActivationLevelForegroundActive;
  [state sceneStateConnected:scene2];

  SceneState* scene3 = [[SceneState alloc] initWithAppState:nil];
  scene3.activationLevel = SceneActivationLevelForegroundInactive;
  [state sceneStateConnected:scene3];

  EXPECT_NSEQ(state.foregroundScenes, nil);

  // The -foregroundScenes should now contains both scene2 and scene3.
  SetProfileStateInitStage(state, ProfileInitStage::kUIReady);
  EXPECT_NSEQ(state.foregroundScenes, (@[ scene2, scene3 ]));
}
