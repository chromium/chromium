// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

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
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/scoped_password_settings_reauth_module_override.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_view_controller.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/testing/protocol_fake.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace password_manager {

// Test fixture for PasswordSettingsCoordinator.
class PasswordSettingsCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kIOSPasswordAuthOnEntryV2);

    TestChromeBrowserState::Builder builder;
    // Add test password store. Used by the mediator.
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));

    browser_state_ = builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());

    NSArray<Protocol*>* command_protocols = @[
      @protocol(ApplicationCommands), @protocol(BrowserCommands),
      @protocol(BrowsingDataCommands), @protocol(ApplicationSettingsCommands),
      @protocol(SnackbarCommands)
    ];
    fake_command_endpoint_ =
        [[ProtocolFake alloc] initWithProtocols:command_protocols];
    for (Protocol* protocol in command_protocols) {
      [browser_->GetCommandDispatcher()
          startDispatchingToTarget:fake_command_endpoint_
                       forProtocol:protocol];
    }

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

    coordinator_ = [[PasswordSettingsCoordinator alloc]
        initWithBaseViewController:root_view_controller_
                           browser:browser_.get()];

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
  //  - skip_auth_on_start: Whether to skip local authentication when the
  //  coordinator is started.
  void StartCoordinatorSkippingAuth(BOOL skip_auth_on_start) {
    coordinator_.skipAuthenticationOnStart = skip_auth_on_start;

    [coordinator_ start];

    // Wait for presentation animation of the coordinator's view controller.
    base::test::ios::SpinRunLoopWithMaxDelay(
        base::test::ios::kWaitForUIElementTimeout);
  }

  // Whether PasswordSettingsViewController was pushed in the
  // navigation controller.
  bool IsPasswordSettingsPresented() {
    UINavigationController* navigation_controller =
        base::apple::ObjCCastStrict<UINavigationController>(
            root_view_controller_.presentedViewController);

    return [navigation_controller.topViewController
        isKindOfClass:[PasswordSettingsViewController class]];
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
  base::test::ScopedFeatureList scoped_feature_list_;
  PasswordSettingsCoordinator* coordinator_ = nil;
  ProtocolFake* fake_command_endpoint_ = nil;
};

// Tests that Password Settings is presented without authentication required.
TEST_F(PasswordSettingsCoordinatorTest, PasswordSettingsPresentedWithoutAuth) {
  StartCoordinatorSkippingAuth(/*skip_auth_on_start=*/YES);

  ASSERT_TRUE(IsPasswordSettingsPresented());
}

// Tests that Password Settings is presented only after passing authentication
TEST_F(PasswordSettingsCoordinatorTest, PasswordSettingsPresentedWithAuth) {
  StartCoordinatorSkippingAuth(/*skip_auth_on_start=*/NO);

  // Password Settings should be covered until auth is passed.
  ASSERT_FALSE(IsPasswordSettingsPresented());

  [mock_reauth_module_ returnMockedReauthenticationResult];

  // Successful auth should leave Password Settings visible.
  ASSERT_TRUE(IsPasswordSettingsPresented());
}

}  // namespace password_manager
