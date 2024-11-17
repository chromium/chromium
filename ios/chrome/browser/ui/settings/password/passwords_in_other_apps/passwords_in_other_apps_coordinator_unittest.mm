// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/scoped_password_settings_reauth_module_override.h"
#import "ios/chrome/browser/ui/settings/password/passwords_in_other_apps/passwords_in_other_apps_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_view_controller.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace password_manager {

// Test fixture for PasswordsInOtherAppsCoordinator.
class PasswordsInOtherAppsCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    // Create scene state for reauthentication coordinator.
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;

    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    mock_reauth_module_ = [[MockReauthenticationModule alloc] init];
    // Delay auth result so auth doesn't pass right after starting coordinator.
    // Needed for verifying behavior when auth is required.
    mock_reauth_module_.shouldSkipReAuth = NO;
    mock_reauth_module_.expectedResult = ReauthenticationResult::kSuccess;
    // Make coordinator use mock reauth module.
    scoped_reauth_override_ =
        ScopedPasswordSettingsReauthModuleOverride::MakeAndArmForTesting(
            mock_reauth_module_);

    navigation_controller_ = [[UINavigationController alloc] init];
    scoped_window_.Get().rootViewController = navigation_controller_;

    coordinator_ = [[PasswordsInOtherAppsCoordinator alloc]
        initWithBaseNavigationController:navigation_controller_
                                 browser:browser_.get()];

    [coordinator_ start];

    // Wait for presentation animation of the coordinator's view controller.
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::test::ios::kWaitForUIElementTimeout);
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

  // Whether Passwords in Other Apps was pushed in the
  // navigation controller.
  bool IsPasswordsInOtherAppPresented() {
    return [navigation_controller_.topViewController
        isKindOfClass:[PasswordsInOtherAppsViewController class]];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  SceneState* scene_state_;
  UINavigationController* navigation_controller_ = nil;
  ScopedKeyWindow scoped_window_;
  MockReauthenticationModule* mock_reauth_module_ = nil;
  std::unique_ptr<ScopedPasswordSettingsReauthModuleOverride>
      scoped_reauth_override_;
  PasswordsInOtherAppsCoordinator* coordinator_ = nil;
};

// Tests that Password in Other Apps is presented when coordinator is tarted.
TEST_F(PasswordsInOtherAppsCoordinatorTest, StartPresentsViewController) {
  ASSERT_TRUE(IsPasswordsInOtherAppPresented());
}

// Verifies that reauthentication is required after the scene goes to the
// background and back to foreground.
TEST_F(PasswordsInOtherAppsCoordinatorTest,
       ReauthenticationRequiredAfterSceneIsBackgrounded) {
  ASSERT_TRUE(IsPasswordsInOtherAppPresented());

  // Simulate scene going to the background.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;

  // Reauthentication view controller should be covering passwords in other
  // apps.
  ASSERT_FALSE(IsPasswordsInOtherAppPresented());

  scene_state_.activationLevel = SceneActivationLevelBackground;

  // Reauthentication view controller should still be covering passwords in
  // other apps.
  ASSERT_FALSE(IsPasswordsInOtherAppPresented());

  // Simulate scene going back to foreground.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Reauthentication view controller should be covering passwords in other apps
  // until auth is passed.
  ASSERT_FALSE(IsPasswordsInOtherAppPresented());

  // Successful auth should reveal passwords in other apps.
  [mock_reauth_module_ returnMockedReauthenticationResult];

  ASSERT_TRUE(IsPasswordsInOtherAppPresented());
}

}  // namespace password_manager
