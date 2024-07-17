// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator.h"

#import "base/run_loop.h"
#import "base/test/task_environment.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/sync/test/test_sync_service.h"
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
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

const FakeSystemIdentity* kPrimaryIdentity = [FakeSystemIdentity fakeIdentity1];
const FakeSystemIdentity* kSecondaryIdentity =
    [FakeSystemIdentity fakeIdentity2];
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
    browser_state_ = builder.Build();

    // Set the manager and services variables.
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    fake_system_identity_manager_ =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    account_manager_service_ =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            browser_state_.get());
    authentication_service_ =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();

    AddPrimaryIdentity();

    // Set the mediator and its mocks delegate.
    delegate_ = OCMStrictProtocolMock(@protocol(AccountMenuMediatorDelegate));
    consumer_ = OCMStrictProtocolMock(@protocol(AccountMenuConsumer));
    mediator_ = [[AccountMenuMediator alloc]
          initWithSyncService:sync_service()
        accountManagerService:account_manager_service_
                  authService:authentication_service_
              identityManager:identity_test_env_.identity_manager()];
    mediator_.delegate = delegate_;
    mediator_.consumer = consumer_;
  }

  void TearDown() override {
    [mediator_ disconnect];
    EXPECT_OCMOCK_VERIFY(delegate_);
    EXPECT_OCMOCK_VERIFY(consumer_);
    PlatformTest::TearDown();
  }

  syncer::TestSyncService* sync_service() { return test_sync_service_.get(); }

 protected:
  // Add a secondary identity
  void AddSecondaryIdentity() {
    fake_system_identity_manager_->AddIdentity(kSecondaryIdentity);
  }

  FakeSystemIdentityManager* GetSystemIdentityManager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  id<AccountMenuMediatorDelegate> delegate_;
  id<AccountMenuConsumer> consumer_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  AccountMenuMediator* mediator_;
  ChromeAccountManagerService* account_manager_service_;
  AuthenticationService* authentication_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  FakeSystemIdentityManager* fake_system_identity_manager_;

 private:
  // Signs in a fake identity.
  void AddPrimaryIdentity() {
    fake_system_identity_manager_->AddIdentity(kPrimaryIdentity);
    authentication_service_->SignIn(
        kPrimaryIdentity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }
};

// Checks that adding and removing a secondary identity lead to updating the
// consumer.
TEST_F(AccountMenuMediatorTest, TestAddAndRemoveSecondaryIdentity) {
  OCMExpect([consumer_
      updateAccountListWithGaiaIDsToAdd:@[ kSecondaryIdentity.gaiaID ]
                        gaiaIDsToRemove:@[]]);
  OCMExpect([consumer_ updatePrimaryAccount]);
  AddSecondaryIdentity();
  EXPECT_OCMOCK_VERIFY(consumer_);

  OCMExpect([consumer_
      updateAccountListWithGaiaIDsToAdd:@[]
                        gaiaIDsToRemove:@[ kSecondaryIdentity.gaiaID ]]);
  OCMExpect([consumer_ updatePrimaryAccount]);
  {
    base::RunLoop run_loop;
    base::RepeatingClosure closure = run_loop.QuitClosure();
    fake_system_identity_manager_->ForgetIdentity(
        kSecondaryIdentity, base::BindOnce(^(NSError* error) {
          ASSERT_THAT(error, testing::IsNull());
          closure.Run();
        }));
    run_loop.Run();
  }
}
