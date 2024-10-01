// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_configuration.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_coordinator.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_coordinator_delegate.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/account_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/manage_storage_alert_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_coordinator.h"
#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_mediator.h"
#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/fakes/fake_ui_view_controller.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Constants for configuring a fake download task.
const char kTestUrl[] = "https://chromium.test/download.txt";
const char kTestMimeType[] = "text/html";

}  // namespace

// Unit tests for the SaveToDriveCoordinator.
class SaveToDriveCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() final {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
    drive_service_ = drive::DriveServiceFactory::GetForProfile(profile_.get());
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
    pref_service_ = profile_->GetPrefs();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    std::unique_ptr<web::FakeWebState> web_state =
        std::make_unique<web::FakeWebState>();
    browser_->GetWebStateList()->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate());

    base_view_controller_ = [[FakeUIViewController alloc] init];

    mock_save_to_drive_commands_handler_ =
        OCMStrictProtocolMock(@protocol(SaveToDriveCommands));
    [browser_->GetCommandDispatcher()
        startDispatchingToTarget:mock_save_to_drive_commands_handler_
                     forProtocol:@protocol(SaveToDriveCommands)];
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

    mock_save_to_drive_mediator_ = OCMClassMock([SaveToDriveMediator class]);

    download_task_ =
        std::make_unique<web::FakeDownloadTask>(GURL(kTestUrl), kTestMimeType);
    download_task_->SetWebState(GetActiveWebState());
  }

  // Set up the mediator stub to ensure the coordinator creates a fake mediator.
  void SetUpMediatorStub() {
    OCMStub([mock_save_to_drive_mediator_ alloc])
        .andReturn(mock_save_to_drive_mediator_);
    OCMStub([mock_save_to_drive_mediator_
                     initWithDownloadTask:download_task_.get()
                       saveToDriveHandler:[OCMArg any]
                manageStorageAlertHandler:[OCMArg any]
                       applicationHandler:[OCMArg any]
                     accountPickerHandler:[OCMArg any]
                              prefService:pref_service_
                    accountManagerService:account_manager_service_
                             driveService:drive_service_])
        .andReturn(mock_save_to_drive_mediator_);
  }

  void TearDown() final {
    [mock_save_to_drive_mediator_ stopMocking];
    PlatformTest::TearDown();
  }

  // Create a SaveToDriveCoordinator to test.
  SaveToDriveCoordinator* CreateSaveToDriveCoordinator() {
    return [[SaveToDriveCoordinator alloc]
        initWithBaseViewController:base_view_controller_
                           browser:browser_.get()
                      downloadTask:download_task_.get()];
  }

  // Returns the browser's active web state.
  web::FakeWebState* GetActiveWebState() {
    return static_cast<web::FakeWebState*>(
        browser_->GetWebStateList()->GetActiveWebState());
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  UIViewController* base_view_controller_;
  std::unique_ptr<web::FakeDownloadTask> download_task_;
  raw_ptr<drive::DriveService> drive_service_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;

  id mock_save_to_drive_mediator_;
  id mock_save_to_drive_commands_handler_;
  id mock_application_commands_handler_;
  id mock_settings_commands_handler_;
};

// Tests that the SaveToDriveCoordinator creates the mediator when started and
// disconnects it when stopped.
TEST_F(SaveToDriveCoordinatorTest, StartsAndDisconnectsMediator) {
  SaveToDriveCoordinator* coordinator = CreateSaveToDriveCoordinator();

  ASSERT_TRUE([SaveToDriveCoordinator
      conformsToProtocol:@protocol(ManageStorageAlertCommands)]);
  id<ManageStorageAlertCommands> manage_storage_commands =
      static_cast<id<ManageStorageAlertCommands>>(coordinator);
  ASSERT_TRUE([SaveToDriveCoordinator
      conformsToProtocol:@protocol(AccountPickerCommands)]);
  id<AccountPickerCommands> account_picker_commands =
      static_cast<id<AccountPickerCommands>>(coordinator);
  OCMExpect([mock_save_to_drive_mediator_ alloc])
      .andReturn(mock_save_to_drive_mediator_);
  OCMExpect([mock_save_to_drive_mediator_
                     initWithDownloadTask:download_task_.get()
                       saveToDriveHandler:static_cast<id<SaveToDriveCommands>>(
                                              browser_->GetCommandDispatcher())
                manageStorageAlertHandler:manage_storage_commands
                       applicationHandler:static_cast<id<ApplicationCommands>>(
                                              browser_->GetCommandDispatcher())
                     accountPickerHandler:account_picker_commands
                              prefService:pref_service_
                    accountManagerService:account_manager_service_
                             driveService:drive_service_])
      .andReturn(mock_save_to_drive_mediator_);
  [coordinator start];
  EXPECT_OCMOCK_VERIFY(mock_save_to_drive_mediator_);

  OCMExpect([mock_save_to_drive_mediator_ disconnect]);
  [coordinator stop];
  EXPECT_OCMOCK_VERIFY(mock_save_to_drive_mediator_);
}

