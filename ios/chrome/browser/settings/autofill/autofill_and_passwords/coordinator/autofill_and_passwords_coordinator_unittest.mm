// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_and_passwords_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#import "components/test/ios/test_utils.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_table_view_controller.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/password/reauthentication/password_manager_reauthentication_delegate.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class AutofillAndPasswordsCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ios::WebDataServiceFactory::GetInstance(),
                              ios::WebDataServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));

    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    mock_scene_commands_ = OCMProtocolMock(@protocol(SceneCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_scene_commands_
                     forProtocol:@protocol(SceneCommands)];

    mock_browser_commands_ = OCMProtocolMock(@protocol(BrowserCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_browser_commands_
                     forProtocol:@protocol(BrowserCommands)];

    mock_settings_commands_ = OCMProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_settings_commands_
                     forProtocol:@protocol(SettingsCommands)];

    mock_snackbar_commands_ = OCMProtocolMock(@protocol(SnackbarCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_snackbar_commands_
                     forProtocol:@protocol(SnackbarCommands)];

    UIViewController* root_view_controller = [[UIViewController alloc] init];
    navigation_controller_ = [[UINavigationController alloc]
        initWithRootViewController:root_view_controller];
    scoped_window_.Get().rootViewController = navigation_controller_;
    [scoped_window_.Get() makeKeyAndVisible];

    coordinator_ = [[AutofillAndPasswordsCoordinator alloc]
        initWithBaseNavigationController:navigation_controller_
                                 browser:browser_.get()
                                referrer:autofill::autofill_metrics::
                                             AutofillSettingsReferrer::
                                                 kSettingsMenu];
  }

  void TearDown() override {
    [coordinator_ stop];
    coordinator_ = nil;
    scoped_window_.Get().rootViewController = nil;
    navigation_controller_ = nil;
    PlatformTest::TearDown();
  }

  // Waits until `view_controller` is attached to a window and any navigation
  // transition on `navigation_controller_` has completed.
  [[nodiscard]] bool WaitForNavigationTransition(
      UIViewController* view_controller) {
    UINavigationController* navigation_controller = navigation_controller_;
    return base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, true, ^bool() {
          return view_controller.viewIfLoaded.window != nil &&
                 navigation_controller.transitionCoordinator == nil;
        });
  }

  // Starts `coordinator_`, waits for its view controller to finish pushing,
  // and returns the pushed AutofillAndPasswordsTableViewController.
  AutofillAndPasswordsTableViewController* StartCoordinator() {
    [coordinator_ start];
    AutofillAndPasswordsTableViewController*
        autofill_and_passwords_view_controller = base::apple::ObjCCastStrict<
            AutofillAndPasswordsTableViewController>(
            navigation_controller_.topViewController);
    EXPECT_NE(nil, autofill_and_passwords_view_controller);
    EXPECT_TRUE(
        WaitForNavigationTransition(autofill_and_passwords_view_controller));
    return autofill_and_passwords_view_controller;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  id<SceneCommands> mock_scene_commands_;
  id<BrowserCommands> mock_browser_commands_;
  id<SettingsCommands> mock_settings_commands_;
  id<SnackbarCommands> mock_snackbar_commands_;
  ScopedKeyWindow scoped_window_;
  UINavigationController* navigation_controller_;
  AutofillAndPasswordsCoordinator* coordinator_;
};

// Tests that selecting Suggestions from Gemini registers the user action.
TEST_F(AutofillAndPasswordsCoordinatorTest,
       SuggestionsFromGeminiActionRecorded) {
  AutofillAndPasswordsTableViewController*
      autofill_and_passwords_view_controller = StartCoordinator();
  ASSERT_NE(nil, autofill_and_passwords_view_controller);

  base::UserActionTester user_action_tester;

  ASSERT_TRUE([coordinator_
      conformsToProtocol:@protocol(
                             AutofillAndPasswordsTableViewControllerDelegate)]);
  id<AutofillAndPasswordsTableViewControllerDelegate> delegate =
      static_cast<id<AutofillAndPasswordsTableViewControllerDelegate>>(
          coordinator_);
  [delegate
      autofillAndPasswordsTableViewControllerDidSelectSuggestionsFromGemini:
          autofill_and_passwords_view_controller];

  EXPECT_EQ(
      1,
      user_action_tester.GetActionCount(
          "PersonalContext.Settings.EntryPoint.AutofillAndPasswordsSettings"));
}

