// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/test/task_environment.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_table_view_controller_action_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_table_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller_action_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_mediator.h"
#import "ios/chrome/test/fakes/fake_ui_navigation_controller.h"
#import "ios/chrome/test/fakes/fake_ui_view_controller.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#pragma mark - DownloadsSettingsCoordinatorTest

// Unit tests for the DownloadsSettingsCoordinator.
class DownloadsSettingsCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() final {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        IdentityManagerFactory::GetInstance(),
        base::BindRepeating(IdentityTestEnvironmentBrowserStateAdaptor::
                                BuildIdentityManagerForTests));
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    base_navigation_controller_ = [[FakeUINavigationController alloc] init];

    mock_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_application_commands_handler_
                     forProtocol:@protocol(ApplicationCommands)];
    mock_settings_commands_handler_ =
        OCMStrictProtocolMock(@protocol(SettingsCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_settings_commands_handler_
                     forProtocol:@protocol(SettingsCommands)];
  }

  void TearDown() final {
    [mock_save_to_photos_settings_mediator_ stopMocking];
    [mock_downloads_settings_table_view_controller_ stopMocking];
    [mock_save_to_photos_settings_account_selection_view_controller_
        stopMocking];
    PlatformTest::TearDown();
  }

  // Creates a DownloadsSettingsCoordinator to test.
  DownloadsSettingsCoordinator* CreateDownloadsSettingsCoordinator() {
    return [[DownloadsSettingsCoordinator alloc]
        initWithBaseNavigationController:base_navigation_controller_
                                 browser:browser_.get()];
  }

  // Creates and returns a mock SaveToPhotosSettingsMediator instance. If
  // `stubbed` is true, then some methods are also stubbed to return the
  // expected values.
  void CreateMockSaveToPhotosSettingsMediatorStubbed(bool stubbed) {
    mock_save_to_photos_settings_mediator_ =
        OCMClassMock([SaveToPhotosSettingsMediator class]);
    if (stubbed) {
      OCMStub([mock_save_to_photos_settings_mediator_ alloc])
          .andReturn(mock_save_to_photos_settings_mediator_);
      OCMStub(
          [mock_save_to_photos_settings_mediator_
              initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                                GetForProfile(profile_.get())
                                prefService:profile_->GetPrefs()
                            identityManager:IdentityManagerFactory::
                                                GetForProfile(profile_.get())])
          .andReturn(mock_save_to_photos_settings_mediator_);
    }
  }

  // Creates and returns a mock DownloadsSettingsTableViewController instance.
  // If `stubbed` is true, then some methods are also stubbed to return the
  // expected values.
  void CreateMockDownloadsSettingsTableViewControllerStubbed(bool stubbed) {
    mock_downloads_settings_table_view_controller_ =
        OCMClassMock([DownloadsSettingsTableViewController class]);
    if (stubbed) {
      OCMStub(
          [mock_downloads_settings_table_view_controller_ navigationController])
          .andReturn(base_navigation_controller_);
      OCMStub([mock_downloads_settings_table_view_controller_ alloc])
          .andReturn(mock_downloads_settings_table_view_controller_);
    }
  }

  // Creates and returns a mock
  // SaveToPhotosSettingsAccountSelectionViewController instance. If `stubbed`
  // is true, then some methods are also stubbed to return the expected values.
  void CreateMockSaveToPhotosSettingsAccountSelectionViewControllerStubbed(
      bool stubbed) {
    mock_save_to_photos_settings_account_selection_view_controller_ =
        OCMClassMock(
            [SaveToPhotosSettingsAccountSelectionViewController class]);
    if (stubbed) {
      OCMStub([mock_save_to_photos_settings_account_selection_view_controller_
                  navigationController])
          .andReturn(base_navigation_controller_);
      OCMStub([mock_save_to_photos_settings_account_selection_view_controller_
                  alloc])
          .andReturn(
              mock_save_to_photos_settings_account_selection_view_controller_);
    }
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  FakeUINavigationController* base_navigation_controller_;

  id mock_save_to_photos_settings_mediator_;
  id mock_downloads_settings_table_view_controller_;
  id mock_save_to_photos_settings_account_selection_view_controller_;
  id mock_application_commands_handler_;
  id mock_settings_commands_handler_;
};

#pragma mark - Tests

