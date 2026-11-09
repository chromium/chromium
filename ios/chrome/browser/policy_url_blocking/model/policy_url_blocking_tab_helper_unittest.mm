// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy_url_blocking/model/policy_url_blocking_tab_helper.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <utility>

#import "base/memory/raw_ptr.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "components/policy/core/browser/url_list/url_blocklist_policy_handler.h"
#import "components/policy/core/browser/url_list/url_list_policy_pref_names.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/safe_search_api/fake_url_checker_client.h"
#import "components/safe_search_api/url_checker.h"
#import "components/safe_search_api/url_checker_client.h"
#import "ios/chrome/browser/policy_url_blocking/model/safe_search_service.h"
#import "ios/chrome/browser/policy_url_blocking/model/safe_search_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/net/protocol_handler_util.h"
#import "ios/web/public/navigation/web_state_policy_decider.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/net_errors.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/page_transition_types.h"
#import "url/gurl.h"

namespace {

class PolicyUrlBlockingTabHelperTest : public PlatformTest {
 public:
  PolicyUrlBlockingTabHelperTest() {
    profile_ = TestProfileIOS::Builder().Build();

    SafeSearchService* safe_search_service =
        SafeSearchServiceFactory::GetForProfile(profile_.get());
    auto fake_client =
        std::make_unique<safe_search_api::FakeURLCheckerClient>();
    fake_client_ = fake_client.get();
    safe_search_service->SetSafeSearchURLCheckerForTest(
        std::make_unique<safe_search_api::URLChecker>(std::move(fake_client),
                                                      /*cache_size=*/0));

    web_state_.SetBrowserState(profile_.get());
    PolicyUrlBlockingTabHelper::CreateForWebState(&web_state_);
  }

  void TearDown() override {
    fake_client_ = nullptr;
    PlatformTest::TearDown();
  }

 protected:
  void SetSafeSitesFilterBehavior(policy::SafeSitesFilterBehavior behavior) {
    profile_->GetPrefs()->SetInteger(
        policy::policy_prefs::kSafeSitesFilterBehavior,
        static_cast<int>(behavior));
  }

