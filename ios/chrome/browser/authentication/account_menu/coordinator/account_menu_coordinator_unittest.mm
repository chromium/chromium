// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_mediator.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_mediator_delegate.h"
#import "ios/chrome/browser/authentication/account_menu/public/account_menu_constants.h"
#import "ios/chrome/browser/authentication/account_menu/ui/account_menu_view_controller.h"
#import "ios/chrome/browser/authentication/add_account_signin/coordinator/add_account_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/authentication/ui_bundled/signout_action_sheet/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/settings/ui_bundled/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/test/stub_browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

const FakeSystemIdentity* kPrimaryIdentity = [FakeSystemIdentity fakeIdentity1];
const FakeSystemIdentity* kSecondaryIdentity =
    [FakeSystemIdentity fakeIdentity2];
const FakeSystemIdentity* kManagedIdentity =
    [FakeSystemIdentity fakeManagedIdentity];

}  // namespace

@interface AccountMenuCoordinator (Testing) <
    AccountMenuMediatorDelegate,
    SignoutActionSheetCoordinatorDelegate,
    SyncErrorSettingsCommandHandler,
    UIAdaptivePresentationControllerDelegate,
    UINavigationControllerDelegate>

@property(nonatomic, weak) AccountMenuViewController* viewController;
@property(nonatomic, weak) AccountMenuMediator* mediator;

@end

