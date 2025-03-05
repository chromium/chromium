// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_mediator.h"

#import "base/containers/flat_map.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_account_item.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_consumer.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_mediator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
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
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMArg.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

const FakeSystemIdentity* kPrimaryIdentity = [FakeSystemIdentity fakeIdentity1];
const FakeSystemIdentity* kSecondaryIdentity =
    [FakeSystemIdentity fakeIdentity2];
const FakeSystemIdentity* kSecondaryIdentity2 =
    [FakeSystemIdentity fakeIdentity3];

enum FeaturesState {
  // Using APIs from ChromeAccountManagerService to retrieve accounts, and with
  // all accounts being assigned to the same single profile.
  kOldApiWithoutSeparateProfiles,
  // Using APIs from IdentityManager to retrieve accounts, and with all accounts
  // being assigned to the same single profile.
  kNewApiWithoutSeparateProfiles,
  // Using APIs from IdentityManager to retrieve accounts, and with managed
  // accounts being assigned into their own separate profiles.
  kNewApiWithSeparateProfiles
  // Note: "SeparateProfiles" depends on "NewApi", so there's no
  // "OldAPiWithSeparateProfiles" variant.
};
}  // namespace

// The test param determines whether `kSeparateProfilesForManagedAccounts` is
// enabled.
class AccountMenuMediatorTest
    : public PlatformTest,
      public testing::WithParamInterface<FeaturesState> {
 public:
  AccountMenuMediatorTest() {
    base::flat_map<base::test::FeatureRef, bool> feature_states;
    switch (GetParam()) {
      case kOldApiWithoutSeparateProfiles:
        feature_states[kUseAccountListFromIdentityManager] = false;
        feature_states[kSeparateProfilesForManagedAccounts] = false;
        break;
      case kNewApiWithoutSeparateProfiles:
        feature_states[kUseAccountListFromIdentityManager] = true;
        feature_states[kSeparateProfilesForManagedAccounts] = false;
        break;
      case kNewApiWithSeparateProfiles:
        feature_states[kUseAccountListFromIdentityManager] = true;
        feature_states[kSeparateProfilesForManagedAccounts] = true;
        break;
        // Note: `kSeparateProfilesForManagedAccounts` depends on
        // `kUseAccountListFromIdentityManager`, so there's no "false + true"
        // case.
    }
    feature_list_.InitWithFeatureStates(feature_states);
  }

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
    delegate_mock_ =
        OCMStrictProtocolMock(@protocol(AccountMenuMediatorDelegate));
    consumer_mock_ = OCMStrictProtocolMock(@protocol(AccountMenuConsumer));
    mediator_ = [[AccountMenuMediator alloc]
          initWithSyncService:SyncService()
        accountManagerService:account_manager_service_
                  authService:authentication_service_
              identityManager:identity_manager_
                        prefs:profile_->GetPrefs()];
    mediator_.delegate = delegate_mock_;
    mediator_.consumer = consumer_mock_;
    authentication_flow_mock_ = OCMStrictClassMock([AuthenticationFlow class]);
  }

  void TearDown() override {
    [mediator_ disconnect];
    VerifyMock();
    PlatformTest::TearDown();
  }

  syncer::TestSyncService* SyncService() { return test_sync_service_.get(); }

 protected:
  // Ignores any `updateAccountListWithGaiaIDsToAdd` calls on `mock_consumer_`
  // where no accounts were added or removed (i.e. just account details were
  // updated, or it was just a no-op update).
  void IgnoreAccountListUpdatesWithNoAdditionsOrRemovals() {
    OCMStub([consumer_mock_ updateAccountListWithGaiaIDsToAdd:@[]
                                              gaiaIDsToRemove:@[]
                                                gaiaIDsToKeep:[OCMArg any]]);
  }

  // Verify that all mocks expectation are fulfilled.
  void VerifyMock() {
    EXPECT_OCMOCK_VERIFY(delegate_mock_);
    EXPECT_OCMOCK_VERIFY(consumer_mock_);
    EXPECT_OCMOCK_VERIFY((id)authentication_flow_mock_);
  }

  // Set the passphrase required, update the mediator, return the account error
  // ui info.
  AccountErrorUIInfo* setPassphraseRequired() {
    SyncService()->SetInitialSyncFeatureSetupComplete(false);
    SyncService()->SetPassphraseRequired();

    __block AccountErrorUIInfo* errorSentToConsumer = nil;
    OCMExpect([consumer_mock_
        updateErrorSection:[OCMArg checkWithBlock:^BOOL(id value) {
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
                                     ^() {
                                       closure.Run();
                                     });
    run_loop.Run();
  }

  base::test::ScopedFeatureList feature_list_;

  id<AccountMenuMediatorDelegate> delegate_mock_;
  id<AccountMenuConsumer> consumer_mock_;
  AuthenticationFlow* authentication_flow_mock_;
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
    authentication_service_->SignIn(kPrimaryIdentity,
                                    signin_metrics::AccessPoint::kUnknown);
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
TEST_P(AccountMenuMediatorTest, TestAddSecondaryIdentity) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  FakeSystemIdentity* thirdIdentity = [FakeSystemIdentity fakeIdentity3];
  // Initially, the user's name isn't known.
  thirdIdentity.userFullName = nil;
  thirdIdentity.userGivenName = nil;
  switch (GetParam()) {
    case kOldApiWithoutSeparateProfiles:
    case kNewApiWithSeparateProfiles:
      break;
    case kNewApiWithoutSeparateProfiles:
      OCMExpect([consumer_mock_
          updateAccountListWithGaiaIDsToAdd:@[]
                            gaiaIDsToRemove:@[]
                              gaiaIDsToKeep:[OCMArg any]]);
      break;
  }
  OCMExpect([consumer_mock_
      updateAccountListWithGaiaIDsToAdd:@[ thirdIdentity.gaiaID ]
                        gaiaIDsToRemove:@[]
                          gaiaIDsToKeep:@[ kSecondaryIdentity.gaiaID ]]);
  fake_system_identity_manager_->AddIdentity(thirdIdentity);

  // Simulate that the identity gets updated (e.g. the username became known).
  // This should result in another notification, even though the list of
  // identities is unchanged.
  thirdIdentity.userFullName = @"First Last";
  thirdIdentity.userGivenName = @"First";
  NSArray<NSString*>* gaiaIDsToKeep =
      @[ kSecondaryIdentity.gaiaID, thirdIdentity.gaiaID ];
  OCMExpect([consumer_mock_ updateAccountListWithGaiaIDsToAdd:@[]
                                              gaiaIDsToRemove:@[]
                                                gaiaIDsToKeep:gaiaIDsToKeep]);
  fake_system_identity_manager_->FireIdentityUpdatedNotification(thirdIdentity);
}

// Checks that removing a secondary identity lead to updating the
// consumer.
TEST_P(AccountMenuMediatorTest, TestRemoveSecondaryIdentity) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  IgnoreAccountListUpdatesWithNoAdditionsOrRemovals();

  OCMExpect([consumer_mock_ updatePrimaryAccount]);

  OCMExpect([consumer_mock_
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
TEST_P(AccountMenuMediatorTest, TestRemovePrimaryIdentity) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  OCMExpect([delegate_mock_
      mediatorWantsToBeDismissed:mediator_
                      withResult:SigninCoordinatorResultInterrupted
                  signedIdentity:nil]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  authentication_service_->SignOut(signin_metrics::ProfileSignout::kTest, ^(){
                                   });
}

#pragma mark - AccountMenuDataSource

// Tests the result of secondaryAccountsGaiaIDs.
TEST_P(AccountMenuMediatorTest, TestSecondaryAccountsGaiaID) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  EXPECT_NSEQ([mediator_ secondaryAccountsGaiaIDs],
              @[ kSecondaryIdentity.gaiaID ]);
}

#pragma mark - AccountMenuDataSource and SyncObserverModelBridge

// Tests the result of nameForGaiaID.
TEST_P(AccountMenuMediatorTest, nameForGaiaID) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  EXPECT_NSEQ([mediator_ nameForGaiaID:kSecondaryIdentity.gaiaID],
              kSecondaryIdentity.userFullName);
}

// Tests the result of emailForGaiaID.
TEST_P(AccountMenuMediatorTest, emailForGaiaID) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  EXPECT_NSEQ([mediator_ emailForGaiaID:kSecondaryIdentity.gaiaID],
              kSecondaryIdentity.userEmail);
}

// Tests the result of imageForGaiaID.
TEST_P(AccountMenuMediatorTest, imageForGaiaID) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  EXPECT_NSEQ([mediator_ imageForGaiaID:kSecondaryIdentity.gaiaID],
              account_manager_service_ -> GetIdentityAvatarWithIdentity(
                                           kSecondaryIdentity,
                                           IdentityAvatarSize::TableViewIcon));
}

