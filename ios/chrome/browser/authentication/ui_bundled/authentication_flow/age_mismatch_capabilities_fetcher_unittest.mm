// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/age_mismatch_capabilities_fetcher.h"

#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

using signin::CapabilityFetchCompletionCallback;

constexpr char kTestEmail[] = "test@gmail.com";

void ExpectCapabilityResultHistogram(
    const base::HistogramTester& histogram_tester,
    const signin::Tribool& expected_capability) {
  CanSignInToChromeCapabilityResult expected_result;
  switch (expected_capability) {
    case signin::Tribool::kFalse:
      expected_result = CanSignInToChromeCapabilityResult::kFalse;
      break;
    case signin::Tribool::kTrue:
      expected_result = CanSignInToChromeCapabilityResult::kTrue;
      break;
    case signin::Tribool::kUnknown:
      expected_result = CanSignInToChromeCapabilityResult::kTimeout;
      break;
  }
  histogram_tester.ExpectBucketCount(
      "Signin.AccountCapabilities.CanSignInToChrome.FetchResult",
      static_cast<int>(expected_result), 1);
}

class AgeMismatchCapabilitiesFetcherTest
    : public PlatformTest,
      public ::testing::WithParamInterface<signin::Tribool> {
 public:
  AgeMismatchCapabilitiesFetcherTest() {
    feature_list_.InitAndEnableFeature(
        switches::kEnforceCanSignInToChromeCapability);
  }

  signin::Tribool ExpectedCapabilityValue() const { return GetParam(); }

  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
  }

  void TearDown() override {
    ASSERT_TRUE(fetcher_);
    [fetcher_ shutdown];
    fetcher_ = nil;
    PlatformTest::TearDown();
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  AgeMismatchCapabilitiesFetcher* BuildAgeMismatchCapabilitiesFetcher() {
    AgeMismatchCapabilitiesFetcher* fetcher =
        [[AgeMismatchCapabilitiesFetcher alloc]
            initWithIdentityManager:identity_manager()];
    return fetcher;
  }

  AccountInfo SignInPrimaryAccount() {
    AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);
    return account;
  }

  void SetAccountInfoCanSignInToChromeCapability(AccountInfo account,
                                                 signin::Tribool capability) {
    account = signin::WithGeneratedUserInfo(account, "given_name");
    AccountCapabilitiesTestMutator mutator(&account);
    mutator.set_can_sign_in_to_chrome(capability == signin::Tribool::kTrue);
    identity_test_env_.UpdateAccountInfoForAccount(account);
  }

 protected:
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestProfileIOS> profile_;
  AgeMismatchCapabilitiesFetcher* fetcher_ = nil;
};

TEST_P(AgeMismatchCapabilitiesFetcherTest,
       TestFetchingAvailableAccountInfoCapabilities) {
  base::HistogramTester histogram_tester;
  const signin::Tribool expected_capability = ExpectedCapabilityValue();

  AccountInfo account = SignInPrimaryAccount();
  SetAccountInfoCanSignInToChromeCapability(account, expected_capability);

  base::RunLoop run_loop;
  CapabilityFetchCompletionCallback callback = base::BindLambdaForTesting(
      [&run_loop, expected_capability](signin::Tribool capability) {
        EXPECT_EQ(capability, expected_capability);
        run_loop.Quit();
      });

  fetcher_ = BuildAgeMismatchCapabilitiesFetcher();
  EXPECT_EQ(
      [fetcher_ canSignInToChromeCapabilityForAccount:account.GetAccountId()],
      expected_capability);

  [fetcher_
      startFetchingCanSignInToChromeCapabilityWithCallback:std::move(callback)
                                                forAccount:account
                                                               .GetAccountId()];
  run_loop.Run();

  histogram_tester.ExpectTotalCount(
      "Signin.AccountCapabilities.CanSignInToChrome.FetchDuration", 1);
  ExpectCapabilityResultHistogram(histogram_tester, expected_capability);
}

