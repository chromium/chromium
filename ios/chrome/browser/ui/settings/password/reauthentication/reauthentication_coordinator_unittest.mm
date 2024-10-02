// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_view_controller.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

@interface FakeReauthenticationCoordinatorDelegate
    : NSObject <ReauthenticationCoordinatorDelegate>

// Set when `successfulReauthenticationWithCoordinator` is called.
@property(nonatomic) BOOL successfulReauth;

// Set when `willPushReauthenticationViewController` is called.
@property(nonatomic) BOOL willPushReauthVCCalled;

// Set when `dismissUIAfterFailedReauthenticationWithCoordinator` is called.
@property(nonatomic) BOOL dismissUICalled;

@end

@implementation FakeReauthenticationCoordinatorDelegate

- (void)successfulReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  self.successfulReauth = YES;
}

- (void)willPushReauthenticationViewController {
  self.willPushReauthVCCalled = YES;
}

- (void)dismissUIAfterFailedReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  _dismissUICalled = YES;
}

@end

// Test fixture for ReauthenticationCoordinator.
class ReauthenticationCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;

    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);
    mocked_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mocked_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];

    // Init navigation controller with a root vc.
    base_navigation_controller_ = [[UINavigationController alloc]
        initWithRootViewController:[[UIViewController alloc] init]];
    mock_reauth_module_ = [[MockReauthenticationModule alloc] init];
    delegate_ = [[FakeReauthenticationCoordinatorDelegate alloc] init];
    coordinator_ = [[ReauthenticationCoordinator alloc]
        initWithBaseNavigationController:base_navigation_controller_
                                 browser:browser_.get()
                  reauthenticationModule:mock_reauth_module_
                             authOnStart:NO];
    coordinator_.delegate = delegate_;

    scoped_window_.Get().rootViewController = base_navigation_controller_;

    [coordinator_ start];
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

  // Verifies that the ReauthenticationViewController was pushed in the
  // navigation controller.
  void CheckReauthenticationViewControllerIsPresented() {
    // Check that reauth vc was pushed to navigation vc.
    ASSERT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForActionTimeout, true, ^bool() {
          return base_navigation_controller_.viewControllers.count == 2LU;
        }));
    ASSERT_TRUE([base_navigation_controller_.topViewController
        isKindOfClass:[ReauthenticationViewController class]]);
    EXPECT_TRUE(delegate_.willPushReauthVCCalled);
  }

  void CheckReauthenticationViewControllerNotPresented() {
    // Check that reauth vc is not in the navigation vc, only the root vc is
    // there.
    ASSERT_TRUE(
        WaitUntilConditionOrTimeout(kWaitForActionTimeout, true, ^bool() {
          return base_navigation_controller_.viewControllers.count == 1LU;
        }));
    ASSERT_FALSE([base_navigation_controller_.topViewController
        isKindOfClass:[ReauthenticationViewController class]]);
  }

  // Tests the auth flow works correctly when backgrounding/foregrounding the
  // scene.
  // - simulate_foreground_inactive: When false the test changes the scene state
  // to the background state without going through foreground inactive.
  void CheckReauthFlowAfterGoingToBackground(
      bool simulate_foreground_inactive) {
    CheckReauthenticationViewControllerNotPresented();

    if (simulate_foreground_inactive) {
      // Simulate transition to inactive state before background state.
      scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
      CheckReauthenticationViewControllerIsPresented();
    }

    // Simulate transition to background.
    scene_state_.activationLevel = SceneActivationLevelBackground;

    // Reauth vc should have been presented either in the inactive or background
    // state.
    CheckReauthenticationViewControllerIsPresented();
    ASSERT_FALSE(delegate_.successfulReauth);

    // Mock reauth result when app is in the foreground and active again.
    mock_reauth_module_.expectedResult = ReauthenticationResult::kSuccess;
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;

    // Reauth vc should be gone and delegate should be notified about the
    // successful auth.
    CheckReauthenticationViewControllerNotPresented();
    ASSERT_TRUE(delegate_.successfulReauth);
  }

  web::WebTaskEnvironment task_environment_;
  SceneState* scene_state_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_window_;
  UINavigationController* base_navigation_controller_ = nil;
  MockReauthenticationModule* mock_reauth_module_ = nil;
  FakeReauthenticationCoordinatorDelegate* delegate_ = nil;
  id mocked_application_commands_handler_;
  ReauthenticationCoordinator* coordinator_ = nil;
};