// Tests that the SaveToDriveCoordinator creates/destroys an
// AccountPickerCoordinator with the expected configuration when it
// starts/stops.
TEST_F(SaveToDriveCoordinatorTest, ShowsAndHidesAccountPicker) {
  SetUpMediatorStub();

  SaveToDriveCoordinator* coordinator = CreateSaveToDriveCoordinator();
  id mock_account_picker_coordinator =
      OCMClassMock([AccountPickerCoordinator class]);
  OCMExpect([mock_account_picker_coordinator alloc])
      .andReturn(mock_account_picker_coordinator);
  __block AccountPickerConfiguration* observed_conf = nil;
  OCMExpect(
      [mock_account_picker_coordinator
          initWithBaseViewController:base_view_controller_
                             browser:browser_.get()
                       configuration:[OCMArg
                                         checkWithBlock:^BOOL(
                                             AccountPickerConfiguration* conf) {
                                           observed_conf = conf;
                                           return YES;
                                         }]])
      .andReturn(mock_account_picker_coordinator);
  OCMExpect([mock_account_picker_coordinator
      setDelegate:static_cast<id<AccountPickerCoordinatorDelegate>>(
                      coordinator)]);
  OCMExpect([base::apple::ObjCCast<AccountPickerCoordinator>(
      mock_account_picker_coordinator) start]);

  [coordinator start];
  EXPECT_OCMOCK_VERIFY(mock_account_picker_coordinator);

  AccountPickerConfiguration* expected_conf =
      drive::GetAccountPickerConfiguration(download_task_.get());
  EXPECT_NSEQ(expected_conf.titleText, observed_conf.titleText);
  EXPECT_NSEQ(expected_conf.bodyText, observed_conf.bodyText);
  EXPECT_NSEQ(expected_conf.submitButtonTitle, observed_conf.submitButtonTitle);
  EXPECT_NSEQ(expected_conf.askEveryTimeSwitchLabelText,
              observed_conf.askEveryTimeSwitchLabelText);

  OCMExpect([base::apple::ObjCCast<AccountPickerCoordinator>(
      mock_account_picker_coordinator) stop]);
  [coordinator stop];
  EXPECT_OCMOCK_VERIFY(mock_account_picker_coordinator);
}

// Tests that the SaveToDriveCoordinator shows the Add account view when the
// Account picker requires it.
TEST_F(SaveToDriveCoordinatorTest, ShowsAddAccount) {
  SetUpMediatorStub();

  SaveToDriveCoordinator* coordinator = CreateSaveToDriveCoordinator();
  [coordinator start];

  ASSERT_TRUE([coordinator
      conformsToProtocol:@protocol(AccountPickerCoordinatorDelegate)]);

  // Create a mock account picker with a mocked view controller.
  id mock_account_picker_coordinator =
      OCMClassMock([AccountPickerCoordinator class]);
  FakeUIViewController* mock_account_picker_coordinator_view_controller =
      [[FakeUIViewController alloc] init];
  OCMStub([mock_account_picker_coordinator viewController])
      .andReturn(mock_account_picker_coordinator_view_controller);

  // Expect that a ShowSigninCommand will be dispatched to present the Add
  // account view on top of the account picker view.
  id<SystemIdentity> added_identity = [FakeSystemIdentity fakeIdentity1];
  OCMExpect([mock_application_commands_handler_
              showSignin:[OCMArg checkWithBlock:^BOOL(
                                     ShowSigninCommand* command) {
                if (command) {
                  command.callback(
                      SigninCoordinatorResultSuccess,
                      [SigninCompletionInfo
                          signinCompletionInfoWithIdentity:added_identity]);
                }
                EXPECT_EQ(AuthenticationOperation::kAddAccount,
                          command.operation);
                EXPECT_FALSE(command.identity);
                EXPECT_EQ(
                    signin_metrics::AccessPoint::ACCESS_POINT_SAVE_TO_DRIVE_IOS,
                    command.accessPoint);
                EXPECT_EQ(
                    signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
                    command.promoAction);
                return YES;
              }]
      baseViewController:mock_account_picker_coordinator_view_controller]);

  // Ask the SaveToDriveCoordinator to open the Add account view and verify the
  // ShowSigninCommand was dispatched.
  [static_cast<id<AccountPickerCoordinatorDelegate>>(coordinator)
          accountPickerCoordinator:mock_account_picker_coordinator
      openAddAccountWithCompletion:^(id<SystemIdentity> identity) {
        EXPECT_EQ(added_identity, identity);
      }];
  EXPECT_OCMOCK_VERIFY(mock_application_commands_handler_);

  [coordinator stop];
}
