// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_mediator.h"

#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_consumer.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

const char kTestEmail[] = "test@gmail.com";

class HistorySyncMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    browser_state_ = builder.Build();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    feature_list_.InitAndEnableFeature(
        switches::kMinorModeRestrictionsForHistorySyncOptIn);
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)consumer_mock_);
    PlatformTest::TearDown();
  }

  FakeSystemIdentityManager* GetSystemIdentityManager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  HistorySyncMediator* BuildHistorySyncMediator(bool show_user_email) {
    ChromeAccountManagerService* chrome_account_manager_service =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            browser_state_.get());
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForBrowserState(browser_state_.get());
    HistorySyncMediator* mediator = [[HistorySyncMediator alloc]
        initWithAuthenticationService:auth_service
          chromeAccountManagerService:chrome_account_manager_service
                      identityManager:identity_manager()
                          syncService:sync_service
                        showUserEmail:show_user_email];
    mediator.consumer = consumer_mock_;
    return mediator;
  }

  AccountInfo SignInPrimaryAccountWithCanShowUnrestrictedOptInsCapability(
      bool value) {
    AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account.capabilities);
    mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
        value);
    identity_test_env_.UpdateAccountInfoForAccount(account);
    return account;
  }

  void SystemSignInWithCanShowUnrestrictedOptInsCapability(bool value) {
    const FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    GetSystemIdentityManager()->AddIdentity(identity);
    AccountCapabilitiesTestMutator* mutator =
        GetSystemIdentityManager()->GetCapabilitiesMutator(identity);
    mutator->set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
        value);

    AuthenticationServiceFactory::GetForBrowserState(browser_state_.get())
        ->SignIn(identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

 protected:
  id<HistorySyncConsumer> consumer_mock_ =
      OCMProtocolMock(@protocol(HistorySyncConsumer));
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::Options::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests that startFetchingCapabilities will process the AccountInfo capability
// CanShowHistorySyncOptInsWithoutMinorModeRestrictions if its value is already
// available.
TEST_F(HistorySyncMediatorTest,
       TestStartFetchingCapabilitiesWithAccountCapabilityValueTrue) {
  OCMExpect([consumer_mock_ displayButtonsWithRestrictionStatus:NO]);

  // Make account capabilities available before the mediator is created.
  SignInPrimaryAccountWithCanShowUnrestrictedOptInsCapability(true);

  // Create the mediator and attempt to fetch existing capabilities.
  HistorySyncMediator* mediator =
      BuildHistorySyncMediator(/*show_user_email=*/false);
  [mediator startFetchingCapabilitiesWithCompletion:nil];
}

// Tests that startFetchingCapabilities will process the AccountInfo capability
// if its value is already available.
TEST_F(HistorySyncMediatorTest,
       TestStartFetchingCapabilitiesWithAccountCapabilityValueFalse) {
  OCMExpect([consumer_mock_ displayButtonsWithRestrictionStatus:YES]);

  // Make account capabilities available before the mediator is created.
  SignInPrimaryAccountWithCanShowUnrestrictedOptInsCapability(false);

  // Create the mediator and attempt to fetch existing capabilities.
  HistorySyncMediator* mediator =
      BuildHistorySyncMediator(/*show_user_email=*/false);
  [mediator startFetchingCapabilitiesWithCompletion:nil];
}

// Tests that the account capability is processed on AccountInfo received.
TEST_F(HistorySyncMediatorTest,
       TestAccountInfoReceivedWithCapabilityValuedTrue) {
  OCMExpect([consumer_mock_ displayButtonsWithRestrictionStatus:NO]);

  // Create the mediator and ensure its lifetime.
  HistorySyncMediator* mediator =
      BuildHistorySyncMediator(/*show_user_email=*/false);
  ASSERT_TRUE(mediator);

  // Create AccountInfo and trigger onExtendedAccountInfoUpdated.
  AccountInfo account =
      SignInPrimaryAccountWithCanShowUnrestrictedOptInsCapability(true);
  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"", "full_name", "given_name", "locale",
      /*picture_url=*/"");
}

// Tests that the account capability is processed on AccountInfo received.
TEST_F(HistorySyncMediatorTest,
       TestAccountInfoReceivedWithCapabilityValuedFalse) {
  OCMExpect([consumer_mock_ displayButtonsWithRestrictionStatus:YES]);

  // Create the mediator and ensure its lifetime.
  HistorySyncMediator* mediator =
      BuildHistorySyncMediator(/*show_user_email=*/false);
  ASSERT_TRUE(mediator);

  // Create AccountInfo and trigger onExtendedAccountInfoUpdated.
  AccountInfo account =
      SignInPrimaryAccountWithCanShowUnrestrictedOptInsCapability(false);
  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"", "full_name", "given_name", "locale",
      /*picture_url=*/"");
}

// Tests that the system capability is processed.
TEST_F(HistorySyncMediatorTest, TestSystemCapabilityValuedTrue) {
  OCMExpect([consumer_mock_ displayButtonsWithRestrictionStatus:NO]);

  // Create the mediator.
  HistorySyncMediator* mediator =
      BuildHistorySyncMediator(/*show_user_email=*/false);

  SystemSignInWithCanShowUnrestrictedOptInsCapability(true);

  // Start fetching and wait until the capability is processed.
  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;
  [mediator
      startFetchingCapabilitiesWithCompletion:^(BOOL actionButtonsUpdated) {
        if (actionButtonsUpdated) {
          run_loop_ptr->Quit();
        }
      }];
  run_loop.Run();
}

// Tests that the system capability is processed.
TEST_F(HistorySyncMediatorTest, TestSystemCapabilityValuedFalse) {
  OCMExpect([consumer_mock_ displayButtonsWithRestrictionStatus:YES]);

  // Create the mediator.
  HistorySyncMediator* mediator =
      BuildHistorySyncMediator(/*show_user_email=*/false);

  SystemSignInWithCanShowUnrestrictedOptInsCapability(false);

  // Start fetching and wait until the capability is processed.
  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;
  [mediator
      startFetchingCapabilitiesWithCompletion:^(BOOL actionButtonsUpdated) {
        if (actionButtonsUpdated) {
          run_loop_ptr->Quit();
        }
      }];
  run_loop.Run();
}

// Tests that the fallback capability value is processed on fetch deadline.
TEST_F(HistorySyncMediatorTest, TestCapabilityFetchDeadline) {
  OCMExpect([consumer_mock_ displayButtonsWithRestrictionStatus:YES]);

  // Create the mediator without setting up capabilities.
  HistorySyncMediator* mediator =
      BuildHistorySyncMediator(/*show_user_email=*/false);

  // Start the timer and wait until the fallback capability is processed.
  base::RunLoop run_loop;
  base::RunLoop* run_loop_ptr = &run_loop;
  [mediator
      viewAppearedWithHiddenButtonsWithCompletion:^(BOOL actionButtonsUpdated) {
        if (actionButtonsUpdated) {
          run_loop_ptr->Quit();
        }
      }];
  run_loop.Run();
}

}  // namespace
