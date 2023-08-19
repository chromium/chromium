// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_view_controller.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

@interface FakeReauthenticationCoordinatorDelegate
    : NSObject <ReauthenticationCoordinatorDelegate>

// Set when `successfulReauthenticationWithCoordinator` is called.
@property(nonatomic) BOOL successfulReauth;

// Set when `willPushReauthenticationViewController` is called.
@property(nonatomic) BOOL willPushReauthVCCalled;

@end

@implementation FakeReauthenticationCoordinatorDelegate

- (void)successfulReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  self.successfulReauth = YES;
}

- (void)willPushReauthenticationViewController {
  self.willPushReauthVCCalled = YES;
}

@end

// Test fixture for ReauthenticationCoordinator.
class ReauthenticationCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kIOSPasswordAuthOnEntryV2);

    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
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

    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);

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
    ASSERT_EQ(base_navigation_controller_.viewControllers.count, 2LU);
    ASSERT_TRUE([base_navigation_controller_.topViewController
        isKindOfClass:[ReauthenticationViewController class]]);
    EXPECT_TRUE(delegate_.willPushReauthVCCalled);
  }

  void CheckReauthenticationViewControllerNotPresented() {
    // Check that reauth vc is not in the navigation vc, only the root vc is
    // there.
    ASSERT_EQ(base_navigation_controller_.viewControllers.count, 1LU);
    ASSERT_FALSE([base_navigation_controller_.topViewController
        isKindOfClass:[ReauthenticationViewController class]]);
  }

  // Tests the auth flow works correctly when backgrounding/foregrounding the
  // scene.
  void CheckReauthFlowAfterGoingToBackground() {
    CheckReauthenticationViewControllerNotPresented();

    // Simulate start of transition to background state.
    scene_state_.activationLevel = SceneActivationLevelForegroundInactive;

    CheckReauthenticationViewControllerIsPresented();

    // Simulate transition to background.
    scene_state_.activationLevel = SceneActivationLevelBackground;

    // Reauth vc should still be there.
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
  std::unique_ptr<ChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_window_;
  UINavigationController* base_navigation_controller_ = nil;
  MockReauthenticationModule* mock_reauth_module_ = nil;
  FakeReauthenticationCoordinatorDelegate* delegate_ = nil;
  id mocked_application_commands_handler_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ReauthenticationCoordinator* coordinator_ = nil;
};

// Tests the auth flow works correctly when backgrounding/foregrounding the
// scene.
TEST_F(ReauthenticationCoordinatorTest, RequestAuthAfterSceneGoesToBackground) {
  CheckReauthFlowAfterGoingToBackground();
}

// Tests the auth flow works correctly when backgrounding/foregrounding the
// scene multiple times. Done to verify the state of the coordinator after each
// auth flow due to scene state changes.
TEST_F(ReauthenticationCoordinatorTest,
       RequestAuthAfterSceneGoesToBackgroundRepeated) {
  CheckReauthFlowAfterGoingToBackground();

  delegate_.successfulReauth = NO;
  delegate_.willPushReauthVCCalled = NO;

  CheckReauthFlowAfterGoingToBackground();
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

// Tests that the set passcode alert is presented after
// backgrounding/foregrounding the scene without a passcode.
TEST_F(ReauthenticationCoordinatorTest,
       SetPasscodeRequestedAfterSceneGoesToBackground) {
  CheckReauthenticationViewControllerNotPresented();

  // Simulate start of transition to background state.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;

  // Wait for presentation of ReauthenticationViewController to finish.
  // Otherwise its view will not get loaded and the AlertCoordinator will not
  // present its alert. See AlertCoordinator start.
  base::test::ios::SpinRunLoopWithMaxDelay(
      base::test::ios::kWaitForUIElementTimeout);

  CheckReauthenticationViewControllerIsPresented();

  // Simulate transition to background.
  scene_state_.activationLevel = SceneActivationLevelBackground;

  // Reauth vc should still be there.
  CheckReauthenticationViewControllerIsPresented();
  ASSERT_FALSE(delegate_.successfulReauth);

  // Mock no local authentication available.
  mock_reauth_module_.canAttempt = NO;

  // Back to foreground active should present an alert.
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  CheckReauthenticationViewControllerIsPresented();

  ASSERT_TRUE(
      [base_navigation_controller_.topViewController.presentedViewController
          isKindOfClass:[UIAlertController class]]);

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

  // Cancelling reauth should close settings.
  OCMExpect([mocked_application_commands_handler_ closeSettingsUI]);

  // Mock reauth result when app is in the foreground and active again.
  mock_reauth_module_.expectedResult = ReauthenticationResult::kFailure;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Reauth vc shouldn't be removed.
  CheckReauthenticationViewControllerIsPresented();

  ASSERT_FALSE(delegate_.successfulReauth);

  // Verify command was dispatched.
  EXPECT_OCMOCK_VERIFY(mocked_application_commands_handler_);
}
