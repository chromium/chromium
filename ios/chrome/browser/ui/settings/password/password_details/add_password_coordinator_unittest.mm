// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/add_password_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_details/add_password_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/scoped_password_settings_reauth_module_override.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using password_manager::PasswordCheckReferrer;

namespace password_manager {

// Test fixture for AddPasswordCoordinatorTest.
class AddPasswordCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    // Add test password store. Used by the mediator.
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));

    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));

    // Create scene state for reauthentication coordinator.
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;

    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    // Mock ApplicationCommands. Since ApplicationCommands conforms to
    // SettingsCommands, it must be mocked as well.
    mocked_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mocked_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    id mocked_application_settings_command_handler =
        OCMProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mocked_application_settings_command_handler
                     forProtocol:@protocol(SettingsCommands)];

    // Init root vc.
    base_view_controller_ = [[UIViewController alloc] init];
    mock_reauth_module_ = [[MockReauthenticationModule alloc] init];
    // Delay auth result so auth doesn't pass right after starting coordinator.
    // Needed for verifying behavior when auth is required.
    mock_reauth_module_.shouldSkipReAuth = NO;
    mock_reauth_module_.expectedResult = ReauthenticationResult::kSuccess;

    // Replace reauthentication module with mock implementation for testing.
    scoped_reauth_module_override_ =
        ScopedPasswordSettingsReauthModuleOverride::MakeAndArmForTesting(
            mock_reauth_module_);

    coordinator_ = [[AddPasswordCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()];

    scoped_window_.Get().rootViewController = base_view_controller_;

    [coordinator_ start];

    // Wait for presentation animation of the coordinator's view controller.
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::test::ios::kWaitForUIElementTimeout);
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

  // Helper returning the top view controller in the navigation controller
  // presented by AddPasswordCoordinator.
  UIViewController* GetTopViewController() {
    UINavigationController* navigation_controller =
        base::apple::ObjCCastStrict<UINavigationController>(
            base_view_controller_.presentedViewController);
    DCHECK(navigation_controller);
    return navigation_controller.topViewController;
  }

  // Verifies that the AddPasswordViewController is the top view controller in
  // the navigation controller.
  void CheckAddPasswordIsTopViewController() {
    ASSERT_TRUE([GetTopViewController()
        isKindOfClass:[AddPasswordViewController class]]);
  }

  // Verifies that the AddPasswordViewController is not on top of the
  // navigation controller.
  void CheckAddPasswordIsNotTopViewController() {
    ASSERT_FALSE([GetTopViewController()
        isKindOfClass:[AddPasswordViewController class]]);
  }

  web::WebTaskEnvironment task_environment_;
  SceneState* scene_state_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_window_;
  std::unique_ptr<ScopedPasswordSettingsReauthModuleOverride>
      scoped_reauth_module_override_;
  UIViewController* base_view_controller_ = nil;
  MockReauthenticationModule* mock_reauth_module_ = nil;
  base::test::ScopedFeatureList scoped_feature_list_;
  id mocked_application_commands_handler_;
  base::HistogramTester histogram_tester_;
  AddPasswordCoordinator* coordinator_ = nil;
};

// Verifies that AddPasswordViewController is presented when the coordinator is
// started.
TEST_F(AddPasswordCoordinatorTest, StartPresentsViewController) {
  CheckAddPasswordIsTopViewController();

  // Verify visit metric was logged.
  histogram_tester_.ExpectUniqueSample(
      /*name=*/password_manager::kPasswordManagerSurfaceVisitHistogramName,
      /*sample=*/password_manager::PasswordManagerSurface::kAddPassword,
      /*count=*/1);
}

// Verifies that reauthentication is required after the scene goes to the
// background and back to foreground.
TEST_F(AddPasswordCoordinatorTest,
       ReauthenticationRequiredAfterSceneIsBackgrounded) {
  CheckAddPasswordIsTopViewController();

  // Simulate scene going to the background.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;

  // Reauthentication view controller should be covering add password.
  CheckAddPasswordIsNotTopViewController();

  scene_state_.activationLevel = SceneActivationLevelBackground;

  // Reauthentication view controller should be covering add password.
  CheckAddPasswordIsNotTopViewController();

  // Simulate scene going back to foreground.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  // Reauthentication view controller should be covering add password until auth
  // is passed.
  CheckAddPasswordIsNotTopViewController();

  // Successful auth should reveal add password.
  [mock_reauth_module_ returnMockedReauthenticationResult];

  CheckAddPasswordIsTopViewController();
}

}  // namespace password_manager
