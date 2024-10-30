// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/safe_mode_app_state_agent.h"

#import "base/ios/block_types.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/app/application_delegate/app_init_stage_test_utils.h"
#import "ios/chrome/app/application_delegate/app_state+Testing.h"
#import "ios/chrome/app/safe_mode_app_state_agent+private.h"
#import "ios/chrome/browser/safe_mode/ui_bundled/safe_mode_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/fake_scene_state.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/testing/scoped_block_swizzler.h"
#import "ios/web/public/test/web_task_environment.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
// A block that takes self as argument and return a BOOL.
typedef BOOL (^DecisionBlock)(id self);
}  // namespace

class SafeModeAppStateAgentTest : public BlockCleanupTest {
 protected:
  SafeModeAppStateAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    window_ = [OCMockObject mockForClass:[UIWindow class]];

    AppState* app_state = [[AppState alloc] initWithStartupInformation:nil];
    app_state_mock_ = OCMPartialMock(app_state);

    main_scene_state_ =
        [[FakeSceneState alloc] initWithAppState:app_state
                                         profile:profile_.get()];
    main_scene_state_.window = window_;

    agent_ = [[SafeModeAppAgent alloc] init];
    [agent_ setAppState:app_state_mock_];
  }

  void SwizzleSafeModeShouldStart(BOOL shouldStart) {
    safe_mode_swizzle_block_ = ^BOOL(id self) {
      return shouldStart;
    };
    safe_mode_swizzler_.reset(new ScopedBlockSwizzler(
        [SafeModeCoordinator class], @selector(shouldStart),
        safe_mode_swizzle_block_));
  }

  // Iterate through the init stages from `curr_stage` up to
  // `dest_stage`.
  void IterateToStage(AppInitStage curr_stage, AppInitStage dest_stage) {
    DCHECK_GE(dest_stage, curr_stage);

    while (dest_stage != curr_stage) {
      const AppInitStage from_stage = curr_stage;
      curr_stage = NextAppInitStage(from_stage);
      [app_state_mock_ setInitStage:curr_stage];
      [agent_ appState:app_state_mock_ didTransitionFromInitStage:from_stage];
    }
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  id app_state_mock_;
  FakeSceneState* main_scene_state_;
  SafeModeAppAgent* agent_;

  std::unique_ptr<ScopedBlockSwizzler> safe_mode_swizzler_;
  DecisionBlock safe_mode_swizzle_block_;

  id window_;
};

TEST_F(SafeModeAppStateAgentTest, startSafeMode) {
  [[window_ expect] makeKeyAndVisible];
  [[[window_ stub] andReturn:nil] rootViewController];
  [[window_ stub] setRootViewController:[OCMArg any]];

  SwizzleSafeModeShouldStart(YES);

  IterateToStage(AppInitStage::kStart, AppInitStage::kSafeMode);

  [agent_ sceneState:main_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  // Second call that should be deduped.
  [agent_ sceneState:main_scene_state_
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];

  EXPECT_OCMOCK_VERIFY(window_);

  // Exit safe mode.
  [agent_ coordinatorDidExitSafeMode:agent_.safeModeCoordinator];

  EXPECT_EQ(nil, agent_.safeModeCoordinator);
}

TEST_F(SafeModeAppStateAgentTest, dontStartSafeModeBecauseNotNeeded) {
  [[app_state_mock_ expect] queueTransitionToNextInitStage];

  SwizzleSafeModeShouldStart(NO);
  IterateToStage(AppInitStage::kStart, AppInitStage::kSafeMode);

  main_scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_OCMOCK_VERIFY(app_state_mock_);
}

TEST_F(SafeModeAppStateAgentTest, dontStartSafeModeBecauseNotActiveLevel) {
  [[app_state_mock_ reject] queueTransitionToNextInitStage];

  SwizzleSafeModeShouldStart(YES);
  IterateToStage(AppInitStage::kStart, AppInitStage::kSafeMode);

  main_scene_state_.activationLevel = SceneActivationLevelForegroundInactive;

  EXPECT_OCMOCK_VERIFY(app_state_mock_);
}

TEST_F(SafeModeAppStateAgentTest,
       dontStartSafeModeBecauseFirstSceneHasAlreadyActivated) {
  [[app_state_mock_ reject] queueTransitionToNextInitStage];

  SwizzleSafeModeShouldStart(YES);
  IterateToStage(AppInitStage::kStart, AppInitStage::kSafeMode);

  agent_.firstSceneHasActivated = YES;
  main_scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  EXPECT_OCMOCK_VERIFY(app_state_mock_);
}
