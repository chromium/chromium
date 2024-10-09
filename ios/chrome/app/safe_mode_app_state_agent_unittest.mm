// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/safe_mode_app_state_agent.h"

#import "base/ios/block_types.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/app/application_delegate/app_init_stage_test_utils.h"
#import "ios/chrome/app/application_delegate/app_state+Testing.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/safe_mode_app_state_agent+private.h"
#import "ios/chrome/browser/safe_mode/ui_bundled/safe_mode_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
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
}

// Iterate through the init stages from `startInitStage` up to
// `initStageDestination`.
void IterateToStage(AppInitStage startInitStage,
                    AppInitStage initStageDestination,
                    SafeModeAppAgent* agent,
                    id appStateMock) {
  AppInitStage initStage = startInitStage;
  if (initStage == AppInitStage::kStart) {
    [appStateMock setInitStage:AppInitStage::kStart];
    [agent appState:appStateMock
        didTransitionFromInitStage:AppInitStage::kStart];
    initStage = NextAppInitStage(startInitStage);
  }

  AppInitStage prevInitStage = PreviousAppInitStage(initStage);
  while (initStage <= initStageDestination) {
    [appStateMock setInitStage:initStage];
    [agent appState:appStateMock didTransitionFromInitStage:prevInitStage];
    prevInitStage = initStage;
    initStage = NextAppInitStage(initStage);
  }
}

class SafeModeAppStateAgentTest : public BlockCleanupTest {
 protected:
  SafeModeAppStateAgentTest() {
    profile_ = TestProfileIOS::Builder().Build();
    window_ = [OCMockObject mockForClass:[UIWindow class]];
    startup_information_mock_ =
        [OCMockObject mockForProtocol:@protocol(StartupInformation)];
    connection_information_mock_ =
        [OCMockObject mockForProtocol:@protocol(ConnectionInformation)];
  }

  void swizzleSafeModeShouldStart(BOOL shouldStart) {
    safe_mode_swizzle_block_ = ^BOOL(id self) {
      return shouldStart;
    };
    safe_mode_swizzler_.reset(new ScopedBlockSwizzler(
        [SafeModeCoordinator class], @selector(shouldStart),
        safe_mode_swizzle_block_));
  }

  AppState* getAppStateWithMock() {
    if (!app_state_) {
      // The swizzle block needs the scene state before app_state is created,
      // but the scene state needs the app state. So this alloc before swizzling
      // and initiate after app state is created.
      main_scene_state_ = [FakeSceneState alloc];

      app_state_ = [[AppState alloc]
          initWithStartupInformation:startup_information_mock_];

      main_scene_state_ = [main_scene_state_ initWithAppState:app_state_
                                                      profile:profile_.get()];
      main_scene_state_.window = GetWindowMock();
    }
    return app_state_;
  }

  id GetWindowMock() { return window_; }

  FakeSceneState* GetSceneState() { return main_scene_state_; }

 private:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  AppState* app_state_;
  FakeSceneState* main_scene_state_;

  std::unique_ptr<ScopedBlockSwizzler> safe_mode_swizzler_;
  DecisionBlock safe_mode_swizzle_block_;

  id startup_information_mock_;
  id connection_information_mock_;
  id window_;
};

TEST_F(SafeModeAppStateAgentTest, startSafeMode) {
  id windowMock = GetWindowMock();
  [[windowMock expect] makeKeyAndVisible];
  [[[windowMock stub] andReturn:nil] rootViewController];
  [[windowMock stub] setRootViewController:[OCMArg any]];

  AppState* appState = getAppStateWithMock();

  swizzleSafeModeShouldStart(YES);

  SafeModeAppAgent* agent = [[SafeModeAppAgent alloc] init];
  [agent setAppState:appState];

  IterateToStage(AppInitStage::kStart, AppInitStage::kSafeMode, agent,
                 appState);

  SceneState* sceneState = GetSceneState();

  [agent sceneState:sceneState
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];
  // Second call that should be deduped.
  [agent sceneState:sceneState
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];

  EXPECT_OCMOCK_VERIFY(windowMock);

  // Exit safe mode.
  [agent coordinatorDidExitSafeMode:agent.safeModeCoordinator];

  EXPECT_EQ(nil, agent.safeModeCoordinator);
}

TEST_F(SafeModeAppStateAgentTest, dontStartSafeModeBecauseNotNeeded) {
  AppState* appState = getAppStateWithMock();
  id appStateMock = OCMPartialMock(appState);
  [[appStateMock expect] queueTransitionToNextInitStage];
  [[appStateMock expect] appState:appState
       didTransitionFromInitStage:AppInitStage::kSafeMode];

  swizzleSafeModeShouldStart(NO);

  SafeModeAppAgent* agent = [[SafeModeAppAgent alloc] init];
  [agent setAppState:appStateMock];

  IterateToStage(AppInitStage::kStart, AppInitStage::kSafeMode, agent,
                 appStateMock);

  [agent sceneState:GetSceneState()
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];

  EXPECT_OCMOCK_VERIFY(appStateMock);
}

TEST_F(SafeModeAppStateAgentTest, dontStartSafeModeBecauseNotActiveLevel) {
  AppState* appState = getAppStateWithMock();
  id appStateMock = OCMPartialMock(appState);
  [[appStateMock reject] queueTransitionToNextInitStage];

  swizzleSafeModeShouldStart(YES);

  SafeModeAppAgent* agent = [[SafeModeAppAgent alloc] init];
  [agent setAppState:appStateMock];

  IterateToStage(AppInitStage::kStart, AppInitStage::kSafeMode, agent,
                 appStateMock);

  [agent sceneState:GetSceneState()
      transitionedToActivationLevel:SceneActivationLevelForegroundInactive];

  EXPECT_OCMOCK_VERIFY(appStateMock);
}

TEST_F(SafeModeAppStateAgentTest,
       dontStartSafeModeBecauseFirstSceneHasAlreadyActivated) {
  AppState* appState = getAppStateWithMock();

  id appStateMock = OCMPartialMock(appState);
  [[appStateMock reject] queueTransitionToNextInitStage];

  swizzleSafeModeShouldStart(YES);

  SafeModeAppAgent* agent = [[SafeModeAppAgent alloc] init];
  [agent setAppState:appStateMock];

  IterateToStage(AppInitStage::kStart, AppInitStage::kSafeMode, agent,
                 appStateMock);

  agent.firstSceneHasActivated = YES;
  [agent sceneState:GetSceneState()
      transitionedToActivationLevel:SceneActivationLevelForegroundActive];

  EXPECT_OCMOCK_VERIFY(appStateMock);
}