TEST_P(AgeMismatchCapabilitiesFetcherTest,
       TestAccountInfoReceivedWithCapability) {
  base::HistogramTester histogram_tester;
  fetcher_ = BuildAgeMismatchCapabilitiesFetcher();
  AccountInfo account = SignInPrimaryAccount();

  base::RunLoop run_loop;
  const signin::Tribool expected_capability = ExpectedCapabilityValue();
  CapabilityFetchCompletionCallback callback = base::BindLambdaForTesting(
      [&run_loop, expected_capability](signin::Tribool capability) {
        EXPECT_EQ(capability, expected_capability);
        run_loop.Quit();
      });

  [fetcher_
      startFetchingCanSignInToChromeCapabilityWithCallback:std::move(callback)
                                                forAccount:account
                                                               .GetAccountId()];

  // Simulate successful fetch before timeout.
  SetAccountInfoCanSignInToChromeCapability(account, expected_capability);
  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.GetAccountId(), account.GetEmail(), account.GetGaiaId(),
      /*hosted_domain=*/"", "full_name", "given_name", "locale",
      /*picture_url=*/"");
  run_loop.Run();

  ExpectCapabilityResultHistogram(histogram_tester, expected_capability);
  histogram_tester.ExpectTotalCount(
      "Signin.AccountCapabilities.CanSignInToChrome.FetchDuration", 1);
}

TEST_P(AgeMismatchCapabilitiesFetcherTest, TestCapabilityFetchDeadline) {
  base::HistogramTester histogram_tester;
  fetcher_ = BuildAgeMismatchCapabilitiesFetcher();
  AccountInfo account = SignInPrimaryAccount();

  bool callback_called = false;
  CapabilityFetchCompletionCallback callback = base::BindLambdaForTesting(
      [&callback_called](signin::Tribool capability) {
        EXPECT_EQ(capability, signin::Tribool::kUnknown);
        callback_called = true;
      });

  [fetcher_
      startFetchingCanSignInToChromeCapabilityWithCallback:std::move(callback)
                                                forAccount:account
                                                               .GetAccountId()];

  // Fast forward time to trigger timeout.
  task_environment_.FastForwardBy(kCanSignInToChromeCapabilityFetchTimeout);
  EXPECT_TRUE(callback_called);

  histogram_tester.ExpectTotalCount(
      "Signin.AccountCapabilities.CanSignInToChrome.FetchDuration", 1);
  ExpectCapabilityResultHistogram(histogram_tester, signin::Tribool::kUnknown);
}

TEST_P(AgeMismatchCapabilitiesFetcherTest, TestConcurrentFetches) {
  const signin::Tribool expected_capability = ExpectedCapabilityValue();
  fetcher_ = BuildAgeMismatchCapabilitiesFetcher();

  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("test1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("test2@gmail.com");

  EXPECT_EQ(
      [fetcher_ canSignInToChromeCapabilityForAccount:account1.GetAccountId()],
      signin::Tribool::kUnknown);
  EXPECT_EQ(
      [fetcher_ canSignInToChromeCapabilityForAccount:account2.GetAccountId()],
      signin::Tribool::kUnknown);

  base::RunLoop run_loop1;
  base::RunLoop run_loop2;

  CapabilityFetchCompletionCallback callback1 = base::BindLambdaForTesting(
      [&run_loop1, expected_capability](signin::Tribool capability) {
        EXPECT_EQ(capability, expected_capability);
        run_loop1.Quit();
      });

  CapabilityFetchCompletionCallback callback2 = base::BindLambdaForTesting(
      [&run_loop2, expected_capability](signin::Tribool capability) {
        EXPECT_EQ(capability, expected_capability);
        run_loop2.Quit();
      });

  [fetcher_
      startFetchingCanSignInToChromeCapabilityWithCallback:std::move(callback1)
                                                forAccount:account1
                                                               .GetAccountId()];
  [fetcher_
      startFetchingCanSignInToChromeCapabilityWithCallback:std::move(callback2)
                                                forAccount:account2
                                                               .GetAccountId()];

  SetAccountInfoCanSignInToChromeCapability(account1, expected_capability);
  SetAccountInfoCanSignInToChromeCapability(account2, expected_capability);

  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account1.GetAccountId(), account1.GetEmail(), account1.GetGaiaId(),
      /*hosted_domain=*/"", "full_name", "given_name", "locale",
      /*picture_url=*/"");
  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account2.GetAccountId(), account2.GetEmail(), account2.GetGaiaId(),
      /*hosted_domain=*/"", "full_name", "given_name", "locale",
      /*picture_url=*/"");

  run_loop1.Run();
  run_loop2.Run();
}

INSTANTIATE_TEST_SUITE_P(,
                         AgeMismatchCapabilitiesFetcherTest,
                         ::testing::Values(signin::Tribool::kFalse,
                                           signin::Tribool::kTrue));

}  // namespace
