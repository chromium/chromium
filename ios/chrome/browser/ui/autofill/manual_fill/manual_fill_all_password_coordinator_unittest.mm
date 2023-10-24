// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_all_password_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/scoped_password_settings_reauth_module_override.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_view_controller.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using base::test::ScopedFeatureList;

// Test fixture for ManualFillAllPasswordCoordinator.
class ManualFillAllPasswordCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kIOSPasswordAuthOnEntryV2);

    TestChromeBrowserState::Builder builder;

    // Mediator dependencies.
    // Add test password store.
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));

    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));

    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    // Add fake web state.
    auto fake_web_state = std::make_unique<web::FakeWebState>();
    browser_->GetWebStateList()->InsertWebState(
        WebStateList::kInvalidIndex, std::move(fake_web_state),
        WebStateList::INSERT_ACTIVATE, WebStateOpener());

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

    mock_reauth_module_ = [[MockReauthenticationModule alloc] init];
    // Delay auth result so auth doesn't pass right after starting coordinator.
    // Needed for verifying behavior when auth is required.
    mock_reauth_module_.shouldReturnSynchronously = NO;
    mock_reauth_module_.expectedResult = ReauthenticationResult::kSuccess;
    // Make coordinator use mock reauth module.
    scoped_reauth_override_ =
        ScopedPasswordSettingsReauthModuleOverride::MakeAndArmForTesting(
            mock_reauth_module_);

    root_view_controller_ = [[UIViewController alloc] init];
    scoped_window_.Get().rootViewController = root_view_controller_;

    coordinator_ = [[ManualFillAllPasswordCoordinator alloc]
        initWithBaseViewController:root_view_controller_
                           browser:browser_.get()
                  injectionHandler:nil];

    // Create scene state for reauthentication coordinator.
    scene_state_ = [[SceneState alloc] initWithAppState:nil];
    scene_state_.activationLevel = SceneActivationLevelForegroundActive;
    SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);
  }

  void TearDown() override {
    [coordinator_ stop];
    PlatformTest::TearDown();
  }

  // Starts the coordinator.
  void StartCoordinator() {
    [coordinator_ start];

    // Wait for presentation animation of the coordinator's view controller.
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::test::ios::kWaitForUIElementTimeout);
  }

  // Whether the view controller with the password list was pushed in the
  // navigation controller.
  bool ArePasswordsVisible() {
    UINavigationController* navigation_controller =
        base::apple::ObjCCastStrict<UINavigationController>(
            root_view_controller_.presentedViewController);

    return [navigation_controller.topViewController
        isKindOfClass:[PasswordViewController class]];
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  SceneState* scene_state_;
  UIViewController* root_view_controller_;
  ScopedKeyWindow scoped_window_;
  MockReauthenticationModule* mock_reauth_module_ = nil;
  std::unique_ptr<ScopedPasswordSettingsReauthModuleOverride>
      scoped_reauth_override_;
  ScopedFeatureList scoped_feature_list_;
  id mocked_application_commands_handler_;
  ManualFillAllPasswordCoordinator* coordinator_ = nil;
};

// Tests that passwords are revealed only after passing authentication.
TEST_F(ManualFillAllPasswordCoordinatorTest,
       PasswordSettingsPresentedWithAuth) {
  StartCoordinator();

  // Passwords should be covered until auth is passed.
  ASSERT_FALSE(ArePasswordsVisible());

  [mock_reauth_module_ returnMockedReauthenticationResult];

  // Successful auth should leave Passwords visible.
  ASSERT_TRUE(ArePasswordsVisible());
}

// Tests that passwords are revealed without authentication when the feature
// requiring auth is disabled.
TEST_F(ManualFillAllPasswordCoordinatorTest,
       PasswordSettingsPresentedWithoutAuth) {
  ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      password_manager::features::kIOSPasswordAuthOnEntryV2);

  StartCoordinator();

  // Passwords should be visible.
  ASSERT_TRUE(ArePasswordsVisible());
}
