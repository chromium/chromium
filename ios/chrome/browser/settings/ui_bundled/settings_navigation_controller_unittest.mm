// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/password_store/test_password_store.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_browser_agent.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/metrics/model/activity_reporter.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/settings/manage_accounts/public/manage_accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/protocol_fake.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

NSString* const kSpdyProxyEnabled = @"SpdyProxyEnabled";

using testing::ReturnRef;

class SettingsNavigationControllerTest : public PlatformTest {
 protected:
  SettingsNavigationControllerTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeProfilePasswordStoreFactory::GetInstance(),
        base::BindOnce(&password_manager::BuildPasswordStore<
                       password_manager::TestPasswordStore>));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindOnce(&CreateTestSyncService));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    DiscoverFeedVisibilityBrowserAgent::CreateForBrowser(browser_.get());

    NSArray<Protocol*>* command_protocols = @[
      @protocol(SceneCommands), @protocol(SettingsCommands),
      @protocol(SnackbarCommands), @protocol(PopupMenuCommands)
    ];
    fake_command_endpoint_ =
        [[ProtocolFake alloc] initWithProtocols:command_protocols];
    for (Protocol* protocol in command_protocols) {
      [browser_->GetCommandDispatcher()
          startDispatchingToTarget:fake_command_endpoint_
                       forProtocol:protocol];
    }

    mockDelegate_ = [OCMockObject
        niceMockForProtocol:@protocol(SettingsNavigationControllerDelegate)];

    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    template_url_service->Load();

    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    initialValueForSpdyProxyEnabled_ =
        [[defaults stringForKey:kSpdyProxyEnabled] copy];
    [defaults setObject:@"Disabled" forKey:kSpdyProxyEnabled];
  }

  ~SettingsNavigationControllerTest() override {
    if (initialValueForSpdyProxyEnabled_) {
      [[NSUserDefaults standardUserDefaults]
          setObject:initialValueForSpdyProxyEnabled_
             forKey:kSpdyProxyEnabled];
    } else {
      [[NSUserDefaults standardUserDefaults]
          removeObjectForKey:kSpdyProxyEnabled];
    }
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
  std::unique_ptr<Browser> browser_;
  id mockDelegate_;
  NSString* initialValueForSpdyProxyEnabled_;
  ProtocolFake* fake_command_endpoint_;
};

// When navigation stack has more than one view controller,
// -popViewControllerAnimated: successfully removes the top view controller.
TEST_F(SettingsNavigationControllerTest, PopController) {
  SettingsNavigationController* settingsController =
      [SettingsNavigationController
          mainSettingsControllerForBrowser:browser_.get()
                                  delegate:nil
                  hasDefaultBrowserBlueDot:NO];
  UIViewController* viewController =
      [[UIViewController alloc] initWithNibName:nil bundle:nil];
  [settingsController pushViewController:viewController animated:NO];
  EXPECT_EQ(2U, [[settingsController viewControllers] count]);

  UIViewController* poppedViewController =
      [settingsController popViewControllerAnimated:NO];
  EXPECT_NSEQ(viewController, poppedViewController);
  EXPECT_EQ(1U, [[settingsController viewControllers] count]);
  [settingsController cleanUpSettings];
}

// When the navigation stack has only one view controller,
// -popViewControllerAnimated: returns false.
TEST_F(SettingsNavigationControllerTest, DontPopRootController) {
  SettingsNavigationController* settingsController =
      [SettingsNavigationController
          mainSettingsControllerForBrowser:browser_.get()
                                  delegate:nil
                  hasDefaultBrowserBlueDot:NO];
  EXPECT_EQ(1U, [[settingsController viewControllers] count]);

  EXPECT_FALSE([settingsController popViewControllerAnimated:NO]);
  [settingsController cleanUpSettings];
}

// When the settings navigation stack has more than one view controller, calling
// -popViewControllerOrCloseSettingsAnimated: pops the top view controller to
// reveal the view controller underneath.
TEST_F(SettingsNavigationControllerTest,
       PopWhenNavigationStackSizeIsGreaterThanOne) {
  SettingsNavigationController* settingsController =
      [SettingsNavigationController
          mainSettingsControllerForBrowser:browser_.get()
                                  delegate:mockDelegate_
                  hasDefaultBrowserBlueDot:NO];
  UIViewController* viewController =
      [[UIViewController alloc] initWithNibName:nil bundle:nil];
  [settingsController pushViewController:viewController animated:NO];
  EXPECT_EQ(2U, [[settingsController viewControllers] count]);
  [[mockDelegate_ reject] closeSettings];
  [settingsController popViewControllerOrCloseSettingsAnimated:NO];
  EXPECT_EQ(1U, [[settingsController viewControllers] count]);
  EXPECT_OCMOCK_VERIFY(mockDelegate_);
  [settingsController cleanUpSettings];
}

// When the settings navigation stack only has one view controller, calling
// -popViewControllerOrCloseSettingsAnimated: calls -closeSettings on the
// delegate.
TEST_F(SettingsNavigationControllerTest,
       CloseSettingsWhenNavigationStackSizeIsOne) {
  base::UserActionTester user_action_tester;
  SettingsNavigationController* settingsController =
      [SettingsNavigationController
          mainSettingsControllerForBrowser:browser_.get()
                                  delegate:mockDelegate_
                  hasDefaultBrowserBlueDot:NO];
  EXPECT_EQ(1U, [[settingsController viewControllers] count]);
  [[mockDelegate_ expect] closeSettings];
  ASSERT_EQ(0, user_action_tester.GetActionCount("MobileSettingsClose"));
  [settingsController popViewControllerOrCloseSettingsAnimated:NO];
  EXPECT_EQ(1, user_action_tester.GetActionCount("MobileSettingsClose"));
  EXPECT_OCMOCK_VERIFY(mockDelegate_);
  [settingsController cleanUpSettings];
}

