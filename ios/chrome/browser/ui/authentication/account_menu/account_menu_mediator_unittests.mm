// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator.h"

#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
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
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
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

    // Set the browser state.
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateMockSyncService));
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = std::move(builder).Build();

    // Set the manager and services variables.
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    fake_system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    authentication_service_ =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            browser_state_.get());
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    identity_manager_ =
        IdentityManagerFactory::GetForBrowserState(browser_state_.get());

    AddPrimaryIdentity();
    AddSecondaryIdentity();

    // Set the mediator and its mocks delegate.
    delegate_ = OCMStrictProtocolMock(@protocol(AccountMenuMediatorDelegate));
    consumer_ = OCMStrictProtocolMock(@protocol(AccountMenuConsumer));
    mediator_ = [[AccountMenuMediator alloc]
          initWithSyncService:SyncService()
        accountManagerService:account_manager_service_
                  authService:authentication_service_
              identityManager:identity_manager_];
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
    base::RunLoop run_loop;
    base::RepeatingClosure closure = run_loop.QuitClosure();
    SyncService()->SetInitialSyncFeatureSetupComplete(false);
    SyncService()->SetPassphraseRequired();

    __block AccountErrorUIInfo* errorSentToConsumer = nil;
    OCMExpect(
        [consumer_ updateErrorSection:[OCMArg checkWithBlock:^BOOL(id value) {
                     errorSentToConsumer = value;
                     closure.Run();
                     return value;
                   }]]);
    SyncService()->FireStateChanged();
    run_loop.Run();
    return errorSentToConsumer;
  }

  FakeSystemIdentityManager* GetSystemIdentityManager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  id<AccountMenuMediatorDelegate> delegate_;
  id<AccountMenuConsumer> consumer_;
  AccountMenuMediator* mediator_;
  ChromeAccountManagerService* account_manager_service_;
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  AuthenticationService* authentication_service_;
  FakeSystemIdentityManager* fake_system_identity_manager_;
  signin::IdentityManager* identity_manager_;

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
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

#pragma mark - Test for ChromeAccountManagerServiceObserver

// Checks that adding a secondary identity lead to updating the
// consumer.
TEST_F(AccountMenuMediatorTest, TestAddSecondaryIdentity) {
  const FakeSystemIdentity* thirdIdentity = [FakeSystemIdentity fakeIdentity3];
  OCMExpect([consumer_
      updateAccountListWithGaiaIDsToAdd:@[ thirdIdentity.gaiaID ]
                        gaiaIDsToRemove:@[]]);
  OCMExpect([consumer_ updatePrimaryAccount]);
  fake_system_identity_manager_->AddIdentity(thirdIdentity);
}

// Checks that removing a secondary identity lead to updating the
// consumer.
TEST_F(AccountMenuMediatorTest, TestRemoveSecondaryIdentity) {
  OCMExpect([consumer_
      updateAccountListWithGaiaIDsToAdd:@[]
                        gaiaIDsToRemove:@[ kSecondaryIdentity.gaiaID ]]);
  OCMExpect([consumer_ updatePrimaryAccount]);
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
  {
    base::RunLoop run_loop;
    base::RepeatingClosure closure = run_loop.QuitClosure();
    authentication_service_->SignOut(signin_metrics::ProfileSignout::kTest,
                                     /*force_clear_browsing_data=*/false, ^() {
                                       closure.Run();
                                     });
    run_loop.Run();
  }
}

#pragma mark - AccountMenuDataSource

// Tests the result of secondaryAccountsGaiaIDs.
TEST_F(AccountMenuMediatorTest, TestSecondaryAccountsGaiaID) {
  EXPECT_NSEQ([mediator_ secondaryAccountsGaiaIDs],
              @[ kSecondaryIdentity.gaiaID ]);
}

#pragma mark - AccountMenuDataSource and SyncObserverModelBridge

// Tests the result of secondaryAccountsGaiaIDs.
TEST_F(AccountMenuMediatorTest, identityItemForGaiaID) {
  TableViewAccountItem* item =
      [mediator_ identityItemForGaiaID:kSecondaryIdentity.gaiaID];
  EXPECT_NSEQ(item.text, kSecondaryIdentity.userFullName);
  EXPECT_NSEQ(item.detailText, kSecondaryIdentity.userEmail);
  EXPECT_NSEQ(item.image,
              account_manager_service_->GetIdentityAvatarWithIdentity(
                  kSecondaryIdentity, IdentityAvatarSize::TableViewIcon));
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
TEST_F(AccountMenuMediatorTest, TestAccountTaped) {
  // Given that the method  `triggerSignoutWithTargetRect:completion` create a
  // callback in a callback, this tests has three parts.  One part by callback,
  // and one part for the initial part of the run.

  // Testing the part before the callback.
  auto target = CGRect();
  // This variable will contain the callback that should be executed once
  // sign-out is done.
  __block void (^onSignoutSuccess)(BOOL) = nil;
  {
    base::RunLoop run_loop;
    base::RepeatingClosure closure = run_loop.QuitClosure();
    OCMExpect([delegate_
        triggerSignoutWithTargetRect:target
                          completion:[OCMArg checkWithBlock:^BOOL(id value) {
                            onSignoutSuccess = value;
                            closure.Run();
                            return true;
                          }]]);
    [mediator_ accountTappedWithGaiaID:kSecondaryIdentity.gaiaID
                            targetRect:target];
    run_loop.Run();
  }
  VerifyMock();

  // Testing the sign-out callback.
  // This variable will contain the callback that should be executed once
  // sign-in is done.
  __block void (^onSigninSuccess)(id<SystemIdentity>) = nil;
  {
    base::RunLoop run_loop;
    base::RepeatingClosure closure = run_loop.QuitClosure();
    OCMExpect([delegate_
        triggerSigninWithSystemIdentity:kSecondaryIdentity
                             completion:[OCMArg checkWithBlock:^BOOL(id value) {
                               onSigninSuccess = value;
                               closure.Run();
                               return true;
                             }]]);
    onSignoutSuccess(true);
    run_loop.Run();
  }
  VerifyMock();

  // Testing the sign-in callback.
  OCMExpect(
      [delegate_ triggerAccountSwitchSnackbarWithIdentity:kSecondaryIdentity]);
  OCMExpect([delegate_ mediatorWantsToBeDismissed:mediator_]);
  onSigninSuccess(kSecondaryIdentity);
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
}
