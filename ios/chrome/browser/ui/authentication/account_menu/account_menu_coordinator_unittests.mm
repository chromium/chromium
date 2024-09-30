// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/memory/raw_ptr.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator_delegate.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
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
    UIAdaptivePresentationControllerDelegate,
    UINavigationControllerDelegate>

@property(nonatomic, weak) AccountMenuViewController* viewController;
@property(nonatomic, weak) AccountMenuMediator* mediator;

@end

class AccountMenuCoordinatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());

    mock_application_commands_handler_ =
        OCMStrictProtocolMock(@protocol(ApplicationCommands));
    mock_snackbar_commands_handler_ =
        OCMStrictProtocolMock(@protocol(SnackbarCommands));
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
    [dispatcher startDispatchingToTarget:mock_settings_commands_handler_
                             forProtocol:@protocol(SettingsCommands)];
    [dispatcher startDispatchingToTarget:mock_browser_commands_handler_
                             forProtocol:@protocol(BrowserCommands)];
    [dispatcher
        startDispatchingToTarget:mock_browser_coordinator_commands_handler_
                     forProtocol:@protocol(BrowserCoordinatorCommands)];

    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    fake_system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_.get());

    SigninWithPrimaryIdentity();
    AddSecondaryIdentity();

    delegate_ =
        OCMStrictProtocolMock(@protocol(AccountMenuCoordinatorDelegate));
    coordinator_ = [[AccountMenuCoordinator alloc]
        initWithBaseViewController:nil
                           browser:browser_.get()];
    coordinator_.delegate = delegate_;

    [coordinator_ start];

    // Replacing the view controller and mediator by mock.
    view_ = coordinator_.viewController.view;
    navigation_controller_ = coordinator_.viewController.navigationController;
    view_controller_ = OCMStrictClassMock([AccountMenuViewController class]);
    OCMStub([view_controller_ view]).andReturn(view_);
    OCMStub([view_controller_ navigationController])
        .andReturn(navigation_controller_);
    coordinator_.viewController = view_controller_;

    [coordinator_.mediator disconnect];
    mediator_ = OCMStrictClassMock([AccountMenuMediator class]);
    coordinator_.mediator = mediator_;
  }

  void TearDown() override {
    OCMExpect(view_controller_.dataSource = nil);
    OCMExpect(view_controller_.mutator = nil);
    OCMExpect(mediator_.consumer = nil);
    OCMExpect([mediator_ disconnect]);
    OCMExpect(mediator_.delegate = nil);
    [coordinator_ stop];
    VerifyMock();
    PlatformTest::TearDown();
  }

  virtual const FakeSystemIdentity* primary_identity() = 0;

 protected:
  void VerifyMock() {
    EXPECT_OCMOCK_VERIFY((id)delegate_);
    EXPECT_OCMOCK_VERIFY((id)mediator_);
    EXPECT_OCMOCK_VERIFY((id)view_controller_);
    EXPECT_OCMOCK_VERIFY((id)mock_application_commands_handler_);
    EXPECT_OCMOCK_VERIFY((id)mock_browser_commands_handler_);
    EXPECT_OCMOCK_VERIFY((id)mock_browser_coordinator_commands_handler_);
    EXPECT_OCMOCK_VERIFY((id)mock_settings_commands_handler_);
    EXPECT_OCMOCK_VERIFY((id)mock_snackbar_commands_handler_);
  }

  AccountMenuCoordinator<UIAdaptivePresentationControllerDelegate>*
      coordinator_;
  id<AccountMenuCoordinatorDelegate> delegate_;
  id<ApplicationCommands> mock_application_commands_handler_;
  id<SnackbarCommands> mock_snackbar_commands_handler_;
  id<SettingsCommands> mock_settings_commands_handler_;
  id<BrowserCommands> mock_browser_commands_handler_;
  id<BrowserCoordinatorCommands> mock_browser_coordinator_commands_handler_;
  AccountMenuViewController* view_controller_;
  AccountMenuMediator* mediator_;
  raw_ptr<AuthenticationService> authentication_service_;
  raw_ptr<FakeSystemIdentityManager> fake_system_identity_manager_;
  // The view owned by the view controller.
  UIView* view_;
  // The navigation controller of the view controller.
  UINavigationController* navigation_controller_;

 private:
  // Signs in primary_identity() as primary identity.
  void SigninWithPrimaryIdentity() {
    fake_system_identity_manager_->AddIdentity(primary_identity());
    authentication_service_->SignIn(
        primary_identity(), signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  // Add kSecondaryIdentity as a secondary identity.
  void AddSecondaryIdentity() {
    fake_system_identity_manager_->AddIdentity(kSecondaryIdentity);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
};

class AccountMenuCoordinatorNonManagedTest : public AccountMenuCoordinatorTest {
 public:
  const FakeSystemIdentity* primary_identity() override {
    return kPrimaryIdentity;
  }
};

// Tests that `didTapManageYourGoogleAccount` requests the view controller to
// present a view.
TEST_F(AccountMenuCoordinatorNonManagedTest, testManageYourGoogleAccount) {
  OCMExpect([view_controller_ presentViewController:[OCMArg any]
                                           animated:YES
                                         completion:nil]);
  [coordinator_ didTapManageYourGoogleAccount];
}

// Tests that `didTapEditAccountList` has no impact on the view controller and
// mediator.
TEST_F(AccountMenuCoordinatorNonManagedTest, testEditAccountList) {
  [coordinator_ didTapEditAccountList];
}

// Tests that `signOutFromTargetRect` requests the delegate to be stopped and
// shows a snackbar and calls its completion.
TEST_F(AccountMenuCoordinatorNonManagedTest, testSignOut) {
  base::RunLoop run_loop;
  base::RepeatingClosure closure = run_loop.QuitClosure();
  CGRect rect = CGRect();
  OCMExpect([delegate_ acountMenuCoordinatorShouldStop:coordinator_]);
  OCMExpect([mock_snackbar_commands_handler_
      showSnackbarMessageOverBrowserToolbar:[OCMArg isNotNil]]);
  [coordinator_ signOutFromTargetRect:rect
                             callback:^(BOOL success) {
                               EXPECT_TRUE(success);
                               closure.Run();
                             }];
  run_loop.Run();
  EXPECT_EQ(authentication_service_->GetPrimaryIdentity(
                signin::ConsentLevel::kSignin),
            nil);
}

// Tests that `didTapAddAccount` requests the appllication commands handler to
// show signin.
TEST_F(AccountMenuCoordinatorNonManagedTest, testAddAccount) {
  base::RunLoop run_loop;
  base::RepeatingClosure closure = run_loop.QuitClosure();
  __block ShowSigninCommand* command = nil;
  OCMExpect([mock_application_commands_handler_
              showSignin:[OCMArg
                             checkWithBlock:^BOOL(ShowSigninCommand* param) {
                               command = param;
                               return true;
                             }]
      baseViewController:[OCMArg any]]);
  [coordinator_ didTapAddAccount:^(SigninCoordinatorResult result,
                                   SigninCompletionInfo* info) {
    closure.Run();
  }];
  command.callback(SigninCoordinatorResult::SigninCoordinatorResultSuccess,
                   nil);
  run_loop.Run();
}

#pragma mark - AccountMenuMediatorDelegate

// Tests that `mediatorWantsToBeDismissed` requests to the delegate to stop the
// coordinator.
TEST_F(AccountMenuCoordinatorNonManagedTest, testMediatorWantsToBeDismissed) {
  OCMExpect([delegate_ acountMenuCoordinatorShouldStop:coordinator_]);
  [coordinator_ mediatorWantsToBeDismissed:coordinator_.mediator];
}

// Tests that `triggerAccountSwitchWithTargetRect` calls the account switch
// using the application handler.
TEST_F(AccountMenuCoordinatorNonManagedTest, TestAccountSwitch) {
  CGRect rect = CGRect();
  __block ShowSigninCommandCompletionCallback callback = nil;
  __block void (^userDecisionCompletion)();
  userDecisionCompletion = nil;
  OCMExpect([mock_application_commands_handler_
      switchAccountWithBaseViewController:navigation_controller_
                              newIdentity:kSecondaryIdentity
                                     rect:rect
                           rectAnchorView:view_
          viewWillBeDismissedAfterSignout:NO
                   userDecisionCompletion:[OCMArg
                                              checkWithBlock:^BOOL(id value) {
                                                userDecisionCompletion = value;
                                                return true;
                                              }]
                         signInCompletion:[OCMArg
                                              checkWithBlock:^BOOL(id value) {
                                                callback = value;
                                                return true;
                                              }]]);

  [coordinator_ triggerAccountSwitchWithTargetRect:rect
      newIdentity:kSecondaryIdentity
      viewWillBeDismissedAfterSignout:NO
      userDecisionCompletion:^() {
      }
      signInCompletion:^(SigninCoordinatorResult result,
                         SigninCompletionInfo* info) {
        EXPECT_EQ(result, SigninCoordinatorResultSuccess);
      }];

  SigninCompletionInfo* signinCompletionInfo = [SigninCompletionInfo
      signinCompletionInfoWithIdentity:kSecondaryIdentity];
  userDecisionCompletion();
  callback(SigninCoordinatorResultSuccess, signinCompletionInfo);
}

#pragma mark - SyncErrorSettingsCommandHandler

// Tests that `openPassphraseDialogWithModalPresentation` has no impact on the
// view controller and mediator. Tests also that the
// `SyncEncryptionPassphraseTableViewController` is allocated, and the view is
// correctly closed when the coordinator is stopped.
TEST_F(AccountMenuCoordinatorNonManagedTest, testPassphrase) {
  SyncEncryptionPassphraseTableViewController* passphraseViewController =
      [SyncEncryptionPassphraseTableViewController alloc];
  id classMock =
      OCMClassMock([SyncEncryptionPassphraseTableViewController class]);
  OCMStub([classMock alloc]).andReturn(passphraseViewController);
  [coordinator_ openPassphraseDialogWithModalPresentation:YES];
}

// Tests that `openTrustedVaultReauthForFetchKeys` calls
// `showTrustedVaultReauthForFetchKeysFromViewController`.
TEST_F(AccountMenuCoordinatorNonManagedTest, testFetchKeys) {
  OCMExpect([mock_application_commands_handler_
      showTrustedVaultReauthForFetchKeysFromViewController:[OCMArg any]
                                          securityDomainID:
                                              trusted_vault::SecurityDomainId::
                                                  kChromeSync
                                                   trigger:
                                                       syncer::
                                                           TrustedVaultUserActionTriggerForUMA::
                                                               kSettings
                                               accessPoint:
                                                   signin_metrics::AccessPoint::
                                                       ACCESS_POINT_ACCOUNT_MENU]);
  [coordinator_ openTrustedVaultReauthForFetchKeys];
}

// Tests that `openTrustedVaultReauthForDegradedRecoverability` calls
// `showTrustedVaultReauthForDegradedRecoverabilityFromViewController`.
TEST_F(AccountMenuCoordinatorNonManagedTest, testDegradedRecoverability) {
  OCMExpect([mock_application_commands_handler_
      showTrustedVaultReauthForDegradedRecoverabilityFromViewController:[OCMArg
                                                                            any]

                                                       securityDomainID:
                                                           trusted_vault::
                                                               SecurityDomainId::
                                                                   kChromeSync
                                                                trigger:
                                                                    syncer::
                                                                        TrustedVaultUserActionTriggerForUMA::
                                                                            kSettings
                                                            accessPoint:
                                                                signin_metrics::
                                                                    AccessPoint::
                                                                        ACCESS_POINT_ACCOUNT_MENU]);
  [coordinator_ openTrustedVaultReauthForDegradedRecoverability];
}

// Tests that `openMDMErrodDialogWithSystemIdentity` has no effects on the
// mediator and view controller.
TEST_F(AccountMenuCoordinatorNonManagedTest, testMDMError) {
  [coordinator_ openMDMErrodDialogWithSystemIdentity:kPrimaryIdentity];
}

// Tests that `openPrimaryAccountReauthDialog` calls `showSignin`.
TEST_F(AccountMenuCoordinatorNonManagedTest, testReauth) {
  OCMExpect([mock_application_commands_handler_
              showSignin:[OCMArg checkWithBlock:^BOOL(
                                     ShowSigninCommand* value) {
                return value.operation ==
                           AuthenticationOperation::kPrimaryAccountReauth &&
                       value.accessPoint == signin_metrics::AccessPoint::
                                                ACCESS_POINT_ACCOUNT_MENU &&
                       value.identity == nil;
              }]
      baseViewController:[OCMArg any]]);
  [coordinator_ openPrimaryAccountReauthDialog];
}
