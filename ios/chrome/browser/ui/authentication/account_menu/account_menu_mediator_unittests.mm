// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/task_environment.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_consumer.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator_delegate.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_view_controller.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

const FakeSystemIdentity* kPrimaryIdentity = [FakeSystemIdentity fakeIdentity1];
const FakeSystemIdentity* kSecondaryIdentity =
    [FakeSystemIdentity fakeIdentity2];
const FakeSystemIdentity* kSecondaryIdentity2 =
    [FakeSystemIdentity fakeIdentity3];
}  // namespace

class AccountMenuMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    // Set the profile.
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();

    // Set the manager and services variables.
    fake_system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    authentication_service_ =
        AuthenticationServiceFactory::GetForProfile(profile_.get());
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_.get());

    AddPrimaryIdentity();
    AddSecondaryIdentity();

    // Set the mediator and its mocks delegate.
    delegate_ = OCMStrictProtocolMock(@protocol(AccountMenuMediatorDelegate));
    consumer_ = OCMStrictProtocolMock(@protocol(AccountMenuConsumer));
    mediator_ = [[AccountMenuMediator alloc]
          initWithSyncService:SyncService()
        accountManagerService:account_manager_service_
                  authService:authentication_service_
              identityManager:identity_manager_
                        prefs:profile_->GetPrefs()];
    mediator_.delegate = delegate_;
    mediator_.consumer = consumer_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    VerifyMock();
    PlatformTest::TearDown();
  }

  syncer::TestSyncService* SyncService() { return test_sync_service_.get(); }

 protected:
  // Verify that all mocks expectation are fulfilled.
  void VerifyMock() {
    EXPECT_OCMOCK_VERIFY(delegate_);
    EXPECT_OCMOCK_VERIFY(consumer_);
  }

  // Set the passphrase required, update the mediator, return the account error
  // ui info.
  AccountErrorUIInfo* setPassphraseRequired() {
    SyncService()->SetInitialSyncFeatureSetupComplete(false);
    SyncService()->SetPassphraseRequired();

    __block AccountErrorUIInfo* errorSentToConsumer = nil;
    OCMExpect(
        [consumer_ updateErrorSection:[OCMArg checkWithBlock:^BOOL(id value) {
                     errorSentToConsumer = value;
                     return value;
                   }]]);
    SyncService()->FireStateChanged();
    return errorSentToConsumer;
  }

  FakeSystemIdentityManager* GetSystemIdentityManager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  void SignOut() {
    base::RunLoop run_loop;
    base::RepeatingClosure closure = run_loop.QuitClosure();
    authentication_service_->SignOut(signin_metrics::ProfileSignout::kTest,
                                     /*force_clear_browsing_data=*/false, ^() {
                                       closure.Run();
                                     });
    run_loop.Run();
  }

  id<AccountMenuMediatorDelegate> delegate_;
  id<AccountMenuConsumer> consumer_;
  AccountMenuMediator* mediator_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  raw_ptr<AuthenticationService> authentication_service_;
  raw_ptr<FakeSystemIdentityManager> fake_system_identity_manager_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  base::UserActionTester user_actions_;

 private:
  // Signs in kPrimaryIdentity as primary identity.
  void AddPrimaryIdentity() {
    fake_system_identity_manager_->AddIdentity(kPrimaryIdentity);
    authentication_service_->SignIn(
        kPrimaryIdentity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  // Add kSecondaryIdentity as a secondary identity.
  void AddSecondaryIdentity() {
    fake_system_identity_manager_->AddIdentity(kSecondaryIdentity);
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
};

#pragma mark - Test for ChromeAccountManagerServiceObserver

// Checks that adding a secondary identity lead to updating the
// consumer.
TEST_F(AccountMenuMediatorTest, TestAddSecondaryIdentity) {
  const FakeSystemIdentity* thirdIdentity = [FakeSystemIdentity fakeIdentity3];
  OCMExpect([consumer_
      updateAccountListWithGaiaIDsToAdd:@[ thirdIdentity.gaiaID ]
                        gaiaIDsToRemove:@[]
                          gaiaIDsToKeep:@[ kSecondaryIdentity.gaiaID ]]);
  fake_system_identity_manager_->AddIdentity(thirdIdentity);
}

// Checks that removing a secondary identity lead to updating the
// consumer.
TEST_F(AccountMenuMediatorTest, TestRemoveSecondaryIdentity) {
  // Expectations due to ChromeAccountManagerServiceObserver updates.
  OCMExpect([consumer_
      updateAccountListWithGaiaIDsToAdd:@[]
                        gaiaIDsToRemove:@[]
                          gaiaIDsToKeep:@[ kSecondaryIdentity.gaiaID ]]);
  OCMExpect([consumer_ updatePrimaryAccount]);
  OCMExpect([consumer_
      updateAccountListWithGaiaIDsToAdd:@[]
                        gaiaIDsToRemove:@[]
                          gaiaIDsToKeep:@[ kSecondaryIdentity.gaiaID ]]);

  OCMExpect([consumer_
      updateAccountListWithGaiaIDsToAdd:@[]
                        gaiaIDsToRemove:@[ kSecondaryIdentity.gaiaID ]
                          gaiaIDsToKeep:@[]]);
  {
    base::RunLoop run_loop;
    base::RepeatingClosure closure = run_loop.QuitClosure();
    fake_system_identity_manager_->ForgetIdentity(
        kSecondaryIdentity, base::BindOnce(^(NSError* error) {
          EXPECT_THAT(error, testing::IsNull());
          closure.Run();
        }));
    run_loop.Run();
  }
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Checks that removing the primary identity lead to updating the
// consumer.
TEST_F(AccountMenuMediatorTest, TestRemovePrimaryIdentity) {
  OCMExpect([delegate_ mediatorWantsToBeDismissed:mediator_]);
  OCMExpect([consumer_ setUserInteractionsEnabled:NO]);
  authentication_service_->SignOut(signin_metrics::ProfileSignout::kTest,
                                   /*force_clear_browsing_data=*/false,
                                   ^(){
                                   });
}

#pragma mark - AccountMenuDataSource

// Tests the result of secondaryAccountsGaiaIDs.
TEST_F(AccountMenuMediatorTest, TestSecondaryAccountsGaiaID) {
  EXPECT_NSEQ([mediator_ secondaryAccountsGaiaIDs],
              @[ kSecondaryIdentity.gaiaID ]);
}

#pragma mark - AccountMenuDataSource and SyncObserverModelBridge

// Tests the result of nameForGaiaID.
TEST_F(AccountMenuMediatorTest, nameForGaiaID) {
  EXPECT_NSEQ([mediator_ nameForGaiaID:kSecondaryIdentity.gaiaID],
              kSecondaryIdentity.userFullName);
}

// Tests the result of emailForGaiaID.
TEST_F(AccountMenuMediatorTest, emailForGaiaID) {
  EXPECT_NSEQ([mediator_ emailForGaiaID:kSecondaryIdentity.gaiaID],
              kSecondaryIdentity.userEmail);
}

// Tests the result of imageForGaiaID.
TEST_F(AccountMenuMediatorTest, imageForGaiaID) {
  EXPECT_NSEQ([mediator_ imageForGaiaID:kSecondaryIdentity.gaiaID],
              account_manager_service_ -> GetIdentityAvatarWithIdentity(
                                           kSecondaryIdentity,
                                           IdentityAvatarSize::TableViewIcon));
}

// Tests the result of primaryAccountEmail.
TEST_F(AccountMenuMediatorTest, TestPrimaryAccountEmail) {
  EXPECT_NSEQ([mediator_ primaryAccountEmail], kPrimaryIdentity.userEmail);
}

// Tests the result of primaryAccountUserFullName.
TEST_F(AccountMenuMediatorTest, TestPrimaryAccountUserFullName) {
  EXPECT_NSEQ([mediator_ primaryAccountUserFullName],
              kPrimaryIdentity.userFullName);
}

// Tests the result of primaryAccountAvatar.
TEST_F(AccountMenuMediatorTest, TestPrimaryAccountAvatar) {
  EXPECT_NSEQ([mediator_ primaryAccountAvatar],
              account_manager_service_ -> GetIdentityAvatarWithIdentity(
                                           kPrimaryIdentity,
                                           IdentityAvatarSize::Large));
}

// Tests the result of TestError when there is no error.
TEST_F(AccountMenuMediatorTest, TestNoError) {
  EXPECT_THAT([mediator_ accountErrorUIInfo], testing::IsNull());
}

// Tests the result of TestError when passphrase is required.
TEST_F(AccountMenuMediatorTest, TestError) {
  // In order to simulate requiring a passphrase, test sync service requires
  // us to explicitly set that the setup is not complete, and fire the state
  // change to its observer.

  AccountErrorUIInfo* errorSentToConsumer = setPassphraseRequired();
  AccountErrorUIInfo* expectedError = GetAccountErrorUIInfo(SyncService());

  AccountErrorUIInfo* actualError = [mediator_ accountErrorUIInfo];
  EXPECT_THAT(actualError, testing::NotNull());
  EXPECT_NSEQ(actualError, expectedError);
  EXPECT_NSEQ(actualError, errorSentToConsumer);
  EXPECT_EQ(actualError.errorType,
            syncer::SyncService::UserActionableError::kNeedsPassphrase);
  EXPECT_EQ(actualError.userActionableType,
            AccountErrorUserActionableType::kEnterPassphrase);
}

#pragma mark - AccountMenuMutator

// Tests the result of accountTappedWithGaiaID:targetRect:
// when sign-out fail.
TEST_F(AccountMenuMediatorTest, TestAccountTapedSignoutFailed) {
  // Given that the method  `triggerSignoutWithTargetRect:completion` creates a
  // callback in a callback, this tests has three parts.  One part by callback,
  // and one part for the initial part of the run.

  // Testing the part before the callback.
  // This variable will contain the callback that should be executed once
  // sign-out ends.
  __block void (^onSignoutSuccess)(BOOL) = nil;
  auto target = CGRect();
  OCMExpect([delegate_
      signOutFromTargetRect:target
                  forSwitch:YES
                 completion:[OCMArg checkWithBlock:^BOOL(id value) {
                   onSignoutSuccess = value;
                   return true;
                 }]]);
  OCMExpect([delegate_ blockOtherScenesIfPossible]);
  OCMExpect([consumer_ switchingStarted]);
  OCMExpect([consumer_ setUserInteractionsEnabled:NO]);
  [mediator_ accountTappedWithGaiaID:kSecondaryIdentity.gaiaID
                          targetRect:target];
  VerifyMock();

  OCMExpect([consumer_
      updateAccountListWithGaiaIDsToAdd:@[]
                        gaiaIDsToRemove:@[]
                          gaiaIDsToKeep:@[ kSecondaryIdentity.gaiaID ]]);
  OCMExpect([delegate_ unblockOtherScenes]);
  OCMExpect([consumer_ switchingStopped]);
  OCMExpect([consumer_ setUserInteractionsEnabled:YES]);
  // Simulate a sign-out failure
  onSignoutSuccess(false);
}

// Tests the result of accountTappedWithGaiaID:targetRect:
// when sign-in fail.
TEST_F(AccountMenuMediatorTest, TestAccountTapedSignInFailed) {
  // Given that the method  `signOutFromTargetRect:forSwitch:completion` create
  // a callback in a callback, this tests has three parts.  One part by
  // callback, and one part for the initial part of the run.

  // Testing the part before the callback.
  // This variable will contain the callback that should be executed once
  // sign-out ends.
  __block signin_ui::SignoutCompletionCallback onSignoutSuccess = nil;
  auto target = CGRect();
  OCMExpect([delegate_
      signOutFromTargetRect:target
                  forSwitch:YES
                 completion:[OCMArg checkWithBlock:^BOOL(id value) {
                   onSignoutSuccess = value;
                   // Actually sign-out, in order to test next step.
                   SignOut();
                   return true;
                 }]]);
  OCMExpect([delegate_ blockOtherScenesIfPossible]);
  OCMExpect([consumer_ switchingStarted]);
  OCMExpect([consumer_ setUserInteractionsEnabled:NO]);
  [mediator_ accountTappedWithGaiaID:kSecondaryIdentity.gaiaID
                          targetRect:target];
  VerifyMock();

  // Simulate a sign-out success.
  // This variable will contain the callback that should be executed once
  // sign-in ended.
  __block signin_ui::SigninCompletionCallback onSigninSuccess = nil;
  OCMExpect([delegate_
                triggerSigninWithSystemIdentity:kSecondaryIdentity
                                     completion:[OCMArg checkWithBlock:^BOOL(
                                                            id value) {
                                       onSigninSuccess = value;
                                       return true;
                                     }]])
      .andReturn(OCMStrictClassMock([AuthenticationFlow class]));
  onSignoutSuccess(true);

  // Testing the sign-in callback.
  // The delegate should not receive any message. The mediator directly sign the
  // user back in the previous account.
  OCMExpect([delegate_ unblockOtherScenes]);
  OCMExpect([consumer_
      updateAccountListWithGaiaIDsToAdd:@[]
                        gaiaIDsToRemove:@[]
                          gaiaIDsToKeep:@[ kSecondaryIdentity.gaiaID ]]);
  OCMExpect([consumer_ updatePrimaryAccount]);
  OCMExpect([consumer_ switchingStopped]);
  OCMExpect([consumer_ setUserInteractionsEnabled:YES]);
  onSigninSuccess(SigninCoordinatorResult::SigninCoordinatorResultInterrupted);

  // Checks the user is signed-back in.
  ASSERT_EQ(kPrimaryIdentity, authentication_service_->GetPrimaryIdentity(
                                  signin::ConsentLevel::kSignin));
}

// Tests the result of accountTappedWithGaiaID:targetRect:
// when switch is succesful.
TEST_F(AccountMenuMediatorTest, TestAccountTapedWithSuccesfulSwitch) {
  // Given that the method  `signOutFromTargetRect:forSwitch:callback` create a
  // callback in a callback, this tests has three parts.  One part by callback,
  // and one part for the initial part of the run.

  // Testing the part before the callback.
  // This variable will contain the callback that should be executed once
  // sign-out ends.
  __block signin_ui::SignoutCompletionCallback onSignoutSuccess = nil;
  auto target = CGRect();
  OCMExpect([delegate_
      signOutFromTargetRect:target
                  forSwitch:YES
                 completion:[OCMArg checkWithBlock:^BOOL(id value) {
                   onSignoutSuccess = value;
                   return true;
                 }]]);
  OCMExpect([delegate_ blockOtherScenesIfPossible]);
  OCMExpect([consumer_ switchingStarted]);
  OCMExpect([consumer_ setUserInteractionsEnabled:NO]);
  [mediator_ accountTappedWithGaiaID:kSecondaryIdentity.gaiaID
                          targetRect:target];
  VerifyMock();

  // Simulate a sign-out success.
  // This variable will contain the callback that should be executed once
  // sign-in ends.
  __block signin_ui::SigninCompletionCallback onSigninSuccess = nil;
  {
    base::RunLoop run_loop;
    base::RepeatingClosure closure = run_loop.QuitClosure();
    OCMExpect([delegate_
                  triggerSigninWithSystemIdentity:kSecondaryIdentity
                                       completion:[OCMArg checkWithBlock:^BOOL(
                                                              id value) {
                                         onSigninSuccess = value;
                                         closure.Run();
                                         return true;
                                       }]])
        .andReturn(OCMStrictClassMock([AuthenticationFlow class]));
    onSignoutSuccess(true);
    run_loop.Run();
  }
  VerifyMock();

  OCMExpect([delegate_ unblockOtherScenes]);
  OCMExpect(
      [delegate_ triggerAccountSwitchSnackbarWithIdentity:kSecondaryIdentity]);
  OCMExpect([delegate_ mediatorWantsToBeDismissed:mediator_]);
  onSigninSuccess(SigninCoordinatorResultSuccess);
}

// Tests the result of didTapErrorButton when a passphrase is required.
TEST_F(AccountMenuMediatorTest, TestTapErrorButtonPassphrase) {
  // While many errors can be displayed by the account menu, this test suite
  // only consider the error where the passphrase is needed. This is because,
  // when the suite was written, `TestSyncService::GetUserActionableError` could
  // only returns `kNeedsPassphrase` and `kSignInNeedsUpdate`. Furthermore,
  // `kSignInNeedsUpdate` is not an error displayed to the user (technically,
  // `GetAccountErrorUIInfo` returns `nil` on `kSignInNeedsUpdate`.)
  setPassphraseRequired();
  OCMExpect([delegate_ openPassphraseDialogWithModalPresentation:YES]);
  [mediator_ didTapErrorButton];
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "Signin_AccountMenu_ErrorButton_Passphrase"));
}

// Tests the effect of didTapManageYourGoogleAccount.
TEST_F(AccountMenuMediatorTest, TestDidTapManageYourGoogleAccount) {
  OCMExpect([delegate_ didTapManageYourGoogleAccount]);
  [mediator_ didTapManageYourGoogleAccount];
}

// Tests the effect of didTapManageAccounts.
TEST_F(AccountMenuMediatorTest, TestDidTapEditAccountList) {
  OCMExpect([delegate_ didTapManageAccounts]);
  [mediator_ didTapManageAccounts];
}

// Tests the effect of didTapAddAccount.
TEST_F(AccountMenuMediatorTest, TestDidTapAddAccount) {
  __block ShowSigninCommandCompletionCallback completion = nil;
  OCMExpect([delegate_
      didTapAddAccountWithCompletion:[OCMArg checkWithBlock:^BOOL(id value) {
        completion = value;
        return true;
      }]]);
  OCMExpect([consumer_ setUserInteractionsEnabled:NO]);
  [mediator_ didTapAddAccount];
  OCMExpect([consumer_ switchingStopped]);
  OCMExpect([consumer_
      updateAccountListWithGaiaIDsToAdd:@[]
                        gaiaIDsToRemove:@[]
                          gaiaIDsToKeep:@[ kSecondaryIdentity.gaiaID ]]);
  OCMExpect([consumer_ setUserInteractionsEnabled:YES]);
  completion(SigninCoordinatorResult::SigninCoordinatorResultInterrupted, nil);
}

// Tests the effect of signOutFromTargetRect.
TEST_F(AccountMenuMediatorTest, TestSignoutFromTargetRect) {
  CGRect rect = CGRectMake(0, 0, 40, 24);

  __block void (^completion)(BOOL) = nil;
  OCMExpect([delegate_
      signOutFromTargetRect:rect
                  forSwitch:NO
                 completion:[OCMArg checkWithBlock:^BOOL(id value) {
                   completion = value;
                   return true;
                 }]]);
  OCMExpect([delegate_ blockOtherScenesIfPossible]).andReturn(YES);
  OCMExpect([consumer_ setUserInteractionsEnabled:NO]);
  [mediator_ signOutFromTargetRect:rect];
  OCMExpect([delegate_ unblockOtherScenes]);
  OCMExpect([delegate_ mediatorWantsToBeDismissed:mediator_]);
  completion(YES);
}

// Tests tapping on the close button just after the sign-out button.
// This is a regression test for crbug.com/371046656.
TEST_F(AccountMenuMediatorTest, TestSignoutAndClose) {
  CGRect rect = CGRectMake(0, 0, 40, 24);
  __block void (^completion)(BOOL) = nil;
  OCMExpect([delegate_
      signOutFromTargetRect:rect
                  forSwitch:NO
                 completion:[OCMArg checkWithBlock:^BOOL(id value) {
                   completion = value;
                   return true;
                 }]]);
  OCMExpect([delegate_ blockOtherScenesIfPossible]).andReturn(YES);
  OCMExpect([consumer_ setUserInteractionsEnabled:NO]);
  [mediator_ signOutFromTargetRect:rect];
  [mediator_ disconnect];
  OCMExpect([delegate_ unblockOtherScenes]);
  OCMExpect([consumer_ setUserInteractionsEnabled:YES]);
  completion(NO);
}

// Tests tapping on the close button just after the sign-out button.
// This is a regression test for crbug.com/371046656.
TEST_F(AccountMenuMediatorTest, TestViewControllerWantToBeClosed) {
  OCMExpect([delegate_ mediatorWantsToBeDismissed:mediator_]);
  OCMExpect([consumer_ setUserInteractionsEnabled:NO]);
  [mediator_
      viewControllerWantsToBeClosed:(AccountMenuViewController*)consumer_];
}
