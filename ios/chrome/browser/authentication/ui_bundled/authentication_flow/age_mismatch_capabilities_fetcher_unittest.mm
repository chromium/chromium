// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/age_mismatch_capabilities_fetcher.h"

#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/scoped_mock_clock_override.h"
#import "base/time/time.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

namespace {

using signin::CapabilityFetchCompletionCallback;

constexpr char kTestEmail[] = "test@gmail.com";

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
    account = signin::WithGeneratedUserInfo(account, "Test");
    AccountCapabilitiesTestMutator mutator(&account.capabilities);
    mutator.set_can_sign_in_to_chrome(capability == signin::Tribool::kTrue);
    signin::UpdateAccountInfoForAccount(identity_manager(), account);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestProfileIOS> profile_;
  AgeMismatchCapabilitiesFetcher* fetcher_ = nil;
};

TEST_P(AgeMismatchCapabilitiesFetcherTest,
       TestFetchingAvailableAccountInfoCapabilities) {
  signin::Tribool expected_capability = ExpectedCapabilityValue();

  AccountInfo account = SignInPrimaryAccount();
  SetAccountInfoCanSignInToChromeCapability(account, expected_capability);

  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"", "full_name", "given_name", "locale",
      /*picture_url=*/"");

  base::RunLoop run_loop;
  CapabilityFetchCompletionCallback callback = base::BindLambdaForTesting(
      [&run_loop, expected_capability](signin::Tribool capability) {
        EXPECT_EQ(capability, expected_capability);
        run_loop.Quit();
      });

  fetcher_ = BuildAgeMismatchCapabilitiesFetcher();
  EXPECT_EQ([fetcher_ canSignInToChromeCapabilityForAccount:account.account_id],
            expected_capability);

  [fetcher_
      startFetchingCanSignInToChromeCapabilityWithCallback:std::move(callback)
                                                forAccount:account.account_id];
  run_loop.Run();
}

TEST_P(AgeMismatchCapabilitiesFetcherTest,
       TestAccountInfoReceivedWithCapability) {
  signin::Tribool expected_capability = ExpectedCapabilityValue();
  fetcher_ = BuildAgeMismatchCapabilitiesFetcher();

  AccountInfo account = SignInPrimaryAccount();
  EXPECT_EQ([fetcher_ canSignInToChromeCapabilityForAccount:account.account_id],
            signin::Tribool::kUnknown);

  base::RunLoop run_loop;
  CapabilityFetchCompletionCallback callback = base::BindLambdaForTesting(
      [&run_loop, expected_capability](signin::Tribool capability) {
        EXPECT_EQ(capability, expected_capability);
        run_loop.Quit();
      });

  [fetcher_
      startFetchingCanSignInToChromeCapabilityWithCallback:std::move(callback)
                                                forAccount:account.account_id];

  SetAccountInfoCanSignInToChromeCapability(account, expected_capability);
  EXPECT_EQ([fetcher_ canSignInToChromeCapabilityForAccount:account.account_id],
            expected_capability);

  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account.account_id, account.email, account.gaia,
      /*hosted_domain=*/"", "full_name", "given_name", "locale",
      /*picture_url=*/"");

  run_loop.Run();
}

TEST_P(AgeMismatchCapabilitiesFetcherTest, TestCapabilityFetchDeadline) {
  base::ScopedMockClockOverride scoped_clock;
  base::RunLoop run_loop;

  CapabilityFetchCompletionCallback callback =
      base::BindLambdaForTesting([&run_loop](signin::Tribool capability) {
        EXPECT_EQ(capability, signin::Tribool::kUnknown);
        run_loop.Quit();
      });

  fetcher_ = BuildAgeMismatchCapabilitiesFetcher();

  AccountInfo account = SignInPrimaryAccount();
  [fetcher_
      startFetchingCanSignInToChromeCapabilityWithCallback:std::move(callback)
                                                forAccount:account.account_id];
  scoped_clock.Advance(base::Seconds(1));
  run_loop.Run();
}

TEST_P(AgeMismatchCapabilitiesFetcherTest, TestConcurrentFetches) {
  signin::Tribool expected_capability = ExpectedCapabilityValue();
  fetcher_ = BuildAgeMismatchCapabilitiesFetcher();

  AccountInfo account1 =
      identity_test_env_.MakeAccountAvailable("test1@gmail.com");
  AccountInfo account2 =
      identity_test_env_.MakeAccountAvailable("test2@gmail.com");

  EXPECT_EQ(
      [fetcher_ canSignInToChromeCapabilityForAccount:account1.account_id],
      signin::Tribool::kUnknown);
  EXPECT_EQ(
      [fetcher_ canSignInToChromeCapabilityForAccount:account2.account_id],
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
                                                forAccount:account1.account_id];
  [fetcher_
      startFetchingCanSignInToChromeCapabilityWithCallback:std::move(callback2)
                                                forAccount:account2.account_id];

  SetAccountInfoCanSignInToChromeCapability(account1, expected_capability);
  SetAccountInfoCanSignInToChromeCapability(account2, expected_capability);

  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account1.account_id, account1.email, account1.gaia,
      /*hosted_domain=*/"", "full_name", "given_name", "locale",
      /*picture_url=*/"");

  identity_test_env_.SimulateSuccessfulFetchOfAccountInfo(
      account2.account_id, account2.email, account2.gaia,
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
