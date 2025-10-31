// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_mediator.h"

#import "base/containers/flat_map.h"
#import "base/memory/raw_ptr.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "components/sync/test/test_sync_service.h"
#import "components/test/ios/test_utils.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_mediator_delegate.h"
#import "ios/chrome/browser/authentication/account_menu/public/account_menu_constants.h"
#import "ios/chrome/browser/authentication/account_menu/ui/account_menu_consumer.h"
#import "ios/chrome/browser/authentication/account_menu/ui/account_menu_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_account_item.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/avatar_provider.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMArg.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

using ::testing::IsNull;

const FakeSystemIdentity* kPrimaryIdentity = [FakeSystemIdentity fakeIdentity1];
const FakeSystemIdentity* kSecondaryIdentity =
    [FakeSystemIdentity fakeIdentity2];
const FakeSystemIdentity* kSecondaryIdentity2 =
    [FakeSystemIdentity fakeIdentity3];

enum FeaturesState {
  // With all accounts being assigned to the same single profile.
  kWithoutSeparateProfiles,
  // With managed accounts being assigned into their own separate profiles.
  kWithSeparateProfiles
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
      case kWithoutSeparateProfiles:
        feature_states[kSeparateProfilesForManagedAccounts] = false;
        break;
      case kWithSeparateProfiles:
        feature_states[kSeparateProfilesForManagedAccounts] = true;
        break;
    }
    feature_list_.InitWithFeatureStates(feature_states);
  }

  void SetUp() override {
    PlatformTest::SetUp();

    // Set the profile.
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
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
    test_sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_.get());

    AddPrimaryIdentity();
    AddSecondaryIdentity();

    // Set the mediator and its mocks delegate.
    delegate_mock_ =
        OCMStrictProtocolMock(@protocol(AccountMenuMediatorDelegate));
    sync_error_settings_mock_ =
        OCMStrictProtocolMock(@protocol(SyncErrorSettingsCommandHandler));
    consumer_mock_ = OCMStrictProtocolMock(@protocol(AccountMenuConsumer));
    mediator_ = [[AccountMenuMediator alloc]
          initWithSyncService:test_sync_service_
        accountManagerService:account_manager_service_
                  authService:authentication_service_
              identityManager:identity_manager_
                        prefs:profile_->GetPrefs()
                  accessPoint:AccountMenuAccessPoint::kNewTabPage
                          URL:GURL()
         prepareChangeProfile:nil];
    mediator_.delegate = delegate_mock_;
    mediator_.syncErrorSettingsCommandHandler = sync_error_settings_mock_;
    mediator_.consumer = consumer_mock_;
    authentication_flow_mock_ = OCMStrictClassMock([AuthenticationFlow class]);
  }

  void TearDown() override {
    // Avoid dangling service pointers, since `profile_` is destroyed first.
    fake_system_identity_manager_ = nullptr;
    authentication_service_ = nullptr;
    account_manager_service_ = nullptr;
    test_sync_service_ = nullptr;
    identity_manager_ = nullptr;

    [mediator_ disconnect];
    VerifyMock();
    PlatformTest::TearDown();
  }

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
    EXPECT_OCMOCK_VERIFY(sync_error_settings_mock_);
    EXPECT_OCMOCK_VERIFY(consumer_mock_);
    EXPECT_OCMOCK_VERIFY((id)authentication_flow_mock_);
  }

  // Sign in, set the passphrase required, update the mediator, return the
  // account error ui info.
  AccountErrorUIInfo* SignInAndSetPassphraseRequired() {
    test_sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);
    test_sync_service_->SetPassphraseRequired();

    __block AccountErrorUIInfo* errorSentToConsumer = nil;
    OCMExpect([consumer_mock_
        updateErrorSection:[OCMArg checkWithBlock:^BOOL(id value) {
          errorSentToConsumer = value;
          return value;
        }]]);
    test_sync_service_->FireStateChanged();
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
  id<SyncErrorSettingsCommandHandler> sync_error_settings_mock_;
  id<AccountMenuConsumer> consumer_mock_;
  AuthenticationFlow* authentication_flow_mock_;
  AccountMenuMediator* mediator_;
  raw_ptr<ChromeAccountManagerService> account_manager_service_;
  raw_ptr<syncer::TestSyncService> test_sync_service_;
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
  FakeSystemIdentity* thirdIdentity = [FakeSystemIdentity fakeIdentity3];
  // Initially, the user's name isn't known.
  thirdIdentity.userFullName = nil;
  thirdIdentity.userGivenName = nil;
  switch (GetParam()) {
    case kWithSeparateProfiles:
      break;
    case kWithoutSeparateProfiles:
      OCMExpect([consumer_mock_
          updateAccountListWithGaiaIDsToAdd:@[]
                            gaiaIDsToRemove:@[]
                              gaiaIDsToKeep:[OCMArg any]]);
      break;
  }
  OCMExpect([consumer_mock_
      updateAccountListWithGaiaIDsToAdd:@[ thirdIdentity.gaiaId.ToNSString() ]
                        gaiaIDsToRemove:@[]
                          gaiaIDsToKeep:@[
                            kSecondaryIdentity.gaiaId.ToNSString()
                          ]]);
  fake_system_identity_manager_->AddIdentity(thirdIdentity);

  // Simulate that the identity gets updated (e.g. the username became known).
  // This should result in another notification, even though the list of
  // identities is unchanged.
  thirdIdentity.userFullName = @"First Last";
  thirdIdentity.userGivenName = @"First";
  NSArray<NSString*>* gaiaIDsToKeep = @[
    kSecondaryIdentity.gaiaId.ToNSString(), thirdIdentity.gaiaId.ToNSString()
  ];
  OCMExpect([consumer_mock_ updateAccountListWithGaiaIDsToAdd:@[]
                                              gaiaIDsToRemove:@[]
                                                gaiaIDsToKeep:gaiaIDsToKeep]);
  fake_system_identity_manager_->FireIdentityUpdatedNotification(thirdIdentity);
}