// Tests the result of primaryAccountEmail.
TEST_P(AccountMenuMediatorTest, TestPrimaryAccountEmail) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  EXPECT_NSEQ([mediator_ primaryAccountEmail], kPrimaryIdentity.userEmail);
}

// Tests the result of primaryAccountUserFullName.
TEST_P(AccountMenuMediatorTest, TestPrimaryAccountUserFullName) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  EXPECT_NSEQ([mediator_ primaryAccountUserFullName],
              kPrimaryIdentity.userFullName);
}

// Tests the result of primaryAccountAvatar.
TEST_P(AccountMenuMediatorTest, TestPrimaryAccountAvatar) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  EXPECT_NSEQ([mediator_ primaryAccountAvatar],
              account_manager_service_ -> GetIdentityAvatarWithIdentity(
                                           kPrimaryIdentity,
                                           IdentityAvatarSize::Large));
}

// Tests the result of TestError when there is no error.
TEST_P(AccountMenuMediatorTest, TestNoError) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  EXPECT_THAT([mediator_ accountErrorUIInfo], testing::IsNull());
}

// Tests the result of TestError when passphrase is required.
TEST_P(AccountMenuMediatorTest, TestError) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
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
TEST_P(AccountMenuMediatorTest, TestAccountTapedSignoutFailed) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  IgnoreAccountListUpdatesWithNoAdditionsOrRemovals();
  // Given that the method  `triggerSignoutWithTargetRect:completion` creates a
  // callback in a callback, this tests has three parts.  One part by callback,
  // and one part for the initial part of the run.

  // Testing the part before the callback.
  // This variable will contain the callback that should be executed once
  // sign-out ends.
  __block signin_ui::SignoutCompletionCallback signoutCallback = nil;
  __block signin_ui::SigninCompletionCallback signinCallback = nil;
  const CGRect target = CGRect();
  OCMExpect([consumer_mock_ switchingStarted]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  OCMExpect([delegate_mock_
                triggerSigninWithSystemIdentity:kSecondaryIdentity
                                     anchorRect:target
                                     completion:[OCMArg checkWithBlock:^BOOL(
                                                            id value) {
                                       signinCallback = value;
                                       return true;
                                     }]])
      .andReturn(authentication_flow_mock_);
  [mediator_ accountTappedWithGaiaID:kSecondaryIdentity.gaiaID
                          targetRect:target];
  VerifyMock();

  OCMExpect([consumer_mock_ switchingStopped]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:YES]);
  // Simulate AuthenticationFlow failure.
  signinCallback(SigninCoordinatorResultCanceledByUser);
  EXPECT_EQ(signoutCallback, nil);
}