// Tests that starting the coordinator creates the expected mediator and the
// Downloads settings view controller, initializes them and pushes the VC on the
// navigation controller.
TEST_F(DownloadsSettingsCoordinatorTest,
       StartsAndStopsCreateAndDestroyMediatorsAndViewController) {
  DownloadsSettingsCoordinator* coordinator =
      CreateDownloadsSettingsCoordinator();

  // Mock mediators and expect that they are created with the expected services.
  CreateMockSaveToPhotosSettingsMediatorStubbed(false);
  OCMExpect([mock_save_to_photos_settings_mediator_ alloc])
      .andReturn(mock_save_to_photos_settings_mediator_);
  OCMExpect(
      [mock_save_to_photos_settings_mediator_
          initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                            GetForProfile(profile_.get())
                            prefService:profile_->GetPrefs()
                        identityManager:IdentityManagerFactory::GetForProfile(
                                            profile_.get())])
      .andReturn(mock_save_to_photos_settings_mediator_);

  // Mock VC and expect it is created and initialized as expected.
  CreateMockDownloadsSettingsTableViewControllerStubbed(false);
  OCMExpect([mock_downloads_settings_table_view_controller_ alloc])
      .andReturn(mock_downloads_settings_table_view_controller_);
  OCMStub([mock_downloads_settings_table_view_controller_ navigationController])
      .andReturn(base_navigation_controller_);
  ASSERT_TRUE([coordinator
      conformsToProtocol:
          @protocol(DownloadsSettingsTableViewControllerActionDelegate)]);
  OCMExpect([mock_downloads_settings_table_view_controller_
      setActionDelegate:
          static_cast<id<DownloadsSettingsTableViewControllerActionDelegate>>(
              coordinator)]);
  ASSERT_TRUE([coordinator
      conformsToProtocol:
          @protocol(DownloadsSettingsTableViewControllerPresentationDelegate)]);
  OCMExpect([mock_downloads_settings_table_view_controller_
      setPresentationDelegate:
          static_cast<
              id<DownloadsSettingsTableViewControllerPresentationDelegate>>(
              coordinator)]);

  // Expect that the mediators and view controllers are connected.
  OCMExpect([mock_save_to_photos_settings_mediator_
      setAccountConfirmationConsumer:
          mock_downloads_settings_table_view_controller_]);
  OCMExpect([mock_downloads_settings_table_view_controller_
      setSaveToPhotosSettingsMutator:mock_save_to_photos_settings_mediator_]);

  // Start the coordinator and verify that the mediator and view controller were
  // properly created and initialized.
  [coordinator start];
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_settings_mediator_);
  EXPECT_OCMOCK_VERIFY(mock_downloads_settings_table_view_controller_);
  // Check that the VC is pushed onto the navigation controller.
  EXPECT_EQ(mock_downloads_settings_table_view_controller_,
            base_navigation_controller_.viewControllers.firstObject);

  // Expect that the mediator is disconnect when the coordinator stops.
  OCMExpect([mock_save_to_photos_settings_mediator_ disconnect]);
  [coordinator stop];
  // Check that the VC has been popped from the navigation controller.
  EXPECT_FALSE(base_navigation_controller_.viewControllers.firstObject);
}

// Tests that the coordinator can present the Save to Photos account selection
// view controller through its
// DownloadsSettingsTableViewControllerActionDelegate interface.
TEST_F(DownloadsSettingsCoordinatorTest, CanOpenSaveToPhotosAccountSelection) {
  DownloadsSettingsCoordinator* coordinator =
      CreateDownloadsSettingsCoordinator();

  // Mock the mediators and the Downloads settings VC.
  CreateMockSaveToPhotosSettingsMediatorStubbed(true);
  CreateMockDownloadsSettingsTableViewControllerStubbed(true);
  [coordinator start];

  // Mock the account selection view controller and expect it is created and
  // initialized as expected.
  CreateMockSaveToPhotosSettingsAccountSelectionViewControllerStubbed(false);
  OCMStub([mock_save_to_photos_settings_account_selection_view_controller_
              navigationController])
      .andReturn(base_navigation_controller_);
  OCMExpect(
      [mock_save_to_photos_settings_account_selection_view_controller_ alloc])
      .andReturn(
          mock_save_to_photos_settings_account_selection_view_controller_);
  ASSERT_TRUE([coordinator
      conformsToProtocol:
          @protocol(
              SaveToPhotosSettingsAccountSelectionViewControllerActionDelegate)]);
  OCMExpect([mock_save_to_photos_settings_account_selection_view_controller_
      setActionDelegate:
          static_cast<id<
              SaveToPhotosSettingsAccountSelectionViewControllerActionDelegate>>(
              coordinator)]);
  ASSERT_TRUE([coordinator
      conformsToProtocol:
          @protocol(
              SaveToPhotosSettingsAccountSelectionViewControllerPresentationDelegate)]);
  OCMExpect([mock_save_to_photos_settings_account_selection_view_controller_
      setPresentationDelegate:
          static_cast<id<
              SaveToPhotosSettingsAccountSelectionViewControllerPresentationDelegate>>(
              coordinator)]);
  OCMExpect([mock_save_to_photos_settings_account_selection_view_controller_
      setMutator:mock_save_to_photos_settings_mediator_]);

  // Expect the mediator is given the account selection VC as account selection
  // consumer.
  OCMExpect([mock_save_to_photos_settings_mediator_
      setAccountSelectionConsumer:
          mock_save_to_photos_settings_account_selection_view_controller_]);

  // Call the coordinator through the action delegate protocol and verify the
  // account selection VC was presented.
  ASSERT_TRUE([coordinator
      conformsToProtocol:
          @protocol(DownloadsSettingsTableViewControllerActionDelegate)]);
  [static_cast<id<DownloadsSettingsTableViewControllerActionDelegate>>(
      coordinator)
      downloadsSettingsTableViewControllerOpenSaveToPhotosAccountSelection:
          mock_downloads_settings_table_view_controller_];
  EXPECT_EQ(mock_save_to_photos_settings_account_selection_view_controller_,
            base_navigation_controller_.viewControllers[1]);
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_settings_mediator_);
  EXPECT_OCMOCK_VERIFY(mock_downloads_settings_table_view_controller_);
  EXPECT_OCMOCK_VERIFY(
      mock_save_to_photos_settings_account_selection_view_controller_);

  // Expect the mediator is disconnected from account selection VC when it has
  // been dismissed.
  OCMExpect(
      [mock_save_to_photos_settings_mediator_ setAccountSelectionConsumer:nil]);
  ASSERT_TRUE([coordinator
      conformsToProtocol:
          @protocol(
              SaveToPhotosSettingsAccountSelectionViewControllerPresentationDelegate)]);
  [static_cast<id<
      SaveToPhotosSettingsAccountSelectionViewControllerPresentationDelegate>>(
      coordinator)
      saveToPhotosSettingsAccountSelectionViewControllerWasRemoved];
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_settings_mediator_);

  [coordinator stop];
}