// The test param determines whether `kSeparateProfilesForManagedAccounts` is
// enabled.
class AccountMenuCoordinatorTest : public PlatformTest,
                                   public testing::WithParamInterface<bool> {
 public:
  AccountMenuCoordinatorTest() {
    feature_list_.InitWithFeatureState(kSeparateProfilesForManagedAccounts,
                                       GetParam());
  }
  void SetUp() override {
    PlatformTest::SetUp();
    scene_state_ = [[SceneState alloc] initWithAppState:nil];

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));
    browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

    stub_browser_interface_provider_ =
        [[StubBrowserProviderInterface alloc] init];
    stub_browser_interface_provider_.currentBrowserProvider.browser =
        browser_.get();
    scene_state_mock_ = OCMPartialMock(scene_state_);
    OCMStub([scene_state_mock_ browserProviderInterface])
        .andReturn(stub_browser_interface_provider_);

    presentation_delegate_ = OCMStrictProtocolMock(@protocol(
        SyncEncryptionPassphraseTableViewControllerPresentationDelegate));
    mock_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    mock_snackbar_commands_handler_ =
        OCMStrictProtocolMock(@protocol(SnackbarCommands));
    mock_help_commands_handler_ =
        OCMStrictProtocolMock(@protocol(HelpCommands));
    mock_settings_commands_handler_ =
        OCMStrictProtocolMock(@protocol(SettingsCommands));
    mock_browser_commands_handler_ =
        OCMStrictProtocolMock(@protocol(BrowserCommands));
    mock_browser_coordinator_commands_handler_ =
        OCMStrictProtocolMock(@protocol(BrowserCoordinatorCommands));
    CommandDispatcher* dispatcher = browser_->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:mock_application_commands_handler_
                             forProtocol:@protocol(ApplicationCommands)];
    [dispatcher startDispatchingToTarget:mock_snackbar_commands_handler_
                             forProtocol:@protocol(SnackbarCommands)];
    [dispatcher startDispatchingToTarget:mock_help_commands_handler_
                             forProtocol:@protocol(HelpCommands)];
    [dispatcher startDispatchingToTarget:mock_settings_commands_handler_
                             forProtocol:@protocol(SettingsCommands)];
    [dispatcher startDispatchingToTarget:mock_browser_commands_handler_
                             forProtocol:@protocol(BrowserCommands)];
    [dispatcher
        startDispatchingToTarget:mock_browser_coordinator_commands_handler_
                     forProtocol:@protocol(BrowserCoordinatorCommands)];

    fake_system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_.get());

    SigninWithPrimaryIdentity();
    AddSecondaryIdentity();

    coordinator_ = [[AccountMenuCoordinator alloc]
        initWithBaseViewController:nil
                           browser:browser_.get()
                        anchorView:nil
                       accessPoint:AccountMenuAccessPoint::kNewTabPage
                               URL:GURL()];
    [coordinator_ start];

    // Replacing the view controller and mediator by mock.
    view_ = coordinator_.viewController.view;
    view_controller_ = OCMStrictClassMock([AccountMenuViewController class]);
    OCMStub([view_controller_ view]).andReturn(view_);
    coordinator_.viewController = view_controller_;

    [coordinator_.mediator disconnect];
    mediator_ = OCMStrictClassMock([AccountMenuMediator class]);
    coordinator_.mediator = mediator_;
  }

  void TearDown() override {
    VerifyMock();
    PlatformTest::TearDown();
  }

 protected:
  void VerifyMock() {
    EXPECT_OCMOCK_VERIFY((id)presentation_delegate_);
    EXPECT_OCMOCK_VERIFY((id)mediator_);
    EXPECT_OCMOCK_VERIFY((id)scene_state_mock_);
    EXPECT_OCMOCK_VERIFY((id)view_controller_);
    EXPECT_OCMOCK_VERIFY((id)mock_application_commands_handler_);
    EXPECT_OCMOCK_VERIFY((id)mock_browser_commands_handler_);
    EXPECT_OCMOCK_VERIFY((id)mock_browser_coordinator_commands_handler_);
    EXPECT_OCMOCK_VERIFY((id)mock_settings_commands_handler_);
    EXPECT_OCMOCK_VERIFY((id)mock_snackbar_commands_handler_);
    EXPECT_OCMOCK_VERIFY((id)mock_help_commands_handler_);
  }

  // Asserts that the coordinator is still open and request it to be closed.
  void AssertOpenAndStop() {
    OCMExpect(view_controller_.dataSource = nil);
    OCMExpect(mediator_.consumer = nil);
    OCMExpect(view_controller_.mutator = nil);
    Stop();
  }

  base::test::ScopedFeatureList feature_list_;

  AccountMenuCoordinator<UIAdaptivePresentationControllerDelegate>*
      coordinator_;
  id<ApplicationCommands> mock_application_commands_handler_;
  id<SnackbarCommands> mock_snackbar_commands_handler_;
  id<HelpCommands> mock_help_commands_handler_;
  id<SettingsCommands> mock_settings_commands_handler_;
  id<BrowserCommands> mock_browser_commands_handler_;
  SceneState* scene_state_;
  // Partial mock for stubbing scene_state_'s methods
  id scene_state_mock_;
  StubBrowserProviderInterface* stub_browser_interface_provider_;
  id<BrowserCoordinatorCommands> mock_browser_coordinator_commands_handler_;
  AccountMenuViewController* view_controller_;
  AccountMenuMediator* mediator_;
  id<SyncEncryptionPassphraseTableViewControllerPresentationDelegate>
      presentation_delegate_;
  raw_ptr<AuthenticationService, DanglingUntriaged> authentication_service_;
  raw_ptr<FakeSystemIdentityManager> fake_system_identity_manager_;
  // The view owned by the view controller.
  UIView* view_;

 private:
  // Stops the coordinator.
  void Stop() {
    OCMExpect([mediator_ disconnect]);
    OCMExpect(mediator_.delegate = nil);
    [coordinator_ stop];
    coordinator_ = nil;
  }

  // Signs in kPrimaryIdentity as primary identity.
  void SigninWithPrimaryIdentity() {
    fake_system_identity_manager_->AddIdentity(kPrimaryIdentity);
    authentication_service_->SignIn(kPrimaryIdentity,
                                    signin_metrics::AccessPoint::kUnknown);
  }

  // Add kSecondaryIdentity as a secondary identity.
  void AddSecondaryIdentity() {
    fake_system_identity_manager_->AddIdentity(kSecondaryIdentity);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

#pragma mark - AccountMenuMediatorDelegate

// Tests that `didTapManageYourGoogleAccount` requests the view controller to
// present a view.
TEST_P(AccountMenuCoordinatorTest, testManageYourGoogleAccount) {
  OCMExpect([view_controller_ presentViewController:[OCMArg any]
                                           animated:YES
                                         completion:nil]);
  [coordinator_ didTapManageYourGoogleAccount];
  OCMExpect([mediator_ accountMenuIsUsable]);
  AssertOpenAndStop();
}

// Tests that `didTapManageAccounts` has no impact on the view controller and
// mediator.
TEST_P(AccountMenuCoordinatorTest, testEditAccountList) {
  [coordinator_ didTapManageAccounts];
  AssertOpenAndStop();
}

// Tests that `signOutFromTargetRect` requests the delegate to be stopped and
// shows a snackbar and calls its completion.
TEST_P(AccountMenuCoordinatorTest, testSignOut) {
  base::RunLoop run_loop;
  base::RepeatingClosure closure = run_loop.QuitClosure();
  CGRect rect = CGRect();
  OCMExpect([mock_snackbar_commands_handler_
      showSnackbarMessageOverBrowserToolbar:[OCMArg isNotNil]]);
  [coordinator_ signOutFromTargetRect:rect
                           completion:^(BOOL success, SceneState* scene_state) {
                             EXPECT_TRUE(success);
                             AssertOpenAndStop();
                             closure.Run();
                           }];
  run_loop.Run();
  EXPECT_EQ(authentication_service_->GetPrimaryIdentity(
                signin::ConsentLevel::kSignin),
            nil);
}

// Tests that `mediatorWantsToBeDismissed` requests to the delegate to stop the
// coordinator.
TEST_P(AccountMenuCoordinatorTest, testMediatorWantsToBeDismissed) {
  AssertOpenAndStop();
}

// Tests that `triggerSignoutWithTargetRect` calls its
// callback.
TEST_P(AccountMenuCoordinatorTest, testTriggerSignout) {
  OCMExpect([mock_snackbar_commands_handler_
      showSnackbarMessageOverBrowserToolbar:[OCMArg any]]);

  base::RunLoop run_loop;
  base::RepeatingClosure closure = run_loop.QuitClosure();
  CGRect rect = CGRect();
  [coordinator_ signOutFromTargetRect:rect
                           completion:^(BOOL success, SceneState* scene_state) {
                             EXPECT_TRUE(success);
                             closure.Run();
                           }];
  run_loop.Run();
  AssertOpenAndStop();
}

#pragma mark - SyncErrorSettingsCommandHandler

// Tests that `openPassphraseDialogWithModalPresentation` has no impact on the
// view controller and mediator. Tests also that the
// `SyncEncryptionPassphraseTableViewController` is allocated, and the view is
// correctly closed when the coordinator is stopped.
TEST_P(AccountMenuCoordinatorTest, testPassphrase) {
  SyncEncryptionPassphraseTableViewController* passphraseViewController =
      [SyncEncryptionPassphraseTableViewController alloc];
  passphraseViewController.presentationDelegate = presentation_delegate_;
  id classMock =
      OCMClassMock([SyncEncryptionPassphraseTableViewController class]);
  OCMExpect([classMock alloc]).andReturn(passphraseViewController);
  [coordinator_ openPassphraseDialogWithModalPresentation:YES];
  AssertOpenAndStop();
  EXPECT_OCMOCK_VERIFY(classMock);
}

// Tests that `openTrustedVaultReauthForFetchKeys` calls
// `showTrustedVaultReauthForFetchKeysFromViewController`.
TEST_P(AccountMenuCoordinatorTest, testFetchKeys) {
  [coordinator_ openTrustedVaultReauthForFetchKeys];
  AssertOpenAndStop();
}

// Tests that `openTrustedVaultReauthForDegradedRecoverability` calls
// `showTrustedVaultReauthForDegradedRecoverabilityFromViewController`.
TEST_P(AccountMenuCoordinatorTest, testDegradedRecoverability) {
  [coordinator_ openTrustedVaultReauthForDegradedRecoverability];
  AssertOpenAndStop();
}

// Tests that `openMDMErrodDialogWithSystemIdentity` has no effects on the
// mediator and view controller.
TEST_P(AccountMenuCoordinatorTest, testMDMError) {
  [coordinator_ openMDMErrodDialogWithSystemIdentity:kPrimaryIdentity];
  AssertOpenAndStop();
}

INSTANTIATE_TEST_SUITE_P(,
                         AccountMenuCoordinatorTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "WithSeparateProfiles"
                                             : "WithoutSeparateProfiles";
                         });
