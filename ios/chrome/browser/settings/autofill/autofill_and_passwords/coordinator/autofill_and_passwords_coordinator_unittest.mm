// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/coordinator/autofill_and_passwords_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/test/ios/wait_util.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/autofill/core/browser/metrics/autofill_settings_metrics.h"
#import "components/test/ios/test_utils.h"
#import "ios/chrome/browser/settings/autofill/autofill_and_passwords/ui/autofill_and_passwords_table_view_controller.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/coordinator/suggestions_from_gemini_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/browser/webdata_services/model/web_data_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
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
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
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

    navigation_controller_ = [[UINavigationController alloc] init];

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
    navigation_controller_ = nil;
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  id<SceneCommands> mock_scene_commands_;
  id<BrowserCommands> mock_browser_commands_;
  id<SettingsCommands> mock_settings_commands_;
  id<SnackbarCommands> mock_snackbar_commands_;
  UINavigationController* navigation_controller_;
  AutofillAndPasswordsCoordinator* coordinator_;
};

// Tests that selecting Suggestions from Gemini registers the user action.
TEST_F(AutofillAndPasswordsCoordinatorTest,
       SuggestionsFromGeminiActionRecorded) {
  [coordinator_ start];

  base::UserActionTester user_action_tester;

  AutofillAndPasswordsTableViewController* viewController =
      static_cast<AutofillAndPasswordsTableViewController*>(
          navigation_controller_.topViewController);
  ASSERT_NE(nil, viewController);

  id<AutofillAndPasswordsTableViewControllerDelegate> delegate =
      static_cast<id<AutofillAndPasswordsTableViewControllerDelegate>>(
          coordinator_);
  [delegate
      autofillAndPasswordsTableViewControllerDidSelectSuggestionsFromGemini:
          viewController];

  EXPECT_EQ(
      1,
      user_action_tester.GetActionCount(
          "PersonalContext.Settings.EntryPoint.AutofillAndPasswordsSettings"));
}