// Tests that the coordinator can present the Add account view through the
// account selection VC action delegate implementation.
TEST_F(DownloadsSettingsCoordinatorTest,
       OpensAddAccountAndSelectsAddedIdentity) {
  DownloadsSettingsCoordinator* coordinator =
      CreateDownloadsSettingsCoordinator();

  // Mock the mediators and VCs.
  CreateMockSaveToPhotosSettingsMediatorStubbed(true);
  CreateMockDownloadsSettingsTableViewControllerStubbed(true);
  CreateMockSaveToPhotosSettingsAccountSelectionViewControllerStubbed(true);
  [coordinator start];

  // Expect that a ShowSigninCommand is dispatched to show the Add account view
  // when -saveToPhotosSettingsAccountSelectionViewControllerAddAccount is
  // called.
  __block ShowSigninCommandCompletionCallback show_signin_callback = nil;
  OCMExpect([mock_application_commands_handler_
              showSignin:[OCMArg checkWithBlock:^BOOL(
                                     ShowSigninCommand* command) {
                EXPECT_TRUE(command.callback);
                show_signin_callback = command.callback;
                EXPECT_EQ(AuthenticationOperation::kAddAccount,
                          command.operation);
                EXPECT_FALSE(command.identity);
                EXPECT_EQ(signin_metrics::AccessPoint::
                              ACCESS_POINT_SAVE_TO_PHOTOS_IOS,
                          command.accessPoint);
                EXPECT_EQ(
                    signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
                    command.promoAction);
                return YES;
              }]
      baseViewController:base_navigation_controller_]);

  // Call the coordinator through the action delegate protocol and verify the
  // ShowSigninCommand was dispatched.
  ASSERT_TRUE([coordinator
      conformsToProtocol:
          @protocol(
              SaveToPhotosSettingsAccountSelectionViewControllerActionDelegate)]);
  [static_cast<
      id<SaveToPhotosSettingsAccountSelectionViewControllerActionDelegate>>(
      coordinator)
      saveToPhotosSettingsAccountSelectionViewControllerAddAccount];
  EXPECT_OCMOCK_VERIFY(mock_application_commands_handler_);

  // Expect that the selected identity Gaia ID is set to the Gaia ID of the
  // identity that was just added.
  id<SystemIdentity> added_identity = [FakeSystemIdentity fakeIdentity1];
  ASSERT_TRUE(show_signin_callback);
  OCMExpect([mock_save_to_photos_settings_mediator_
      setSelectedIdentityGaiaID:added_identity.gaiaID]);
  show_signin_callback(
      SigninCoordinatorResultSuccess,
      [SigninCompletionInfo signinCompletionInfoWithIdentity:added_identity]);
  EXPECT_OCMOCK_VERIFY(mock_save_to_photos_settings_mediator_);

  [coordinator stop];
}