  web::WebStatePolicyDecider::RequestInfo DefaultRequestInfo() {
    return web::WebStatePolicyDecider::RequestInfo(
        ui::PageTransition::PAGE_TRANSITION_LINK,
        /*target_frame_is_main=*/true,
        /*target_frame_is_cross_origin=*/false,
        /*target_window_is_cross_origin=*/false,
        /*is_user_initiated=*/false,
        /*user_tapped_recently=*/false);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<safe_search_api::FakeURLCheckerClient> fake_client_;
  web::FakeWebState web_state_;
};

TEST_F(PolicyUrlBlockingTabHelperTest, UrlBlocklistTakesPrecedence) {
  base::ListValue blocklist;
  blocklist.Append("example.com");
  profile_->GetPrefs()->SetList(policy::policy_prefs::kUrlBlocklist,
                                std::move(blocklist));
  SetSafeSitesFilterBehavior(
      policy::SafeSitesFilterBehavior::kSafeSitesFilterEnabled);

  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision> future;
  web_state_.ShouldAllowRequest(
      [NSURLRequest
          requestWithURL:net::NSURLWithGURL(GURL("https://example.com"))],
      DefaultRequestInfo(), future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  web::WebStatePolicyDecider::PolicyDecision decision = future.Take();
  EXPECT_TRUE(decision.ShouldCancelNavigation());
  EXPECT_TRUE(decision.ShouldDisplayError());
  NSError* error = decision.GetDisplayError();
  EXPECT_NSEQ(net::kNSErrorDomain, error.domain);
  EXPECT_EQ(net::ERR_BLOCKED_BY_ADMINISTRATOR, error.code);
}

TEST_F(PolicyUrlBlockingTabHelperTest,
       UrlAllowlistTakesPrecedenceOverSafeSites) {
  base::ListValue allowlist;
  allowlist.Append("example.com");
  profile_->GetPrefs()->SetList(policy::policy_prefs::kUrlAllowlist,
                                std::move(allowlist));
  SetSafeSitesFilterBehavior(
      policy::SafeSitesFilterBehavior::kSafeSitesFilterEnabled);

  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision> future;
  web_state_.ShouldAllowRequest(
      [NSURLRequest
          requestWithURL:net::NSURLWithGURL(GURL("https://example.com"))],
      DefaultRequestInfo(), future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  web::WebStatePolicyDecider::PolicyDecision decision = future.Take();
  EXPECT_TRUE(decision.ShouldAllowNavigation());
  EXPECT_FALSE(decision.ShouldDisplayError());
}

TEST_F(PolicyUrlBlockingTabHelperTest, SafeSitesFilterDisabledByDefault) {
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision> future;
  web_state_.ShouldAllowRequest(
      [NSURLRequest
          requestWithURL:net::NSURLWithGURL(GURL("https://example.com"))],
      DefaultRequestInfo(), future.GetCallback());

  EXPECT_TRUE(future.IsReady());
  web::WebStatePolicyDecider::PolicyDecision decision = future.Take();
  EXPECT_TRUE(decision.ShouldAllowNavigation());
  EXPECT_FALSE(decision.ShouldDisplayError());
}

TEST_F(PolicyUrlBlockingTabHelperTest, SafeSitesFilterEnabledSafeUrlAllowed) {
  SetSafeSitesFilterBehavior(
      policy::SafeSitesFilterBehavior::kSafeSitesFilterEnabled);

  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision> future;
  web_state_.ShouldAllowRequest(
      [NSURLRequest
          requestWithURL:net::NSURLWithGURL(GURL("https://example.com"))],
      DefaultRequestInfo(), future.GetCallback());

  EXPECT_FALSE(future.IsReady());
  fake_client_->RunCallback(safe_search_api::ClientClassification::kAllowed);

  EXPECT_TRUE(future.IsReady());
  web::WebStatePolicyDecider::PolicyDecision decision = future.Take();
  EXPECT_TRUE(decision.ShouldAllowNavigation());
  EXPECT_FALSE(decision.ShouldDisplayError());
}

TEST_F(PolicyUrlBlockingTabHelperTest, SafeSitesFilterEnabledUnsafeUrlBlocked) {
  SetSafeSitesFilterBehavior(
      policy::SafeSitesFilterBehavior::kSafeSitesFilterEnabled);

  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision> future;
  web_state_.ShouldAllowRequest(
      [NSURLRequest
          requestWithURL:net::NSURLWithGURL(GURL("https://example.com"))],
      DefaultRequestInfo(), future.GetCallback());

  EXPECT_FALSE(future.IsReady());
  fake_client_->RunCallback(safe_search_api::ClientClassification::kRestricted);

  EXPECT_TRUE(future.IsReady());
  web::WebStatePolicyDecider::PolicyDecision decision = future.Take();
  EXPECT_TRUE(decision.ShouldCancelNavigation());
  EXPECT_TRUE(decision.ShouldDisplayError());
  NSError* error = decision.GetDisplayError();
  EXPECT_NSEQ(net::kNSErrorDomain, error.domain);
  EXPECT_EQ(net::ERR_BLOCKED_BY_ADMINISTRATOR, error.code);
}

TEST_F(PolicyUrlBlockingTabHelperTest, NonHttpOrHttpsUrlsBypassed) {
  SetSafeSitesFilterBehavior(
      policy::SafeSitesFilterBehavior::kSafeSitesFilterEnabled);

  static constexpr const char* const kUrls[] = {
      "blob:https://example.com/00000000-0000-0000-0000-000000000000",
      "about:blank",
      "chrome://version",
      "data:text/html,Hello",
  };

  for (const char* url_str : kUrls) {
    base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision> future;
    web_state_.ShouldAllowRequest(
        [NSURLRequest requestWithURL:net::NSURLWithGURL(GURL(url_str))],
        DefaultRequestInfo(), future.GetCallback());

    EXPECT_TRUE(future.IsReady());
    web::WebStatePolicyDecider::PolicyDecision decision = future.Take();
    EXPECT_TRUE(decision.ShouldAllowNavigation());
    EXPECT_FALSE(decision.ShouldDisplayError());
  }
}

TEST_F(PolicyUrlBlockingTabHelperTest, AsyncClassificationHandledCorrectly) {
  SetSafeSitesFilterBehavior(
      policy::SafeSitesFilterBehavior::kSafeSitesFilterEnabled);

  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision> future;
  web_state_.ShouldAllowRequest(
      [NSURLRequest
          requestWithURL:net::NSURLWithGURL(GURL("https://example.com"))],
      DefaultRequestInfo(), future.GetCallback());

  EXPECT_FALSE(future.IsReady());
  fake_client_->RunCallbackAsync(
      safe_search_api::ClientClassification::kAllowed);

  web::WebStatePolicyDecider::PolicyDecision decision = future.Take();
  EXPECT_TRUE(decision.ShouldAllowNavigation());
  EXPECT_FALSE(decision.ShouldDisplayError());
}

TEST_F(PolicyUrlBlockingTabHelperTest, WebStateDestroyedWhileCheckPending) {
  SetSafeSitesFilterBehavior(
      policy::SafeSitesFilterBehavior::kSafeSitesFilterEnabled);

  auto temp_web_state = std::make_unique<web::FakeWebState>();
  temp_web_state->SetBrowserState(profile_.get());
  PolicyUrlBlockingTabHelper::CreateForWebState(temp_web_state.get());

  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision> future;
  temp_web_state->ShouldAllowRequest(
      [NSURLRequest
          requestWithURL:net::NSURLWithGURL(GURL("https://example.com"))],
      DefaultRequestInfo(), future.GetCallback());

  EXPECT_FALSE(future.IsReady());
  temp_web_state.reset();

  fake_client_->RunCallback(safe_search_api::ClientClassification::kAllowed);

  EXPECT_TRUE(future.IsReady());
  web::WebStatePolicyDecider::PolicyDecision decision = future.Take();
  EXPECT_TRUE(decision.ShouldCancelNavigation());
}

TEST_F(PolicyUrlBlockingTabHelperTest,
       SafeSearchCheckerDestroyedWhileCheckPending) {
  SetSafeSitesFilterBehavior(
      policy::SafeSitesFilterBehavior::kSafeSitesFilterEnabled);

  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision> future;
  web_state_.ShouldAllowRequest(
      [NSURLRequest
          requestWithURL:net::NSURLWithGURL(GURL("https://example.com"))],
      DefaultRequestInfo(), future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  // Destroy the URL checker while the check is still pending. The pending
  // callback must run immediately and fail closed.
  fake_client_ = nullptr;
  SafeSearchService* safe_search_service =
      SafeSearchServiceFactory::GetForProfile(profile_.get());
  safe_search_service->SetSafeSearchURLCheckerForTest(nullptr);

  EXPECT_TRUE(future.IsReady());
  web::WebStatePolicyDecider::PolicyDecision decision = future.Take();
  EXPECT_TRUE(decision.ShouldCancelNavigation());
  EXPECT_FALSE(decision.ShouldDisplayError());
}

}  // namespace