// Checks that removing a secondary identity lead to updating the
// consumer.
TEST_P(AccountMenuMediatorTest, TestRemoveSecondaryIdentity) {
  IgnoreAccountListUpdatesWithNoAdditionsOrRemovals();

  OCMExpect([consumer_mock_ updatePrimaryAccount]);

  OCMExpect([consumer_mock_
      updateAccountListWithGaiaIDsToAdd:@[]
                        gaiaIDsToRemove:@[
                          kSecondaryIdentity.gaiaId.ToNSString()
                        ]
                          gaiaIDsToKeep:@[]]);
  {
    base::RunLoop run_loop;
    base::RepeatingClosure closure = run_loop.QuitClosure();
    fake_system_identity_manager_->ForgetIdentity(
        kSecondaryIdentity, base::BindOnce(^(NSError* error) {
          EXPECT_THAT(error, IsNull());
          closure.Run();
        }));
    run_loop.Run();
  }
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Checks that removing the primary identity lead to updating the
// consumer.
TEST_P(AccountMenuMediatorTest, TestRemovePrimaryIdentity) {
  OCMExpect([delegate_mock_
      mediatorWantsToBeDismissed:mediator_
           withCancelationReason:signin_ui::CancelationReason::kFailed
                  signedIdentity:nil
                 userTappedClose:NO]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  authentication_service_->SignOut(signin_metrics::ProfileSignout::kTest, ^(){
                                   });
}

#pragma mark - AccountMenuDataSource

// Tests the result of secondaryAccountsGaiaIDs.
TEST_P(AccountMenuMediatorTest, TestSecondaryAccountsGaiaID) {
  EXPECT_EQ([mediator_ secondaryAccountsGaiaIDs],
            std::vector<GaiaId> { kSecondaryIdentity.gaiaId });
}

#pragma mark - AccountMenuDataSource and SyncObserverModelBridge

// Tests the result of nameForGaiaID.
TEST_P(AccountMenuMediatorTest, nameForGaiaID) {
  EXPECT_NSEQ([mediator_ nameForGaiaID:kSecondaryIdentity.gaiaId],
              kSecondaryIdentity.userFullName);
}

// Tests the result of emailForGaiaID.
TEST_P(AccountMenuMediatorTest, emailForGaiaID) {
  EXPECT_NSEQ([mediator_ emailForGaiaID:kSecondaryIdentity.gaiaId],
              kSecondaryIdentity.userEmail);
}

// Tests the result of imageForGaiaID.
TEST_P(AccountMenuMediatorTest, imageForGaiaID) {
  EXPECT_NSEQ(
      [mediator_ imageForGaiaID:kSecondaryIdentity.gaiaId],
      GetApplicationContext() -> GetIdentityAvatarProvider()
                                  -> GetIdentityAvatar(
                                      kSecondaryIdentity,
                                      IdentityAvatarSize::TableViewIcon));
}

// Tests the result of primaryAccountEmail.
TEST_P(AccountMenuMediatorTest, TestPrimaryAccountEmail) {
  EXPECT_NSEQ([mediator_ primaryAccountEmail], kPrimaryIdentity.userEmail);
}

// Tests the result of primaryAccountUserFullName.
TEST_P(AccountMenuMediatorTest, TestPrimaryAccountUserFullName) {
  EXPECT_NSEQ([mediator_ primaryAccountUserFullName],
              kPrimaryIdentity.userFullName);
}

// Tests the result of primaryAccountAvatar.
TEST_P(AccountMenuMediatorTest, TestPrimaryAccountAvatar) {
  EXPECT_NSEQ([mediator_ primaryAccountAvatar],
              GetApplicationContext() -> GetIdentityAvatarProvider()
                                          -> GetIdentityAvatar(
                                              kPrimaryIdentity,
                                              IdentityAvatarSize::Large));
}

// Tests the result of TestError when there is no error.
TEST_P(AccountMenuMediatorTest, TestNoError) {
  EXPECT_THAT([mediator_ accountErrorUIInfo], IsNull());
}

// Tests the result of TestError when passphrase is required.
TEST_P(AccountMenuMediatorTest, TestError) {
  // In order to simulate requiring a passphrase, test sync service requires
  // us to explicitly set that the setup is not complete, and fire the state
  // change to its observer.

  AccountErrorUIInfo* errorSentToConsumer = SignInAndSetPassphraseRequired();
  AccountErrorUIInfo* expectedError = GetAccountErrorUIInfo(test_sync_service_);

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
  IgnoreAccountListUpdatesWithNoAdditionsOrRemovals();
  // Given that the method  `triggerSignoutWithTargetRect:completion` creates a
  // callback in a callback, this tests has three parts.  One part by callback,
  // and one part for the initial part of the run.

  // Testing the part before the callback.
  const CGRect target = CGRect();
  OCMExpect([consumer_mock_ switchingStarted]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);

  OCMExpect([delegate_mock_ authenticationFlow:kSecondaryIdentity
                                    anchorRect:target])
      .andReturn(authentication_flow_mock_);
  __block id<AuthenticationFlowDelegate> authentication_flow_request_helper =
      nil;
  OCMExpect([authentication_flow_mock_
      setDelegate:[OCMArg checkWithBlock:^(id value) {
        authentication_flow_request_helper = value;
        return mediator_ == value;
      }]]);
  OCMExpect([authentication_flow_mock_ startSignIn]);
  auto gaiaId2 = kSecondaryIdentity.gaiaId;
  [mediator_ accountTappedWithGaiaID:&gaiaId2 targetRect:target];
  // Simulate a double tap. The second tap should be ignored.
  [mediator_ accountTappedWithGaiaID:&gaiaId2 targetRect:target];
  VerifyMock();

  OCMExpect([consumer_mock_ switchingStopped]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:YES]);
  OCMExpect([delegate_mock_ signinFinished]);
  // Simulate AuthenticationFlow failure.
  [authentication_flow_request_helper
      authenticationFlowDidSignInInSameProfileWithCancelationReason:
          signin_ui::CancelationReason::kUserCanceled
                                                           identity:nil];
}

// Tests the result of accountTappedWithGaiaID:targetRect:
// when sign-in fail.
TEST_P(AccountMenuMediatorTest, TestAccountTapedSignInFailed) {
  IgnoreAccountListUpdatesWithNoAdditionsOrRemovals();
  // Given that the method  `signOutFromTargetRect:completion` create
  // a callback in a callback, this tests has three parts.  One part by
  // callback, and one part for the initial part of the run.

  // Testing the part before the callback.
  const CGRect target = CGRect();
  OCMExpect([consumer_mock_ switchingStarted]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  // Simulate a sign-out success.
  // This variable will contain the callback that should be executed once
  // sign-in ended.
  OCMExpect([delegate_mock_ authenticationFlow:kSecondaryIdentity
                                    anchorRect:target])
      .andReturn(authentication_flow_mock_);
  __block id<AuthenticationFlowDelegate> authentication_flow_request_helper =
      nil;
  OCMExpect([authentication_flow_mock_
      setDelegate:[OCMArg checkWithBlock:^(id value) {
        authentication_flow_request_helper = value;
        return mediator_ == value;
      }]]);
  // Simulate account switching.
  OCMExpect([authentication_flow_mock_ startSignIn]);
  auto gaiaId2 = kSecondaryIdentity.gaiaId;
  [mediator_ accountTappedWithGaiaID:&gaiaId2 targetRect:target];
  // Simulate a double tap. The second tap should be ignored.
  [mediator_ accountTappedWithGaiaID:&gaiaId2 targetRect:target];

  // Expect that the consumer unlocks the UI.
  OCMExpect([consumer_mock_ switchingStopped]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:YES]);
  OCMExpect([delegate_mock_ signinFinished]);
  [authentication_flow_request_helper
      authenticationFlowDidSignInInSameProfileWithCancelationReason:
          signin_ui::CancelationReason::kFailed
                                                           identity:nil];

  // Checks the user is signed-back in.
  ASSERT_EQ(kPrimaryIdentity, authentication_service_->GetPrimaryIdentity(
                                  signin::ConsentLevel::kSignin));
}

// Tests the result of accountTappedWithGaiaID:targetRect:
// when switch is successful.
TEST_P(AccountMenuMediatorTest, TestAccountTapedWithSuccessfulSwitch) {
  // Given that the method  `signOutFromTargetRect:callback` create a
  // callback in a callback, this tests has three parts.  One part by callback,
  // and one part for the initial part of the run.

  // Testing the part before the callback.
  const CGRect target = CGRect();
  OCMExpect([consumer_mock_ switchingStarted]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  // Simulate account switching.
  OCMExpect([delegate_mock_ authenticationFlow:kSecondaryIdentity
                                    anchorRect:target])
      .andReturn(authentication_flow_mock_);
  __block id<AuthenticationFlowDelegate> authentication_flow_request_helper =
      nil;
  OCMExpect([authentication_flow_mock_
      setDelegate:[OCMArg checkWithBlock:^(id value) {
        authentication_flow_request_helper = value;
        return mediator_ == value;
      }]]);
  OCMExpect([authentication_flow_mock_ startSignIn]);
  auto gaiaId2 = kSecondaryIdentity.gaiaId;
  [mediator_ accountTappedWithGaiaID:&gaiaId2 targetRect:target];
  // Simulate a double tap. The second tap should be ignored.
  [mediator_ accountTappedWithGaiaID:&gaiaId2 targetRect:target];
  VerifyMock();
  OCMExpect([delegate_mock_
      mediatorWantsToBeDismissed:mediator_
           withCancelationReason:signin_ui::CancelationReason::kNotCanceled
                  signedIdentity:kSecondaryIdentity
                 userTappedClose:NO]);
  OCMExpect([delegate_mock_ signinFinished]);
  [authentication_flow_request_helper
      authenticationFlowDidSignInInSameProfileWithCancelationReason:
          signin_ui::CancelationReason::kNotCanceled
                                                           identity:
                                                               kSecondaryIdentity];
}

// Tests the result of didTapErrorButton when a passphrase is required.
TEST_P(AccountMenuMediatorTest, TestTapErrorButtonPassphrase) {
  // While many errors can be displayed by the account menu, this test suite
  // only consider the error where the passphrase is needed. This is because,
  // when the suite was written, `TestSyncService::GetUserActionableError` could
  // only returns `kNeedsPassphrase` and `kSignInNeedsUpdate`. Furthermore,
  // `kSignInNeedsUpdate` is not an error displayed to the user (technically,
  // `GetAccountErrorUIInfo` returns `nil` on `kSignInNeedsUpdate`.)
  SignInAndSetPassphraseRequired();
  OCMExpect([sync_error_settings_mock_
      openPassphraseDialogWithModalPresentation:YES]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  [mediator_ didTapErrorButton];
  // Simulate a double tap. The second tap should be ignored.
  [mediator_ didTapErrorButton];
  EXPECT_EQ(1, user_actions_.GetActionCount(
                   "Signin_AccountMenu_ErrorButton_Passphrase"));
}

// Tests the effect of didTapManageYourGoogleAccount.
TEST_P(AccountMenuMediatorTest, TestDidTapManageYourGoogleAccount) {
  OCMExpect([delegate_mock_ didTapManageYourGoogleAccount]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  [mediator_ didTapManageYourGoogleAccount];
  // Simulate a double tap. The second tap should be ignored.
  [mediator_ didTapManageYourGoogleAccount];
}

// Tests the effect of didTapManageAccounts.
TEST_P(AccountMenuMediatorTest, TestDidTapEditAccountList) {
  OCMExpect([delegate_mock_ didTapManageAccounts]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  [mediator_ didTapManageAccounts];

  // Simulate a double tap. Nothing should occurs on the second one.
  [mediator_ didTapManageAccounts];
}

// Tests the effect of didTapAddAccount on iOS26+.
// The expected behavior depends on iOS version because of a UIKit but up to
// iOS 18. See crbug.com/395959814.
TEST_P(AccountMenuMediatorTest, TestDidTapAddAccountiOS26) {
  if (!@available(iOS 26, *)) {
    return;
  }
  IgnoreAccountListUpdatesWithNoAdditionsOrRemovals();
  OCMExpect([delegate_mock_ didTapAddAccount]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  [mediator_ didTapAddAccount];
  // Simulate a double tap. The second tap should be ignored.
  [mediator_ didTapAddAccount];
  OCMExpect([consumer_mock_ switchingStopped]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:YES]);
  [mediator_ accountMenuIsUsable];
}

// Tests the effect of didTapAddAccount on iOS 18 and less.
TEST_P(AccountMenuMediatorTest, TestDidTapAddAccount) {
  if (@available(iOS 26, *)) {
    return;
  }
  IgnoreAccountListUpdatesWithNoAdditionsOrRemovals();
  OCMExpect([delegate_mock_ didTapAddAccount]);
  OCMExpect([delegate_mock_ didTapAddAccount]);
  [mediator_ didTapAddAccount];
  // Simulate a double tap. The second tap should be transmitted because the add
  // account view may have disapperead.
  [mediator_ didTapAddAccount];
  OCMExpect([consumer_mock_ switchingStopped]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:YES]);
  [mediator_ accountMenuIsUsable];
}

// Tests the effect of signOutFromTargetRect.
TEST_P(AccountMenuMediatorTest, TestSignoutFromTargetRect) {
  CGRect rect = CGRectMake(0, 0, 40, 24);

  __block signin_ui::SignoutCompletionCallback completion = nil;
  OCMExpect([delegate_mock_
      signOutFromTargetRect:rect
                 completion:AssignValueToVariable(completion)]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  [mediator_ signOutFromTargetRect:rect];
  // Simulate a double tap. The second tap should be ignored.
  [mediator_ signOutFromTargetRect:rect];
  OCMExpect([delegate_mock_
      mediatorWantsToBeDismissed:mediator_
           withCancelationReason:signin_ui::CancelationReason::kUserCanceled
                  signedIdentity:nil
                 userTappedClose:NO]);
  completion(YES, nil);
}

// Tests tapping on the close button just after the sign-out button.
// This is a regression test for crbug.com/371046656.
TEST_P(AccountMenuMediatorTest, TestSignoutAndClose) {
  CGRect rect = CGRectMake(0, 0, 40, 24);
  __block signin_ui::SignoutCompletionCallback completion = nil;
  OCMExpect([delegate_mock_
      signOutFromTargetRect:rect
                 completion:AssignValueToVariable(completion)]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  [mediator_ signOutFromTargetRect:rect];
  // Simulate a double tap. The second tap should be ignored.
  [mediator_ signOutFromTargetRect:rect];
  [mediator_ disconnect];
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:YES]);
  completion(NO, nil);
}

// Tests tapping on the close button just after the sign-out button.
// This is a regression test for crbug.com/371046656.
TEST_P(AccountMenuMediatorTest, TestViewControllerWantToBeClosed) {
  OCMExpect([delegate_mock_
      mediatorWantsToBeDismissed:mediator_
           withCancelationReason:signin_ui::CancelationReason::kUserCanceled
                  signedIdentity:nil
                 userTappedClose:YES]);
  OCMExpect([consumer_mock_ setUserInteractionsEnabled:NO]);
  [mediator_
      viewControllerWantsToBeClosed:(AccountMenuViewController*)consumer_mock_];
}

// Tests that the consumer is not notified to update the error section multiple
// times if the underlying error does not change.
TEST_P(AccountMenuMediatorTest, TestErrorSectionUptadedOnceForSameError) {
  SignInAndSetPassphraseRequired();

  // The error has not changed. The consumer should not be notified again.
  OCMReject([consumer_mock_ updateErrorSection:[OCMArg any]]);
  test_sync_service_->FireStateChanged();
}

// Tests that the consumer is notified to update the error section if the
// underlying error is resolved.
TEST_P(AccountMenuMediatorTest, TestErrorSectionUpdatedWhenErrorCleared) {
  test_sync_service_->SetSignedIn(signin::ConsentLevel::kSignin);
  constexpr char kSyncPassphrase[] = "passphrase";
  test_sync_service_->GetUserSettings()->SetPassphraseRequired(kSyncPassphrase);

  OCMExpect([consumer_mock_ updateErrorSection:[OCMArg any]]);
  test_sync_service_->FireStateChanged();
  EXPECT_EQ([mediator_ accountErrorUIInfo].errorType,
            syncer::SyncService::UserActionableError::kNeedsPassphrase);

  // Resolve the error. The consumer should be notified.
  OCMExpect([consumer_mock_ updateErrorSection:[OCMArg any]]);
  test_sync_service_->GetUserSettings()->SetDecryptionPassphrase(
      kSyncPassphrase);
  test_sync_service_->FireStateChanged();
  EXPECT_THAT([mediator_ accountErrorUIInfo], IsNull());
}

INSTANTIATE_TEST_SUITE_P(,
                         AccountMenuMediatorTest,
                         testing::ValuesIn({kWithoutSeparateProfiles,
                                            kWithSeparateProfiles}),
                         [](const testing::TestParamInfo<FeaturesState>& info) {
                           switch (info.param) {
                             case kWithoutSeparateProfiles:
                               return "WithoutSeparateProfiles";
                             case kWithSeparateProfiles:
                               return "WithSeparateProfiles";
                           }
                         });