// Tests that dismissing Password Manager after failed reauthentication pops the
// navigation stack back to the Autofill and Passwords page rather than the root
// Settings page.
TEST_F(AutofillAndPasswordsCoordinatorTest,
       DismissPasswordManagerAfterFailedReauthenticationPopsToAutofillPage) {
  AutofillAndPasswordsTableViewController*
      autofill_and_passwords_view_controller = StartCoordinator();
  ASSERT_NE(nil, autofill_and_passwords_view_controller);
  ASSERT_EQ(2u, navigation_controller_.viewControllers.count);

  // Simulate Password Manager and reauthentication view controllers pushed on
  // top of the Autofill and Passwords page.
  UIViewController* password_manager_view_controller =
      [[UIViewController alloc] init];
  UIViewController* reauthentication_view_controller =
      [[UIViewController alloc] init];
  [navigation_controller_ pushViewController:password_manager_view_controller
                                    animated:NO];
  [navigation_controller_ pushViewController:reauthentication_view_controller
                                    animated:NO];
  ASSERT_EQ(4u, navigation_controller_.viewControllers.count);
  ASSERT_TRUE(WaitForNavigationTransition(reauthentication_view_controller));

  ASSERT_TRUE([coordinator_
      conformsToProtocol:@protocol(PasswordManagerReauthenticationDelegate)]);
  id<PasswordManagerReauthenticationDelegate> reauthentication_delegate =
      static_cast<id<PasswordManagerReauthenticationDelegate>>(coordinator_);
  [reauthentication_delegate dismissPasswordManagerAfterFailedReauthentication];

  UINavigationController* navigation_controller = navigation_controller_;
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, true, ^bool() {
        return navigation_controller.topViewController ==
                   autofill_and_passwords_view_controller &&
               navigation_controller.viewControllers.count == 2u &&
               navigation_controller.transitionCoordinator == nil;
      }));
}

// Tests that dismissing Password Manager after failed reauthentication also
// dismisses any modally presented child view controller while returning to the
// Autofill and Passwords page.
TEST_F(
    AutofillAndPasswordsCoordinatorTest,
    DismissPasswordManagerAfterFailedReauthenticationDismissesPresentedView) {
  AutofillAndPasswordsTableViewController*
      autofill_and_passwords_view_controller = StartCoordinator();
  ASSERT_NE(nil, autofill_and_passwords_view_controller);

  UIViewController* password_manager_view_controller =
      [[UIViewController alloc] init];
  [navigation_controller_ pushViewController:password_manager_view_controller
                                    animated:NO];
  ASSERT_EQ(3u, navigation_controller_.viewControllers.count);
  ASSERT_TRUE(WaitForNavigationTransition(password_manager_view_controller));

  UIViewController* presented_view_controller = [[UIViewController alloc] init];
  [password_manager_view_controller
      presentViewController:presented_view_controller
                   animated:NO
                 completion:nil];
  ASSERT_EQ(presented_view_controller,
            navigation_controller_.topViewController.presentedViewController);

  ASSERT_TRUE([coordinator_
      conformsToProtocol:@protocol(PasswordManagerReauthenticationDelegate)]);
  id<PasswordManagerReauthenticationDelegate> reauthentication_delegate =
      static_cast<id<PasswordManagerReauthenticationDelegate>>(coordinator_);
  [reauthentication_delegate dismissPasswordManagerAfterFailedReauthentication];

  UINavigationController* navigation_controller = navigation_controller_;
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, true, ^bool() {
        return presented_view_controller.presentingViewController == nil &&
               navigation_controller.topViewController ==
                   autofill_and_passwords_view_controller &&
               navigation_controller.transitionCoordinator == nil;
      }));
  EXPECT_EQ(2u, navigation_controller_.viewControllers.count);
}

// Tests that selecting Shopping records the category link click metric.
TEST_F(AutofillAndPasswordsCoordinatorTest, ShoppingCategoryLinkClickRecorded) {
  StartCoordinator();

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "Autofill.YourSavedInfoSettingsPage.CategoryLinkClick", 0);

  AutofillAndPasswordsTableViewController* viewController =
      static_cast<AutofillAndPasswordsTableViewController*>(
          navigation_controller_.topViewController);
  ASSERT_NE(nil, viewController);

  id<AutofillAndPasswordsTableViewControllerDelegate> delegate =
      static_cast<id<AutofillAndPasswordsTableViewControllerDelegate>>(
          coordinator_);
  [delegate
      autofillAndPasswordsTableViewControllerDidSelectShopping:viewController];

  // 5 corresponds to YourSavedInfoDataCategory::kShopping.
  histogram_tester.ExpectUniqueSample(
      "Autofill.YourSavedInfoSettingsPage.CategoryLinkClick", 5, 1);
}
