// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/utils/utils.h"

#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_test_utils.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/ui_blocker_manager.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Test suite for `GetEligibleSceneForSyncedSetUp()`.
class SyncedSetUpUtilsTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    profile_state_ = [[ProfileState alloc] initWithAppState:nil];
    ASSERT_NSEQ(profile_state_.foregroundActiveScene, nil);
  }

  // Helper function to create and connect a scene with a specific activation
  // level.
  SceneState* ConnectSceneWithActivationLevel(SceneActivationLevel level) {
    SceneState* scene = [[SceneState alloc] initWithAppState:nil];
    scene.activationLevel = level;

    [profile_state_ sceneStateConnected:scene];

    if (level == SceneActivationLevelForegroundActive) {
      EXPECT_EQ(profile_state_.foregroundActiveScene, scene);
    } else {
      EXPECT_EQ(profile_state_.foregroundActiveScene, nil);
    }

    return scene;
  }

  ProfileState* profile_state_;
};

// Tests that the active scene is returned when all preconditions are met (main
// provider active, profile ready, no blockers).
TEST_F(SyncedSetUpUtilsTest, ReturnsActiveSceneWhenAllPreconditionsMet) {
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);

  SceneState* scene =
      ConnectSceneWithActivationLevel(SceneActivationLevelForegroundActive);

  SceneState* result = GetEligibleSceneForSyncedSetUp(profile_state_);

  EXPECT_EQ(result, scene);
}

// Tests that `nil` is returned if the input `ProfileState` is `null`.
TEST_F(SyncedSetUpUtilsTest, ReturnsNilWhenProfileStateIsNull) {
  SceneState* result = GetEligibleSceneForSyncedSetUp(nil);

  EXPECT_EQ(result, nil);
}

// Tests that `nil` is returned if the profile initialization is not complete.
TEST_F(SyncedSetUpUtilsTest, ReturnsNilWhenProfileIsNotFinalized) {
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kProfileLoaded);

  ConnectSceneWithActivationLevel(SceneActivationLevelForegroundActive);

  SceneState* result = GetEligibleSceneForSyncedSetUp(profile_state_);

  EXPECT_EQ(result, nil);
}

// Tests that `nil` is returned if a UI blocker is present.
TEST_F(SyncedSetUpUtilsTest, ReturnsNilWhenUIBlockerIsPresent) {
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);

  SceneState* scene =
      ConnectSceneWithActivationLevel(SceneActivationLevelForegroundActive);

  [profile_state_ incrementBlockingUICounterForTarget:scene];

  SceneState* result = GetEligibleSceneForSyncedSetUp(profile_state_);

  EXPECT_EQ(result, nil);
}

// Tests that `nil` is returned if there is no foreground active scene.
TEST_F(SyncedSetUpUtilsTest, ReturnsNilWhenNoForegroundActiveSceneExists) {
  SetProfileStateInitStage(profile_state_, ProfileInitStage::kFinal);

  ConnectSceneWithActivationLevel(SceneActivationLevelForegroundInactive);

  SceneState* result = GetEligibleSceneForSyncedSetUp(profile_state_);

  EXPECT_EQ(result, nil);
}
