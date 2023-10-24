// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/password_manager/core/browser/affiliation/fake_affiliation_service.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_checkup/password_checkup_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_view_controller.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using password_manager::PasswordCheckReferrer;

namespace password_manager {

// Test fixture for PasswordCheckupCoordinator.
class PasswordCheckupCoordinatorTest
    : public PlatformTest,
      public testing::WithParamInterface<PasswordCheckReferrer> {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kIOSPasswordAuthOnEntryV2);

    TestChromeBrowserState::Builder builder;
    // Add test password store and affiliation service. Used by the mediator.
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));
    builder.AddTestingFactory(
        IOSChromeAffiliationServiceFactory::GetInstance(),
        base::BindRepeating(base::BindLambdaForTesting([](web::BrowserState*) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<password_manager::FakeAffiliationService>());
        })));

    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    // Mock ApplicationCommands. Since ApplicationCommands conforms to
    // ApplicationSettingsCommands, it must be mocked as well.
    mocked_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mocked_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    id mocked_application_settings_command_handler =
        OCMProtocolMock(@protocol(ApplicationSettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mocked_application_settings_command_handler
                     forProtocol:@protocol(ApplicationSettingsCommands)];

    // Init navigation controller with a root vc.
    base_navigation_controller_ = [[UINavigationController alloc]
        initWithRootViewController:[[UIViewController alloc] init]];
    mock_reauth_module_ = [[MockReauthenticationModule alloc] init];
    // Delay auth result so auth doesn't pass right after starting coordinator.
    // Needed for verifying behavior when auth is required.
    mock_reauth_module_.shouldReturnSynchronously = NO;
    mock_reauth_module_.expectedResult = ReauthenticationResult::kSuccess;

    coordinator_ = [[PasswordCheckupCoordinator alloc]
        initWithBaseNavigationController:base_navigation_controller_
                                 browser:browser_.get()
                            reauthModule:mock_reauth_module_
                                referrer:GetParam()];

    // Create scene state for reauthentication coordinator.
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);

    scoped_window_.Get().rootViewController = base_navigation_controller_;

    [coordinator_ start];

    // Wait for presentation animation of the coordinator's view controller.
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::test::ios::kWaitForUIElementTimeout);
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

  // Verifies that the PasswordCheckupViewController was pushed in the
  // navigation controller.
  void CheckPasswordCheckupIsPresented() {
    ASSERT_TRUE([base_navigation_controller_.topViewController
        isKindOfClass:[PasswordCheckupViewController class]]);
  }

  // Verifies that the PasswordCheckupViewController is not on top of the
  // navigation controller.
  void CheckPasswordCheckupIsNotPresented() {
    ASSERT_FALSE([base_navigation_controller_.topViewController
        isKindOfClass:[PasswordCheckupViewController class]]);
  }

  web::WebTaskEnvironment task_environment_;
  SceneState* scene_state_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  ScopedKeyWindow scoped_window_;
  UINavigationController* base_navigation_controller_ = nil;
  MockReauthenticationModule* mock_reauth_module_ = nil;
  base::test::ScopedFeatureList scoped_feature_list_;
  id mocked_application_commands_handler_;
  PasswordCheckupCoordinator* coordinator_ = nil;
};

// Test fixture for tests opening Password Checkup without authentication
// required.
class PasswordCheckupCoordinatorWithoutReauthenticationTest
    : public PasswordCheckupCoordinatorTest {};

// Test fixture for tests opening Password Checkup with authentication required.
class PasswordCheckupCoordinatorWithReauthenticationTest
    : public PasswordCheckupCoordinatorTest {};

// Tests that Password Checkup is presented without authentication required.
TEST_P(PasswordCheckupCoordinatorWithoutReauthenticationTest,
       PasswordCheckupPresentedWithoutAuth) {
  CheckPasswordCheckupIsPresented();
}

// Tests that Password Check is presented only after passing authentication
TEST_P(PasswordCheckupCoordinatorWithReauthenticationTest,
       PasswordCheckupPresentedWithAuth) {
  // Checkup should be covered until auth is passed.
  CheckPasswordCheckupIsNotPresented();

  [mock_reauth_module_ returnMockedReauthenticationResult];

  // Successful auth should leave Checkup visible.
  CheckPasswordCheckupIsPresented();
}

// Test Password Checkup entry points that do not require authentication.
INSTANTIATE_TEST_SUITE_P(
    ,  // Empty instantiation name.
    PasswordCheckupCoordinatorWithoutReauthenticationTest,
    ::testing::Values(PasswordCheckReferrer::kPasswordSettings));

// Test Password Checkup entry points that require authentication.
INSTANTIATE_TEST_SUITE_P(
    ,  // Empty instantiation name.
    PasswordCheckupCoordinatorWithReauthenticationTest,
    ::testing::Values(PasswordCheckReferrer::kPhishGuardDialog,
                      PasswordCheckReferrer::kSafetyCheck,
                      PasswordCheckReferrer::kPasswordBreachDialog,
                      PasswordCheckReferrer::kMoreToFixBubble,
                      PasswordCheckReferrer::kSafetyCheckMagicStack));

}  // namespace password_manager
