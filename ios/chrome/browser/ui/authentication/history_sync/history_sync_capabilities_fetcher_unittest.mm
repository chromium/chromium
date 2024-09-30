// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_capabilities_fetcher.h"

#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_mock_clock_override.h"
#import "base/time/time.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

const char kTestEmail[] = "test@gmail.com";

class HistorySyncCapabilitiesFetcherTest
    : public PlatformTest,
      public ::testing::WithParamInterface<signin::Tribool> {
 public:
  signin::Tribool ExpectedCapabilityValue() const { return GetParam(); }

  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
  }

  void TearDown() override {
    ASSERT_TRUE(fetcher_);
    [fetcher_ shutdown];
    PlatformTest::TearDown();
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  HistorySyncCapabilitiesFetcher* BuildHistorySyncCapabilitiesFetcher() {
    HistorySyncCapabilitiesFetcher* fetcher =
        [[HistorySyncCapabilitiesFetcher alloc]
            initWithIdentityManager:identity_manager()];
    return fetcher;
  }

  AccountInfo SignInPrimaryAccount() {
    // Sign in AccountInfo.
    AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);
    return account;
  }

  void SetAccountInfoCanShowUnrestrictedOptInsCapability(
      AccountInfo account,
      signin::Tribool capability) {
    AccountCapabilitiesTestMutator mutator(&account.capabilities);
    mutator.set_can_show_history_sync_opt_ins_without_minor_mode_restrictions(
        capability == signin::Tribool::kTrue);
    identity_test_env_.UpdateAccountInfoForAccount(account);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestProfileIOS> profile_;
  HistorySyncCapabilitiesFetcher* fetcher_ = nil;
};

// Tests that startFetchingRestrictionCapability will process the AccountInfo
// capability CanShowHistorySyncOptInsWithoutMinorModeRestrictions if its value
// is already available.
TEST_P(HistorySyncCapabilitiesFetcherTest,
       TestFetchingAvailableAccountInfoCapabilities) {
  base::HistogramTester histogram_tester;
  signin::Tribool expected_capability = ExpectedCapabilityValue();

  // Make account capabilities available before the fetcher is created.
  AccountInfo account = SignInPrimaryAccount();
  SetAccountInfoCanShowUnrestrictedOptInsCapability(account,
                                                    expected_capability);

  base::RunLoop run_loop;
  CapabilityFetchCompletionCallback callback = base::BindLambdaForTesting(
      [&run_loop, expected_capability](signin::Tribool capability) {
        EXPECT_EQ(capability, expected_capability);
        run_loop.Quit();
      });

  // Create the fetcher and attempt to fetch existing capabilities.
  fetcher_ = BuildHistorySyncCapabilitiesFetcher();
  EXPECT_EQ([fetcher_ canShowUnrestrictedOptInsCapability],
            expected_capability);

  [fetcher_ startFetchingRestrictionCapabilityWithCallback:std::move(callback)];
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

// Tests that the account capability is processed on AccountInfo received.
TEST_P(HistorySyncCapabilitiesFetcherTest,
       TestAccountInfoReceivedWithCapability) {
  base::HistogramTester histogram_tester;
  signin::Tribool expected_capability = ExpectedCapabilityValue();
  fetcher_ = BuildHistorySyncCapabilitiesFetcher();

  // Sign in AccountInfo without capabilities setup.
  AccountInfo account = SignInPrimaryAccount();
  EXPECT_EQ([fetcher_ canShowUnrestrictedOptInsCapability],
            signin::Tribool::kUnknown);

  // Set up the callback.
  base::RunLoop run_loop;
  CapabilityFetchCompletionCallback callback = base::BindLambdaForTesting(
      [&run_loop, expected_capability](signin::Tribool capability) {
        EXPECT_EQ(capability, expected_capability);
        run_loop.Quit();
      });

  // Test that onExtendedAccountInfoUpdated can be executed during async
  // capability fetching.
  [fetcher_ startFetchingRestrictionCapabilityWithCallback:std::move(callback)];

  // Set up AccountInfo capabilities.
  SetAccountInfoCanShowUnrestrictedOptInsCapability(account,
                                                    expected_capability);
  EXPECT_EQ([fetcher_ canShowUnrestrictedOptInsCapability],
            expected_capability);

  // Trigger onExtendedAccountInfoUpdated
  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"", "full_name", "given_name", "locale",
      /*picture_url=*/"");

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

// Tests that the fallback capability value is processed on fetch deadline.
TEST_P(HistorySyncCapabilitiesFetcherTest, TestCapabilityFetchDeadline) {
  base::HistogramTester histogram_tester;
  base::ScopedMockClockOverride scoped_clock;
  base::RunLoop run_loop;

  // Create the fetcher and wait for capability fetch timeout.
  CapabilityFetchCompletionCallback callback =
      base::BindLambdaForTesting([&run_loop](signin::Tribool capability) {
        EXPECT_EQ(capability, signin::Tribool::kUnknown);
        run_loop.Quit();
      });

  fetcher_ = BuildHistorySyncCapabilitiesFetcher();

  [fetcher_ startFetchingRestrictionCapabilityWithCallback:std::move(callback)];
  scoped_clock.Advance(kMinorModeRestrictionsFetchDeadline);
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

INSTANTIATE_TEST_SUITE_P(,
                         HistorySyncCapabilitiesFetcherTest,
                         /*ExpectedCapabilityValue*/
                         ::testing::Values(signin::Tribool::kFalse,
                                           signin::Tribool::kTrue));

}  // namespace
