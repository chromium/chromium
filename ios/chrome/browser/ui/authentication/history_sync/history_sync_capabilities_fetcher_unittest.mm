// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_capabilities_fetcher.h"

#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/scoped_mock_clock_override.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

const char kTestEmail[] = "test@gmail.com";

class HistorySyncCapabilitiesFetcherTest : public PlatformTest {
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
    feature_list_
        .InitWithFeatures(/*enabled_features=*/
                          {switches::kMinorModeRestrictionsForHistorySyncOptIn,
                           switches::
                               kUseSystemCapabilitiesForMinorModeRestrictions},
                          /*disabled_features=*/{});
  }

  void TearDown() override {
    ASSERT_TRUE(fetcher_);
    [fetcher_ shutdown];
    PlatformTest::TearDown();
  }

  FakeSystemIdentityManager* GetSystemIdentityManager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  HistorySyncCapabilitiesFetcher* BuildHistorySyncCapabilitiesFetcher() {
    AuthenticationService* auth_service =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
    HistorySyncCapabilitiesFetcher* fetcher =
        [[HistorySyncCapabilitiesFetcher alloc]
            initWithAuthenticationService:auth_service
                          identityManager:identity_manager()];
    return fetcher;
  }

  AccountInfo SignInPrimaryAccount() {
    // Sign in SystemIdentity with unknown capabilities.
    const FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    GetSystemIdentityManager()->AddIdentityWithUnknownCapabilities(identity);
    AuthenticationServiceFactory::GetForBrowserState(browser_state_.get())
        ->SignIn(identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
    // Sign in AccountInfo.
    AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);
    return account;
  }

  void SetAccountInfoCanShowUnrestrictedOptInsCapability(AccountInfo account,
                                                         bool value) {
    AccountCapabilitiesTestMutator mutator(&account.capabilities);
    mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
        value);
    identity_test_env_.UpdateAccountInfoForAccount(account);
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
  web::WebTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList feature_list_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  HistorySyncCapabilitiesFetcher* fetcher_ = nil;
};

// Tests that startFetchingRestrictionCapability will process the AccountInfo
// capability CanShowHistorySyncOptInsWithoutMinorModeRestrictions if its value
// is already available.
TEST_F(HistorySyncCapabilitiesFetcherTest,
       TestStartFetchingCapabilitiesWithAccountCapabilityValueTrue) {
  // Make account capabilities available before the mediator is created.
  AccountInfo account = SignInPrimaryAccount();
  SetAccountInfoCanShowUnrestrictedOptInsCapability(account, true);

  base::RunLoop run_loop;
  CapabilityFetchCompletionCallback callback =
      base::BindLambdaForTesting([&run_loop](bool capability) {
        EXPECT_TRUE(capability);
        run_loop.Quit();
      });

  // Create the fetcher and attempt to fetch existing capabilities.
  fetcher_ = BuildHistorySyncCapabilitiesFetcher();
  [fetcher_
      fetchImmediatelyAvailableRestrictionCapabilityWithCallback:std::move(
                                                                     callback)];

  run_loop.Run();
}

// Tests that startFetchingRestrictionCapability will process the AccountInfo
// capability if its value is already available.
TEST_F(HistorySyncCapabilitiesFetcherTest,
       TestStartFetchingCapabilitiesWithAccountCapabilityValueFalse) {
  // Make account capabilities available before the mediator is created.
  AccountInfo account = SignInPrimaryAccount();
  SetAccountInfoCanShowUnrestrictedOptInsCapability(account, false);

  base::RunLoop run_loop;
  CapabilityFetchCompletionCallback callback =
      base::BindLambdaForTesting([&run_loop](bool capability) {
        EXPECT_FALSE(capability);
        run_loop.Quit();
      });

  // Create the fetcher and attempt to fetch existing capabilities.
  fetcher_ = BuildHistorySyncCapabilitiesFetcher();
  [fetcher_
      fetchImmediatelyAvailableRestrictionCapabilityWithCallback:std::move(
                                                                     callback)];

  run_loop.Run();
}

