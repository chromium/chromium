// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_capabilities_fetcher.h"

#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/scoped_mock_clock_override.h"
#import "base/time/time.h"
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

// Short timeout to wait for asynchronously fetching already available system
// capabilities.
constexpr base::TimeDelta kFetchImmediatelyAvailableCapabilityDeadline =
    base::Milliseconds(20);

class HistorySyncCapabilitiesFetcherTest
    : public PlatformTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  bool IsFetchingImmediatelyAvailableCapabilities() const {
    return std::get<0>(GetParam());
  }

  bool ExpectedCapabilityValue() const { return std::get<1>(GetParam()); }

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
TEST_P(HistorySyncCapabilitiesFetcherTest,
       TestFetchingAvailableAccountInfoCapabilities) {
  base::HistogramTester histogram_tester;
  bool expected_capability = ExpectedCapabilityValue();

  // Make account capabilities available before the fetcher is created.
  AccountInfo account = SignInPrimaryAccount();
  SetAccountInfoCanShowUnrestrictedOptInsCapability(account,
                                                    expected_capability);

  base::RunLoop run_loop;
  CapabilityFetchCompletionCallback callback = base::BindLambdaForTesting(
      [&run_loop, expected_capability](bool capability) {
        EXPECT_EQ(capability, expected_capability);
        run_loop.Quit();
      });

  // Create the fetcher and attempt to fetch existing capabilities.
  fetcher_ = BuildHistorySyncCapabilitiesFetcher();

  if (IsFetchingImmediatelyAvailableCapabilities()) {
    [fetcher_ fetchImmediatelyAvailableRestrictionCapabilityWithCallback:
                  std::move(callback)];
    run_loop.Run();

    // Do not record metrics when fetching immediately available capabilities.
    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.ImmediatelyAvailable", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.UserVisibleLatency", 0);
    histogram_tester.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                      0);

  } else {
    [fetcher_
        startFetchingRestrictionCapabilityWithCallback:std::move(callback)];
    run_loop.Run();

    histogram_tester.ExpectUniqueSample(
        "Signin.AccountCapabilities.ImmediatelyAvailable", true, 1);
    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.ImmediatelyAvailable", 1);
    histogram_tester.ExpectUniqueSample(
        "Signin.AccountCapabilities.UserVisibleLatency", 0, 1);
    histogram_tester.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                      0);
  }
}

// Tests that the account capability is processed on AccountInfo received.
TEST_P(HistorySyncCapabilitiesFetcherTest,
       TestAccountInfoReceivedWithCapability) {
  base::HistogramTester histogram_tester;
  bool expected_capability = ExpectedCapabilityValue();
  fetcher_ = BuildHistorySyncCapabilitiesFetcher();

  // Sign in AccountInfo without capabilities setup.
  AccountInfo account = SignInPrimaryAccount();

  // Set up the callback.
  base::RunLoop run_loop;
  CapabilityFetchCompletionCallback callback = base::BindLambdaForTesting(
      [&run_loop, expected_capability](bool capability) {
        EXPECT_EQ(capability, expected_capability);
        run_loop.Quit();
      });

  if (IsFetchingImmediatelyAvailableCapabilities()) {
    [fetcher_ fetchImmediatelyAvailableRestrictionCapabilityWithCallback:
                  std::move(callback)];
  } else {
    [fetcher_
        startFetchingRestrictionCapabilityWithCallback:std::move(callback)];
  }

  // Set up AccountInfo capabilities.
  SetAccountInfoCanShowUnrestrictedOptInsCapability(account,
                                                    expected_capability);

  // Trigger onExtendedAccountInfoUpdated
  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"", "full_name", "given_name", "locale",
      /*picture_url=*/"");

  run_loop.Run();

  if (IsFetchingImmediatelyAvailableCapabilities()) {
    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.ImmediatelyAvailable", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.UserVisibleLatency", 0);
    histogram_tester.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                      0);

  } else {
    histogram_tester.ExpectUniqueSample(
        "Signin.AccountCapabilities.ImmediatelyAvailable", false, 1);
    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.ImmediatelyAvailable", 1);
    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.UserVisibleLatency", 1);
    histogram_tester.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                      1);
  }
}

// Tests that the system capability is processed.
TEST_P(HistorySyncCapabilitiesFetcherTest, TestFetchingSystemCapability) {
  base::HistogramTester histogram_tester;
  bool expected_capability = ExpectedCapabilityValue();
  SystemSignInWithCanShowUnrestrictedOptInsCapability(expected_capability);

  base::RunLoop run_loop;
  CapabilityFetchCompletionCallback callback = base::BindLambdaForTesting(
      [&run_loop, expected_capability](bool capability) {
        EXPECT_EQ(capability, expected_capability);
        run_loop.Quit();
      });

  // Create the fetcher and attempt to fetch existing system capabilities.
  fetcher_ = BuildHistorySyncCapabilitiesFetcher();
  if (IsFetchingImmediatelyAvailableCapabilities()) {
    [fetcher_ fetchImmediatelyAvailableRestrictionCapabilityWithCallback:
                  std::move(callback)];
    run_loop.Run();
    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.ImmediatelyAvailable", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.UserVisibleLatency", 0);
    histogram_tester.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                      0);

  } else {
    [fetcher_
        startFetchingRestrictionCapabilityWithCallback:std::move(callback)];
    run_loop.Run();

    histogram_tester.ExpectUniqueSample(
        "Signin.AccountCapabilities.ImmediatelyAvailable", false, 1);
    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.ImmediatelyAvailable", 1);
    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.UserVisibleLatency", 1);
    histogram_tester.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                      1);
  }
}

// Tests that the fallback capability value is processed on fetch deadline.
TEST_P(HistorySyncCapabilitiesFetcherTest, TestCapabilityFetchDeadline) {
  base::HistogramTester histogram_tester;
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
  if (IsFetchingImmediatelyAvailableCapabilities()) {
    [fetcher_ fetchImmediatelyAvailableRestrictionCapabilityWithCallback:
                  std::move(callback)];
    scoped_clock.Advance(kFetchImmediatelyAvailableCapabilityDeadline);
    run_loop.Run();

    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.ImmediatelyAvailable", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.UserVisibleLatency", 0);
    histogram_tester.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                      0);
  } else {
    [fetcher_
        startFetchingRestrictionCapabilityWithCallback:std::move(callback)];
    scoped_clock.Advance(base::Milliseconds(
        switches::kMinorModeRestrictionsFetchDeadlineMs.Get()));
    run_loop.Run();

    histogram_tester.ExpectUniqueSample(
        "Signin.AccountCapabilities.ImmediatelyAvailable", false, 1);
    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.ImmediatelyAvailable", 1);
    histogram_tester.ExpectTotalCount(
        "Signin.AccountCapabilities.UserVisibleLatency", 1);
    histogram_tester.ExpectTotalCount("Signin.AccountCapabilities.FetchLatency",
                                      1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    HistorySyncCapabilitiesFetcherTest,
    ::testing::Combine(
        /*IsFetchingImmediatelyAvailableCapabilities*/ ::testing::Bool(),
        /*ExpectedCapabilityValue*/ ::testing::Bool()));

}  // namespace
