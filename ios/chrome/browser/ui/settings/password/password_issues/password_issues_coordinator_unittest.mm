// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/memory/scoped_refptr.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/metrics/ios_password_manager_metrics.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_issues/password_issues_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_view_controller.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace password_manager {

// Test fixture for PasswordIssuesCoordinator.
class PasswordIssuesCoordinatorTest : public PlatformTest {
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

    // Keep a scoped reference to IOSChromePasswordCheckManager until the test
    // finishes, otherwise it gets destroyed as soon as PasswordIssuesMediator
    // init goes out of scope.
    password_check_manager_ =
        IOSChromePasswordCheckManagerFactory::GetForProfile(profile_.get());

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

    // Init navigation controller with a root vc.
    base_navigation_controller_ = [[UINavigationController alloc]
        initWithRootViewController:[[UIViewController alloc] init]];
    mock_reauth_module_ = [[MockReauthenticationModule alloc] init];
    // Delay auth result so auth doesn't pass right after starting coordinator.
    // Needed for verifying behavior when auth is required.
    mock_reauth_module_.shouldSkipReAuth = NO;
    mock_reauth_module_.expectedResult = ReauthenticationResult::kSuccess;

    coordinator_ = [[PasswordIssuesCoordinator alloc]
              initForWarningType:password_manager::WarningType::
                                     kCompromisedPasswordsWarning
        baseNavigationController:base_navigation_controller_
                         browser:browser_.get()];

    coordinator_.reauthModule = mock_reauth_module_;

    scoped_window_.Get().rootViewController = base_navigation_controller_;
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

  // Verifies that the PasswordIssuesTableViewController was pushed in the
  // navigation controller.
  void CheckPasswordIssuesIsPresented() {
    ASSERT_TRUE([base_navigation_controller_.topViewController
        isKindOfClass:[PasswordIssuesTableViewController class]]);
  }

  // Verifies that the PasswordIssuesTableViewController is not on top of the
  // navigation controller.
  void CheckPasswordIssuesIsNotPresented() {
    ASSERT_FALSE([base_navigation_controller_.topViewController
        isKindOfClass:[PasswordIssuesTableViewController class]]);
  }

  // Verifies that a given number of password issues visits have been recorded.
  void CheckPasswordIssuesVisitMetricsCount(int count) {
    histogram_tester_.ExpectUniqueSample(
        /*name=*/password_manager::kPasswordManagerSurfaceVisitHistogramName,
        /*sample=*/password_manager::PasswordManagerSurface::kPasswordIssues,
        /*count=*/count);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  // IOSChromePasswordCheckManager must be destroyed before browser otherwise it
  // crashes when unregistering itself as an observer of a keyed service.
  scoped_refptr<IOSChromePasswordCheckManager> password_check_manager_;
  SceneState* scene_state_;
  ScopedKeyWindow scoped_window_;
  UINavigationController* base_navigation_controller_ = nil;
  MockReauthenticationModule* mock_reauth_module_ = nil;
  id mocked_application_commands_handler_;
  base::HistogramTester histogram_tester_;
  PasswordIssuesCoordinator* coordinator_ = nil;
};

// Tests that Password Issues is presented only after passing authentication
TEST_F(PasswordIssuesCoordinatorTest, PasswordIssuesPresentedWithAuth) {
  StartCoordinatorSkippingAuth(/*skip_auth_on_start=*/NO);

  // Password Issues should be covered until auth is passed.
  CheckPasswordIssuesIsNotPresented();

  // No visits logged until successful auth.
  CheckPasswordIssuesVisitMetricsCount(0);

  [mock_reauth_module_ returnMockedReauthenticationResult];

  // Successful auth should leave Password Issues visible and record visit.
  CheckPasswordIssuesIsPresented();

  CheckPasswordIssuesVisitMetricsCount(1);
}

// Tests that Password Issues visits are only logged once after the first
// successful authentication.
TEST_F(PasswordIssuesCoordinatorTest, PasswordIssuesVisitRecordedOnlyOnce) {
  StartCoordinatorSkippingAuth(/*skip_auth_on_start=*/NO);

  // Password Issues should be covered until auth is passed.
  CheckPasswordIssuesIsNotPresented();

  // No visits logged until successful auth.
  CheckPasswordIssuesVisitMetricsCount(0);

  [mock_reauth_module_ returnMockedReauthenticationResult];
  // Visit should be recorded after passing auth.
  CheckPasswordIssuesVisitMetricsCount(1);

  // Simulate scene transitioning to the background and back to foreground. This
  // should trigger an auth request.
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  scene_state_.activationLevel = SceneActivationLevelBackground;
  scene_state_.activationLevel = SceneActivationLevelForegroundInactive;
  scene_state_.activationLevel = SceneActivationLevelForegroundActive;

  [mock_reauth_module_ returnMockedReauthenticationResult];

  // Validate no new visits were recorded.
  CheckPasswordIssuesVisitMetricsCount(1);
}

}  // namespace password_manager