// Tests that the account capability is processed on AccountInfo received.
TEST_F(HistorySyncCapabilitiesFetcherTest,
       TestAccountInfoReceivedWithCapabilityValuedTrue) {
  fetcher_ = BuildHistorySyncCapabilitiesFetcher();

  // Sign in AccountInfo without capabilities setup.
  AccountInfo account = SignInPrimaryAccount();

  // Set up the callback.
  base::RunLoop run_loop;
  CapabilityFetchCompletionCallback callback =
      base::BindLambdaForTesting([&run_loop](bool capability) {
        EXPECT_TRUE(capability);
        run_loop.Quit();
      });
  [fetcher_ startFetchingRestrictionCapabilityWithCallback:std::move(callback)];

  // Set up AccountInfo capabilities.
  SetAccountInfoCanShowUnrestrictedOptInsCapability(account, true);

  // Trigger onExtendedAccountInfoUpdated
  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"", "full_name", "given_name", "locale",
      /*picture_url=*/"");

  run_loop.Run();
}

// Tests that the account capability is processed on AccountInfo received.
TEST_F(HistorySyncCapabilitiesFetcherTest,
       TestAccountInfoReceivedWithCapabilityValuedFalse) {
  fetcher_ = BuildHistorySyncCapabilitiesFetcher();

  // Sign in AccountInfo without capabilities setup.
  AccountInfo account = SignInPrimaryAccount();

  // Set up the callback.
  base::RunLoop run_loop;
  CapabilityFetchCompletionCallback callback =
      base::BindLambdaForTesting([&run_loop](bool capability) {
        EXPECT_FALSE(capability);
        run_loop.Quit();
      });
  [fetcher_ startFetchingRestrictionCapabilityWithCallback:std::move(callback)];

  // Set up AccountInfo capabilities.
  SetAccountInfoCanShowUnrestrictedOptInsCapability(account, false);

  // Trigger onExtendedAccountInfoUpdated
  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"", "full_name", "given_name", "locale",
      /*picture_url=*/"");

  run_loop.Run();
}

// Tests that the system capability is processed.
TEST_F(HistorySyncCapabilitiesFetcherTest, TestSystemCapabilityValuedTrue) {
  SystemSignInWithCanShowUnrestrictedOptInsCapability(true);

  base::RunLoop run_loop;
  CapabilityFetchCompletionCallback callback =
      base::BindLambdaForTesting([&run_loop](bool capability) {
        EXPECT_TRUE(capability);
        run_loop.Quit();
      });

  // Create the fetcher and attempt to fetch existing system capabilities.
  fetcher_ = BuildHistorySyncCapabilitiesFetcher();
  [fetcher_
      fetchImmediatelyAvailableRestrictionCapabilityWithCallback:std::move(
                                                                     callback)];

  run_loop.Run();
}

// Tests that the system capability is processed.
TEST_F(HistorySyncCapabilitiesFetcherTest, TestSystemCapabilityValuedFalse) {
  SystemSignInWithCanShowUnrestrictedOptInsCapability(false);

  base::RunLoop run_loop;
  CapabilityFetchCompletionCallback callback =
      base::BindLambdaForTesting([&run_loop](bool capability) {
        EXPECT_FALSE(capability);
        run_loop.Quit();
      });

  // Create the fetcher and attempt to fetch existing system capabilities.
  fetcher_ = BuildHistorySyncCapabilitiesFetcher();
  [fetcher_
      fetchImmediatelyAvailableRestrictionCapabilityWithCallback:std::move(
                                                                     callback)];

  run_loop.Run();
}

// Tests that the fallback capability value is processed on fetch deadline.
TEST_F(HistorySyncCapabilitiesFetcherTest, TestCapabilityFetchDeadline) {
  base::ScopedMockClockOverride scoped_clock;

  // Sign in fake identity without setting up capabilities.
  const FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
  GetSystemIdentityManager()->AddIdentityWithUnknownCapabilities(identity);
  AuthenticationServiceFactory::GetForBrowserState(browser_state_.get())
      ->SignIn(identity, signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);

  base::RunLoop run_loop;

  // Create the fetcher and wait for capability fetch timeout.
  CapabilityFetchCompletionCallback callback =
      base::BindLambdaForTesting([&run_loop](bool capability) {
        EXPECT_FALSE(capability);
        run_loop.Quit();
      });

  fetcher_ = BuildHistorySyncCapabilitiesFetcher();
  [fetcher_ startFetchingRestrictionCapabilityWithCallback:std::move(callback)];

  scoped_clock.Advance(base::Milliseconds(
      switches::kMinorModeRestrictionsFetchDeadlineMs.Get()));

  run_loop.Run();
}

}  // namespace