// Tests the result of accountTappedWithGaiaID:targetRect:
// when sign-in fail.
TEST_P(AccountMenuMediatorTest, TestAccountTapedSignInFailed) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  IgnoreAccountListUpdatesWithNoAdditionsOrRemovals();
  // Given that the method  `signOutFromTargetRect:completion` create
  // a callback in a callback, this tests has three parts.  One part by
  // callback, and one part for the initial part of the run.

  // Testing the part before the callback.
  // This variable will contain the callback that should be executed once
  // sign-out ends.
  __block signin_ui::SigninCompletionCallback signinCallback = nil;
  const CGRect target = CGRect();
  OCMExpect([consumer_mock_ switchingStarted]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);

  // Simulate a sign-out success.
  // This variable will contain the callback that should be executed once
  // sign-in ended.
  OCMExpect([delegate_mock_
                triggerSigninWithSystemIdentity:kSecondaryIdentity
                                     anchorRect:target
                                     completion:[OCMArg checkWithBlock:^BOOL(
                                                            id value) {
                                       signinCallback = value;
                                       return true;
                                     }]])
      .andReturn(authentication_flow_mock_);
  // Simulate account switching.
  [mediator_ accountTappedWithGaiaID:kSecondaryIdentity.gaiaID
                          targetRect:target];

  // Expect that the consumer unlocks the UI.
  OCMExpect([consumer_mock_ switchingStopped]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:YES]);
  signinCallback(SigninCoordinatorResult::SigninCoordinatorResultInterrupted);

  // Checks the user is signed-back in.
  ASSERT_EQ(kPrimaryIdentity, authentication_service_->GetPrimaryIdentity(
                                  signin::ConsentLevel::kSignin));
}