// Tests the auth flow works correctly when backgrounding/foregrounding the
// scene.
TEST_F(ReauthenticationCoordinatorTest, RequestAuthAfterSceneGoesToBackground) {
  CheckReauthFlowAfterGoingToBackground(/*simulate_foreground_inactive=*/true);
}

// Tests the auth flow works correctly when backgrounding/foregrounding the
// scene but skipping the foreground inactive state.
TEST_F(ReauthenticationCoordinatorTest,
       RequestAuthAfterSceneGoesToBackgroundSkippingInactive) {
  CheckReauthFlowAfterGoingToBackground(/*simulate_foreground_inactive=*/false);
}

// Tests the auth flow works correctly when backgrounding/foregrounding the
// scene multiple times. Done to verify the state of the coordinator after each
// auth flow due to scene state changes.
TEST_F(ReauthenticationCoordinatorTest,
       RequestAuthAfterSceneGoesToBackgroundRepeated) {
  CheckReauthFlowAfterGoingToBackground(/*simulate_foreground_inactive=*/true);

  delegate_.successfulReauth = NO;
  delegate_.willPushReauthVCCalled = NO;

  CheckReauthFlowAfterGoingToBackground(/*simulate_foreground_inactive=*/true);
}

// Tests that auth is not requested with scene goes to the foreground inactive
// state and back to foreground active.
TEST_F(ReauthenticationCoordinatorTest,
       AuthNotRequestedAfterSceneGoesToForegroundInactive) {
  CheckReauthenticationViewControllerNotPresented();
  ASSERT_FALSE(delegate_.successfulReauth);

  // Simulate start of transition to background state.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;

  CheckReauthenticationViewControllerIsPresented();

  // Back to foreground active should remove vc.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  CheckReauthenticationViewControllerNotPresented();
  ASSERT_FALSE(delegate_.successfulReauth);
}

// Tests that settings are closed after backgrounding/foregrounding the scene
// and canceling local authentication.
TEST_F(ReauthenticationCoordinatorTest,
       CancelAuthAfterSceneGoesToBackgroundDispatchesCommand) {
  CheckReauthenticationViewControllerNotPresented();

  // Simulate start of transition to background state.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;

  CheckReauthenticationViewControllerIsPresented();

  // Simulate transition to background.
  scene_state_.activationLevel = SceneActivationLevelBackground;

  // Reauth vc should still be there.
  CheckReauthenticationViewControllerIsPresented();
  ASSERT_FALSE(delegate_.successfulReauth);
  ASSERT_FALSE(delegate_.dismissUICalled);

  // Mock reauth result when app is in the foreground and active again.
  mock_reauth_module_.expectedResult = ReauthenticationResult::kFailure;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Reauth vc shouldn't be removed.
  CheckReauthenticationViewControllerIsPresented();

  // Cancelling reauth should close settings.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, true, ^bool() {
    return delegate_.dismissUICalled;
  }));

  ASSERT_FALSE(delegate_.successfulReauth);
}

// Tests that ReauthenticationCoordinator dismissed its view controller after a
// successful reauthentication before the scene is foregrounded.
TEST_F(ReauthenticationCoordinatorTest,
       ReauthViewControllerDismissedBeforeTheSceneIsForegrounded) {
  CheckReauthenticationViewControllerNotPresented();
  mock_reauth_module_.shouldSkipReAuth = NO;
  mock_reauth_module_.expectedResult = ReauthenticationResult::kSuccess;

  // Simulate transition to inactive state before background state.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  CheckReauthenticationViewControllerIsPresented();

  // Simulate transition to background.
  scene_state_.activationLevel = SceneActivationLevelBackground;
  CheckReauthenticationViewControllerIsPresented();
  ASSERT_FALSE(delegate_.successfulReauth);

  // Simulate transition to foreground. This will trigger a reauthentication
  // request.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;
  CheckReauthenticationViewControllerIsPresented();
  ASSERT_FALSE(delegate_.successfulReauth);

  // Transition back to background.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  CheckReauthenticationViewControllerIsPresented();
  scene_state_.activationLevel = SceneActivationLevelBackground;
  CheckReauthenticationViewControllerIsPresented();

  // Then back to foreground, delivering the reauthentication result before
  // reaching the foreground state.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  [mock_reauth_module_ returnMockedReauthenticationResult];
  ASSERT_TRUE(delegate_.successfulReauth);
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Reauth view controller should be gone.
  CheckReauthenticationViewControllerNotPresented();
}