// Checks that metrics are correctly reported.
TEST_F(SettingsNavigationControllerTest, Metrics) {
  base::UserActionTester user_action_tester;
  SettingsNavigationController* settingsController =
      [SettingsNavigationController
          mainSettingsControllerForBrowser:browser_.get()
                                  delegate:mockDelegate_
                  hasDefaultBrowserBlueDot:NO];
  ASSERT_EQ(user_action_tester.GetActionCount(kMobileKeyCommandClose), 0);

  [settingsController keyCommand_close];

  EXPECT_EQ(user_action_tester.GetActionCount(kMobileKeyCommandClose), 1);
  [settingsController cleanUpSettings];
}

TEST_F(SettingsNavigationControllerTest, ActivityReporting) {
  SettingsNavigationController* settingsController =
      [SettingsNavigationController
          mainSettingsControllerForBrowser:browser_.get()
                                  delegate:nil
                  hasDefaultBrowserBlueDot:NO];

  id mockInstance = OCMClassMock([ActivityReporter class]);
  [settingsController setValue:mockInstance forKey:@"activityReporter"];

  OCMExpect([mockInstance reportActive]);
  [settingsController viewWillAppear:NO];
  [mockInstance verify];

  OCMExpect([mockInstance reportInactive]);
  [settingsController viewDidDisappear:NO];
  [mockInstance verify];

  [settingsController cleanUpSettings];
  [settingsController setValue:nil forKey:@"activityReporter"];
  [mockInstance stopMocking];
}

// Tests that `-cleanUpSettings` on an accounts controller created with
// `closeSettingsOnAddAccount` set to `YES` stops `ManageAccountsCoordinator`
// without calling `-closeSettings` on the delegate.
TEST_F(SettingsNavigationControllerTest,
       CleanUpAccountsControllerDoesNotCloseSettings) {
  // Add an identity on the device and sign in with it.
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(fake_identity);
  AuthenticationServiceFactory::GetForProfile(profile_.get())
      ->SignIn(fake_identity, signin_metrics::AccessPoint::kSettings);

  // The strict mock fails the test if any delegate method is called.
  id strict_delegate =
      OCMStrictProtocolMock(@protocol(SettingsNavigationControllerDelegate));
  @autoreleasepool {
    UIViewController* base_view_controller = [[UIViewController alloc] init];
    SettingsNavigationController* settings_controller =
        [SettingsNavigationController
                   accountsControllerForBrowser:browser_.get()
                             baseViewController:base_view_controller
                                       delegate:strict_delegate
                      closeSettingsOnAddAccount:YES
                              showSignoutButton:YES
                                 showDoneButton:NO
            signoutDismissalByParentCoordinator:NO];
    EXPECT_EQ(1U, settings_controller.viewControllers.count);

    // Simulate the settings being cleaned up.
    [settings_controller cleanUpSettings];
  }
  EXPECT_OCMOCK_VERIFY(strict_delegate);
}

// Tests that tapping the Done button on an accounts controller notifies the
// delegate to close settings once without recursing when the delegate calls
// `-cleanUpSettings`.
TEST_F(SettingsNavigationControllerTest,
       CloseAccountsControllerFromDoneButtonWithoutRecursion) {
  base::UserActionTester user_action_tester;
  // Add an identity on the device and sign in with it.
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(fake_identity);
  AuthenticationServiceFactory::GetForProfile(profile_.get())
      ->SignIn(fake_identity, signin_metrics::AccessPoint::kSettings);

  id strict_delegate =
      OCMStrictProtocolMock(@protocol(SettingsNavigationControllerDelegate));
  @autoreleasepool {
    UIViewController* base_view_controller = [[UIViewController alloc] init];
    SettingsNavigationController* settings_controller =
        [SettingsNavigationController
                   accountsControllerForBrowser:browser_.get()
                             baseViewController:base_view_controller
                                       delegate:strict_delegate
                      closeSettingsOnAddAccount:NO
                              showSignoutButton:NO
                                 showDoneButton:YES
            signoutDismissalByParentCoordinator:YES];
    __weak SettingsNavigationController* weak_settings_controller =
        settings_controller;
    ASSERT_EQ(1U, settings_controller.viewControllers.count);

    UIBarButtonItem* done_button =
        settings_controller.topViewController.navigationItem.rightBarButtonItem;
    ASSERT_NE(nil, done_button);
    EXPECT_NSEQ(kSettingsAccountsTableViewDoneButtonId,
                done_button.accessibilityIdentifier);

    OCMExpect([strict_delegate closeSettings])
        .andDo(^(NSInvocation* invocation) {
          // Simulate the delegate cleaning up settings while closing them.
          [weak_settings_controller cleanUpSettings];
        });

    // Simulate the user tapping the Done button.
    [UIApplication.sharedApplication sendAction:done_button.action
                                             to:done_button.target
                                           from:done_button
                                       forEvent:nil];

    EXPECT_EQ(
        1, user_action_tester.GetActionCount("Signin_AccountsTableView_Close"));
  }
  EXPECT_OCMOCK_VERIFY(strict_delegate);
}

}  // namespace