// Tests the result of accountTappedWithGaiaID:targetRect:
// when switch is successful.
TEST_P(AccountMenuMediatorTest, TestAccountTapedWithSuccessfulSwitch) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  // Given that the method  `signOutFromTargetRect:callback` create a
  // callback in a callback, this tests has three parts.  One part by callback,
  // and one part for the initial part of the run.

  // Testing the part before the callback.
  // This variable will contain the callback that should be executed once
  // sign-out ends.
  __block signin_ui::SigninCompletionCallback signinCallback = nil;
  const CGRect target = CGRect();
  OCMExpect([consumer_mock_ switchingStarted]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  // Simulate account switching.
  OCMExpect([delegate_mock_
                triggerSigninWithSystemIdentity:kSecondaryIdentity
                                     anchorRect:target
                                     completion:[OCMArg checkWithBlock:^BOOL(
                                                            id value) {
                                       signinCallback = value;
                                       return true;
                                     }]])
      .andReturn(authentication_flow_mock_);
  [mediator_ accountTappedWithGaiaID:kSecondaryIdentity.gaiaID
                          targetRect:target];
  VerifyMock();

  OCMExpect([delegate_mock_
      triggerAccountSwitchSnackbarWithIdentity:kSecondaryIdentity]);
  OCMExpect([delegate_mock_
      mediatorWantsToBeDismissed:mediator_
                      withResult:SigninCoordinatorResultSuccess
                  signedIdentity:kSecondaryIdentity]);
  signinCallback(SigninCoordinatorResultSuccess);
}

// Tests the result of didTapErrorButton when a passphrase is required.
TEST_P(AccountMenuMediatorTest, TestTapErrorButtonPassphrase) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  // While many errors can be displayed by the account menu, this test suite
  // only consider the error where the passphrase is needed. This is because,
  // when the suite was written, `TestSyncService::GetUserActionableError` could
  // only returns `kNeedsPassphrase` and `kSignInNeedsUpdate`. Furthermore,
  // `kSignInNeedsUpdate` is not an error displayed to the user (technically,
  // `GetAccountErrorUIInfo` returns `nil` on `kSignInNeedsUpdate`.)
  setPassphraseRequired();
  OCMExpect([delegate_mock_ openPassphraseDialogWithModalPresentation:YES]);
  [mediator_ didTapErrorButton];
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "Signin_AccountMenu_ErrorButton_Passphrase"));
}

// Tests the effect of didTapManageYourGoogleAccount.
TEST_P(AccountMenuMediatorTest, TestDidTapManageYourGoogleAccount) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  OCMExpect([delegate_mock_ didTapManageYourGoogleAccount]);
  [mediator_ didTapManageYourGoogleAccount];
}

// Tests the effect of didTapManageAccounts.
TEST_P(AccountMenuMediatorTest, TestDidTapEditAccountList) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  OCMExpect([delegate_mock_ didTapManageAccounts]);
  [mediator_ didTapManageAccounts];
}

// Tests the effect of didTapAddAccount.
TEST_P(AccountMenuMediatorTest, TestDidTapAddAccount) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  IgnoreAccountListUpdatesWithNoAdditionsOrRemovals();
  __block SigninCoordinatorCompletionCallback completion = nil;
  OCMExpect([delegate_mock_
      didTapAddAccountWithCompletion:[OCMArg checkWithBlock:^BOOL(id value) {
        completion = value;
        return true;
      }]]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  [mediator_ didTapAddAccount];
  OCMExpect([consumer_mock_ switchingStopped]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:YES]);
  completion(SigninCoordinatorResult::SigninCoordinatorResultInterrupted, nil);
}

// Tests the effect of signOutFromTargetRect.
TEST_P(AccountMenuMediatorTest, TestSignoutFromTargetRect) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  CGRect rect = CGRectMake(0, 0, 40, 24);

  __block void (^completion)(BOOL) = nil;
  OCMExpect([delegate_mock_
      signOutFromTargetRect:rect
                 completion:[OCMArg checkWithBlock:^BOOL(id value) {
                   completion = value;
                   return true;
                 }]]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  [mediator_ signOutFromTargetRect:rect];
  OCMExpect([delegate_mock_
      mediatorWantsToBeDismissed:mediator_
                      withResult:SigninCoordinatorResultCanceledByUser
                  signedIdentity:nil]);
  completion(YES);
}

// Tests tapping on the close button just after the sign-out button.
// This is a regression test for crbug.com/371046656.
TEST_P(AccountMenuMediatorTest, TestSignoutAndClose) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  CGRect rect = CGRectMake(0, 0, 40, 24);
  __block void (^completion)(BOOL) = nil;
  OCMExpect([delegate_mock_
      signOutFromTargetRect:rect
                 completion:[OCMArg checkWithBlock:^BOOL(id value) {
                   completion = value;
                   return true;
                 }]]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  [mediator_ signOutFromTargetRect:rect];
  [mediator_ disconnect];
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:YES]);
  completion(NO);
}

// Tests tapping on the close button just after the sign-out button.
// This is a regression test for crbug.com/371046656.
TEST_P(AccountMenuMediatorTest, TestViewControllerWantToBeClosed) {
  if (!@available(iOS 17, *)) {
    if (GetParam() == kNewApiWithSeparateProfiles) {
      // Separate profiles are only available in iOS 17+.
      return;
    }
  }
  OCMExpect([delegate_mock_
      mediatorWantsToBeDismissed:mediator_
                      withResult:SigninCoordinatorResultCanceledByUser
                  signedIdentity:nil]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  [mediator_
      viewControllerWantsToBeClosed:(AccountMenuViewController*)consumer_mock_];
}

INSTANTIATE_TEST_SUITE_P(,
                         AccountMenuMediatorTest,
                         testing::ValuesIn({kOldApiWithoutSeparateProfiles,
                                            kNewApiWithoutSeparateProfiles,
                                            kNewApiWithSeparateProfiles}),
                         [](const testing::TestParamInfo<FeaturesState>& info) {
                           switch (info.param) {
                             case kOldApiWithoutSeparateProfiles:
                               return "OldApiWithoutSeparateProfile";
                             case kNewApiWithoutSeparateProfiles:
                               return "NewApiWithoutSeparateProfiles";
                             case kNewApiWithSeparateProfiles:
                               return "NewApiWithSeparateProfiles";
                           }
                         });
