// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/test/bind.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_service.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_error.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_query_manager.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/apple/url_conversions.h"
#import "services/network/public/mojom/fetch_api.mojom.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
enum class SafeBrowsingDecisionTimingWithAsync {
  kBeforeResponseAsyncDisabled,
  kBeforeResponseAsyncEnabled,
  kAfterResponseAsyncDisabled,
  kAfterResponseAsyncEnabled
};
}

class SafeBrowsingTabHelperTest
    : public testing::TestWithParam<
          std::tuple<SafeBrowsingDecisionTimingWithAsync, bool>> {
 protected:
  SafeBrowsingTabHelperTest()
      : browser_state_(std::make_unique<web::FakeBrowserState>()),
        client_(/*pref_service=*/nullptr) {
    feature_list_.InitWithFeatureState(safe_browsing::kLocalListsUseSBv5,
                                       UseV5());
    SafeBrowsingQueryManager::CreateForWebState(&web_state_, &client_);
    SafeBrowsingTabHelper::CreateForWebState(&web_state_, &client_);
    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);
    SafeBrowsingUnsafeResourceContainer::CreateForWebState(&web_state_);
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    web_state_.SetBrowserState(browser_state_.get());
  }

  SafeBrowsingDecisionTimingWithAsync GetTiming() const {
    return std::get<0>(GetParam());
  }

  bool UseV5() const { return std::get<1>(GetParam()); }

  // Whether Safe Browsing decisions arrive before calls to
  // ShouldAllowResponseUrl().
  bool SafeBrowsingDecisionArrivesBeforeResponse() const {
    return GetTiming() == SafeBrowsingDecisionTimingWithAsync::
                              kBeforeResponseAsyncDisabled ||
           GetTiming() ==
               SafeBrowsingDecisionTimingWithAsync::kBeforeResponseAsyncEnabled;
  }

  // Helper function that calls into WebState::ShouldAllowRequest with the
  // given `url` and `for_main_frame`.
  web::WebStatePolicyDecider::PolicyDecision ShouldAllowRequestUrl(
      const GURL& url,
      bool for_main_frame = true,
      ui::PageTransition transition =
          ui::PageTransition::PAGE_TRANSITION_FIRST) {
    const web::WebStatePolicyDecider::RequestInfo request_info(
        transition, for_main_frame, /*target_frame_is_cross_origin=*/false,
        /*target_window_is_cross_origin=*/false,
        /*is_user_initiated=*/false, /*user_tapped_recently=*/false);
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback =
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policy_decision = decision;
          callback_called = true;
        });
    web_state_.ShouldAllowRequest(
        [NSURLRequest requestWithURL:net::NSURLWithGURL(url)], request_info,
        std::move(callback));
    EXPECT_TRUE(callback_called);
    return policy_decision;
  }

  // Helper function that calls into WebState::ShouldAllowResponse with the
  // given `url` and `for_main_frame`, waits for the callback with the decision
  // to be called, and returns the decision.
  web::WebStatePolicyDecider::PolicyDecision ShouldAllowResponseUrl(
      const GURL& url,
      bool for_main_frame = true) {
    NSURLResponse* response =
        [[NSURLResponse alloc] initWithURL:net::NSURLWithGURL(url)
                                  MIMEType:@"text/html"
                     expectedContentLength:0
                          textEncodingName:nil];
    __block bool callback_called = false;
    __block web::WebStatePolicyDecider::PolicyDecision policy_decision =
        web::WebStatePolicyDecider::PolicyDecision::Allow();
    auto callback =
        base::BindOnce(^(web::WebStatePolicyDecider::PolicyDecision decision) {
          policy_decision = decision;
          callback_called = true;
        });
    web::WebStatePolicyDecider::ResponseInfo response_info(for_main_frame);
    web_state_.ShouldAllowResponse(response, response_info,
                                   std::move(callback));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(callback_called);
    return policy_decision;
  }

  // Helper function that simulates a main frame load to a safe URL, returning
  // the NavigationItem that was committed.  Used for sub frame tests.
  web::NavigationItem* SimulateSafeMainFrameLoad() {
    GURL safe_url("http://chromium.test");
    navigation_manager_->AddItem(safe_url, ui::PAGE_TRANSITION_LINK);
    web::NavigationItem* item = navigation_manager_->GetItemAtIndex(
        navigation_manager_->GetItemCount() - 1);
    navigation_manager_->SetLastCommittedItem(item);

    web::FakeNavigationContext context;
    context.SetHasCommitted(true);
    web_state_.OnNavigationFinished(&context);

    return item;
  }

  // Helper function that simulates a main frame redirect.
  void SimulateMainFrameRedirect() {
    web::FakeNavigationContext context;
    web_state_.OnNavigationRedirected(&context);
  }

  // Helper function to run all async callbacks first then sync callbacks.
  void RunAsyncCallbacksThenSyncCallbacks() {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^() {
          client_.run_async_callbacks();
        }));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^() {
          client_.run_sync_callbacks();
        }));

    // TODO(crbug.com/359420122): Remove when clean up is complete.
    if (SafeBrowsingDecisionArrivesBeforeResponse()) {
      base::RunLoop().RunUntilIdle();
    }
  }

  // Helper function to run all sync callbacks first then async callbacks.
  void RunSyncCallbacksThenAsyncCallbacks() {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^() {
          client_.run_sync_callbacks();
        }));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(^() {
          client_.run_async_callbacks();
        }));

    // TODO(crbug.com/359420122): Remove when clean up is complete.
    if (SafeBrowsingDecisionArrivesBeforeResponse()) {
      base::RunLoop().RunUntilIdle();
    }
  }

  // Waits until the unsafe verdict for `url` has been processed and recorded
  // as a pending decision in `SafeBrowsingUrlAllowList`. Unsafe verdicts reach
  // `SafeBrowsingTabHelper` through a posted task that displays the blocking
  // page, so they are not processed when `run_async_callbacks()` returns.
  [[nodiscard]] bool WaitForPendingUnsafeNavigationDecision(const GURL& url) {
    return base::test::RunUntil([&] {
      SafeBrowsingUrlAllowList* allow_list =
          SafeBrowsingUrlAllowList::FromWebState(&web_state_);
      return allow_list && allow_list->IsUnsafeNavigationDecisionPending(url);
    });
  }

  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<web::FakeBrowserState> browser_state_;
  web::FakeWebState web_state_;
  raw_ptr<web::FakeNavigationManager> navigation_manager_ = nullptr;
  FakeSafeBrowsingClient client_;
};

// Tests the case of a single navigation request and response, for a URL that is
// safe.
TEST_P(SafeBrowsingTabHelperTest, SingleSafeRequestAndResponse) {
  GURL url("http://chromium.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of a single navigation request and response, for a URL that is
// safe where the async query completes before the sync query.
TEST_P(SafeBrowsingTabHelperTest, SingleSafeRequestAndResponseAsyncQueryFirst) {
  GURL url("http://chromium.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  RunAsyncCallbacksThenSyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of a single navigation request and response, for a URL that is
// unsafe.
TEST_P(SafeBrowsingTabHelperTest, SingleUnsafeRequestAndResponse) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());

  NSError* decision_error = response_decision.GetDisplayError();
  EXPECT_NSEQ(decision_error.domain, kSafeBrowsingErrorDomain);
  EXPECT_EQ(static_cast<SafeBrowsingErrorCode>(decision_error.code),
            SafeBrowsingErrorCode::kUnsafeResource);
}

// Tests the case of a single navigation request and response, for a URL that is
// blocked by an Enterprise organization.
TEST_P(SafeBrowsingTabHelperTest, SingleEnterpriseBlockRequestAndResponse) {
  GURL url("http://" + FakeSafeBrowsingService::kEnterpriseBlockHost);
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);

  EXPECT_TRUE(response_decision.ShouldCancelNavigation());

  NSError* decision_error = response_decision.GetDisplayError();
  EXPECT_NSEQ(decision_error.domain, kSafeBrowsingErrorDomain);
  EXPECT_EQ(static_cast<SafeBrowsingErrorCode>(decision_error.code),
            SafeBrowsingErrorCode::kEnterpriseBlock);
}

// Tests the case of a single navigation request and response, for a URL that
// requires displaying a warning from an Enterprise organization.
TEST_P(SafeBrowsingTabHelperTest, SingleEnterpriseWarnRequestAndResponse) {
  GURL url("http://" + FakeSafeBrowsingService::kEnterpriseWarnHost);
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);

  EXPECT_TRUE(response_decision.ShouldCancelNavigation());

  NSError* decision_error = response_decision.GetDisplayError();
  EXPECT_NSEQ(decision_error.domain, kSafeBrowsingErrorDomain);
  EXPECT_EQ(static_cast<SafeBrowsingErrorCode>(decision_error.code),
            SafeBrowsingErrorCode::kEnterpriseWarn);
}

// Tests the case of a single navigation request and response, for a URL that is
// unsafe where the async query is completed before the sync query.
TEST_P(SafeBrowsingTabHelperTest,
       SingleUnsafeRequestAndResponseAsyncQueryFirst) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  RunAsyncCallbacksThenSyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
}

// Tests the case of a single safe navigation where the response URL has a
// different ref than the request URL.
TEST_P(SafeBrowsingTabHelperTest, SafeRequestAndResponseWithDifferingRef) {
  GURL request_url("http://chromium.test");
  GURL response_url("http://chromium.test#ref");
  EXPECT_TRUE(ShouldAllowRequestUrl(request_url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(response_url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of a single unsafe navigation where the response URL has a
// different ref than the request URL.
TEST_P(SafeBrowsingTabHelperTest, UnsafeRequestAndResponseWithDifferingRef) {
  GURL request_url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  GURL response_url("http://" + FakeSafeBrowsingService::kUnsafeHost + "#ref");
  EXPECT_TRUE(ShouldAllowRequestUrl(request_url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(response_url);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
}

// Tests the case of a single navigation request followed by multiple responses
// for the same URL.
TEST_P(SafeBrowsingTabHelperTest, RepeatedResponse) {
  GURL url("http://chromium.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
  response_decision = ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
  response_decision = ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of multiple requests, followed by a single response skipping
// some of the request URLs.
TEST_P(SafeBrowsingTabHelperTest, MultipleRequestsSingleResponse) {
  GURL url1("http://chromium.test");
  GURL url2("http://chromium3.test");
  GURL url3("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
}

// Tests the case of repeated requests for the same unsafe URL, ensuring that
// request decisions are not re-used and distinct errors are returned.
TEST_P(SafeBrowsingTabHelperTest, RepeatedRequestsGetDistinctError) {
  // Compare the NSError objects.
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldDisplayError());

  auto request_decision2 = ShouldAllowRequestUrl(url);
  EXPECT_TRUE(request_decision2.ShouldDisplayError());
  EXPECT_NE(response_decision.GetDisplayError(),
            request_decision2.GetDisplayError());
}

// Tests the case of a request and response with URLs that have an unsupported
// scheme.
TEST_P(SafeBrowsingTabHelperTest, RequestAndResponseWithUnsupportedScheme) {
  GURL request_url("blob:http://chromium.test/123");
  EXPECT_TRUE(ShouldAllowRequestUrl(request_url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  GURL response_url("blob:http://chromium.test/456");
  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(response_url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of a request and response that are not identical, but have
// the same host.
TEST_P(SafeBrowsingTabHelperTest, RequestAndResponseWithOnlyMatchingHost) {
  GURL request_url("http://chromium.test/page1.html");
  GURL response_url("http://chromium.test/page2.html");

  EXPECT_TRUE(ShouldAllowRequestUrl(request_url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  // We cannot use `ShouldAllowResponseUrl` directly because the verification
  // check now begins during ShouldAllowResponse, requiring manual invocation of
  // test callbacks to fulfill it.
  NSURLResponse* response =
      [[NSURLResponse alloc] initWithURL:net::NSURLWithGURL(response_url)
                                MIMEType:@"text/html"
                   expectedContentLength:0
                        textEncodingName:nil];
  bool callback_called = false;
  web::WebStatePolicyDecider::PolicyDecision policy_decision =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  auto callback = base::BindLambdaForTesting(
      [&](web::WebStatePolicyDecider::PolicyDecision decision) {
        policy_decision = decision;
        callback_called = true;
      });
  web::WebStatePolicyDecider::ResponseInfo response_info(
      /*for_main_frame=*/true);
  web_state_.ShouldAllowResponse(response, response_info, std::move(callback));

  // Fulfill the queued on-demand Safe Browsing check for response_url.
  client_.run_sync_callbacks();
  client_.run_async_callbacks();
  ASSERT_TRUE(base::test::RunUntil([&] { return callback_called; }));

  EXPECT_TRUE(policy_decision.ShouldAllowNavigation());
}

// Tests the case of a single sub frame navigation request and response, for a
// URL that is unsafe.
TEST_P(SafeBrowsingTabHelperTest, UnsafeSubFrameRequestAndResponse) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  SimulateSafeMainFrameLoad();

  // Execute ShouldAllowRequest() for an unsafe subframe navigation.
  auto sub_frame_request_decision =
      ShouldAllowRequestUrl(url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_request_decision.ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  // Verify that the sub frame navigation is allowed.
  web::WebStatePolicyDecider::PolicyDecision sub_frame_response_decision =
      ShouldAllowResponseUrl(url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_response_decision.ShouldAllowNavigation());
}

// Tests the case of a main frame reload request that arrives when both the last
// committed item and pending items are null.
TEST_P(SafeBrowsingTabHelperTest, MainFrameReload) {
  GURL url("http://chromium.test");
  ASSERT_FALSE(navigation_manager_->GetLastCommittedItem());
  ASSERT_FALSE(navigation_manager_->GetPendingItem());

  auto request_decision = ShouldAllowRequestUrl(
      url, /*for_main_frame=*/true, ui::PageTransition::PAGE_TRANSITION_RELOAD);
  EXPECT_TRUE(request_decision.ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of a redirection chain, where all URLs in the chain are safe.
TEST_P(SafeBrowsingTabHelperTest, SafeRedirectChain) {
  GURL url1("http://chromium1.test");
  GURL url2("http://chromium2.test");
  GURL url3("http://chromium3.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());

  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&context);

  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(tab_helper->GetRedirectChain(),
            std::vector<GURL>({url1, url2, url3}));
}

// Tests that GetRedirectChain returns the expected URL for a navigation without
// redirects.
TEST_P(SafeBrowsingTabHelperTest, NonRedirectChain) {
  GURL url("http://chromium.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());

  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&context);

  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url}));
}

// Tests the case of a redirection chain, where the first URL in the chain is
// unsafe and the rest are safe.
TEST_P(SafeBrowsingTabHelperTest, RedirectChainFirstRequestUnsafe) {
  GURL url1("http://" + FakeSafeBrowsingService::kUnsafeHost);
  GURL url2("http://chromium2.test");
  GURL url3("http://chromium3.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
}

// Tests the case of a redirection chain with async checks, where the first URL
// in the chain is found as unsafe from sync queries and the rest are safe.
TEST_P(SafeBrowsingTabHelperTest,
       RedirectChainFirstRequestUnsafeWithAsyncChecks) {
  GURL url1("http://" + FakeSafeBrowsingService::kUnsafeHost);
  GURL url2("http://chromium2.test");
  GURL url3("http://chromium3.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
}

// Tests the case of a redirection chain, where the middle URL in the chain is
// unsafe and the rest are safe.
TEST_P(SafeBrowsingTabHelperTest, RedirectChainMiddleRequestUnsafe) {
  GURL url1("http://chromium1.test");
  GURL url2("http://" + FakeSafeBrowsingService::kUnsafeHost);
  GURL url3("http://chromium3.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
}

// Tests the case of a redirection chain with async checks, where the middle URL
// in the chain is found as unsafe from sync queries and the rest are safe.
TEST_P(SafeBrowsingTabHelperTest,
       RedirectChainMiddleRequestUnsafeWithAsyncCheck) {
  GURL url1("http://chromium1.test");
  GURL url2("http://" + FakeSafeBrowsingService::kUnsafeHost);
  GURL url3("http://chromium3.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  SimulateMainFrameRedirect();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
}

// Tests the case of a redirection chain, where the final URL in the chain is
// unsafe and the rest are safe.
TEST_P(SafeBrowsingTabHelperTest, RedirectChainFinalRequestUnsafe) {
  GURL url1("http://chromium1.test");
  GURL url2("http://chromium3.test");
  GURL url3("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
}

// Tests the case of a redirection chain with async check, where the final URL
// in the chain is found as unsafe from sync queries and the rest are safe.
TEST_P(SafeBrowsingTabHelperTest,
       RedirectChainFinalRequestUnsafeWithAsyncCheck) {
  GURL url1("http://chromium1.test");
  GURL url2("http://chromium3.test");
  GURL url3("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
}

// Tests the case of a redirection chain, where the all URLs in the chain are
// unsafe.
TEST_P(SafeBrowsingTabHelperTest, RedirectChainAllRequestsUnsafe) {
  GURL url1("http://" + FakeSafeBrowsingService::kUnsafeHost + "/1");
  GURL url2("http://" + FakeSafeBrowsingService::kUnsafeHost + "/2");
  GURL url3("http://" + FakeSafeBrowsingService::kUnsafeHost + "/3");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
}

// Tests the case of a redirection chain with async check, where the all URLs in
// the chain are found as unsafe from sync queries.
TEST_P(SafeBrowsingTabHelperTest,
       RedirectChainAllRequestsUnsafeWithAsyncCheck) {
  GURL url1("http://" + FakeSafeBrowsingService::kUnsafeHost + "/1");
  GURL url2("http://" + FakeSafeBrowsingService::kUnsafeHost + "/2");
  GURL url3("http://" + FakeSafeBrowsingService::kUnsafeHost + "/3");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
}

// Tests that when there are consecutive requests without a redirect, the
// decision for the final request is unaffected by the decision for an earlier
// request.
TEST_P(SafeBrowsingTabHelperTest, ConsecutiveRequestsWithoutRedirect) {
  GURL url1("http://" + FakeSafeBrowsingService::kUnsafeHost + "/1");
  GURL url2("http://" + FakeSafeBrowsingService::kUnsafeHost + "/2");
  GURL url3("http://chromium.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  // Since there was no redirect, `url3` should be treated as safe.
  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of a redirection chain that is interuppted by an unrelated
// request.
TEST_P(SafeBrowsingTabHelperTest, InterruptedUnsafeRedirectChain) {
  GURL url1("http://" + FakeSafeBrowsingService::kUnsafeHost + "/1");
  GURL url2("http://" + FakeSafeBrowsingService::kUnsafeHost + "/2");
  GURL url3("http://chromium3.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  // Interrupt the redirection chain with a brand new unrelated query. This
  // should be treated as safe.
  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of a redirection chain with sync and async checks that are
// interrupted by an unrelated request.
TEST_P(SafeBrowsingTabHelperTest,
       InterruptedUnsafeRedirectChainWithAsyncCheck) {
  GURL url1("http://" + FakeSafeBrowsingService::kUnsafeHost + "/1");
  GURL url2("http://" + FakeSafeBrowsingService::kUnsafeHost + "/2");
  GURL url3("http://chromium3.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  // Interrupt the redirection chain with a brand new unrelated query. This
  // should be treated as safe.
  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of a redirection chain where a safe URL redirects to itself.
TEST_P(SafeBrowsingTabHelperTest, RedirectToSameSafeURL) {
  GURL url("http://chromium.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  // Simulate the URL redirecting to itself multiple times before producing a
  // response.
  SimulateMainFrameRedirect();
  SimulateMainFrameRedirect();
  SimulateMainFrameRedirect();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of a redirection chain where an unsafe URL redirects to
// itself.
TEST_P(SafeBrowsingTabHelperTest, RedirectToSameUnsafeURL) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  // Simulate the URL redirecting to itself multiple times before producing a
  // response.
  SimulateMainFrameRedirect();
  SimulateMainFrameRedirect();
  SimulateMainFrameRedirect();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
}

// Tests the case of a redirection chain where all URLs in the chain are safe,
// and one URL appears multiple times.
TEST_P(SafeBrowsingTabHelperTest, SafeRedirectChainWithRepeatedURL) {
  GURL url1("http://chromium1.test");
  GURL url2("http://chromium2.test");
  GURL url3("http://chromium3.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url1);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of a redirection chain where an unsafe URL appears multiple
// times.
TEST_P(SafeBrowsingTabHelperTest, UnsafeRedirectChainWithRepeatedURL) {
  GURL url1("http://chromium1.test");
  GURL url2("http://" + FakeSafeBrowsingService::kUnsafeHost);
  GURL url3("http://chromium3.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  base::RunLoop().RunUntilIdle();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  // Because the unsafe decision for `url2` was recorded even when stale, the
  // repeated request to `url2` is cancelled immediately at request time.
  auto request_decision = ShouldAllowRequestUrl(url2);
  EXPECT_TRUE(request_decision.ShouldCancelNavigation());
  EXPECT_TRUE(request_decision.ShouldDisplayError());
}

// Tests the case of a redirection where ShouldAllowRequest is not called on
// the target of the redirection but instead called a second time on the source.
TEST_P(SafeBrowsingTabHelperTest, RedirectWithMissingShouldAllowRequest) {
  GURL url1("http://chromium1.test/page1.html");
  GURL url2("http://chromium2.test/page2.html");
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  SimulateMainFrameRedirect();

  // We cannot use `ShouldAllowResponseUrl` directly because the verification
  // check now begins during ShouldAllowResponse, requiring manual invocation of
  // test callbacks to fulfill it.
  NSURLResponse* response =
      [[NSURLResponse alloc] initWithURL:net::NSURLWithGURL(url2)
                                MIMEType:@"text/html"
                   expectedContentLength:0
                        textEncodingName:nil];
  bool callback_called = false;
  web::WebStatePolicyDecider::PolicyDecision policy_decision =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  auto callback = base::BindLambdaForTesting(
      [&](web::WebStatePolicyDecider::PolicyDecision decision) {
        policy_decision = decision;
        callback_called = true;
      });
  web::WebStatePolicyDecider::ResponseInfo response_info(
      /*for_main_frame=*/true);
  web_state_.ShouldAllowResponse(response, response_info, std::move(callback));

  // Fulfill the queued on-demand Safe Browsing check for url2.
  client_.run_sync_callbacks();
  client_.run_async_callbacks();
  ASSERT_TRUE(base::test::RunUntil([&] { return callback_called; }));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(policy_decision.ShouldAllowNavigation());
}

// Tests the case of a redirection where ShouldAllowRequest is skipped for an
// unsafe target URL, ensuring that Safe Browsing identifies the mismatch and
// blocks it.
TEST_P(SafeBrowsingTabHelperTest, RedirectWithMissingShouldAllowRequestUnsafe) {
  GURL url1("http://chromium1.test/page1.html");
  GURL url2("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  // Simulate quirk: ShouldAllowRequest is called on source again instead of
  // target.
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  SimulateMainFrameRedirect();

  // We cannot use `ShouldAllowResponseUrl` directly because it expects
  // completion. In this edge case, the check begins during ShouldAllowResponse.
  NSURLResponse* response =
      [[NSURLResponse alloc] initWithURL:net::NSURLWithGURL(url2)
                                MIMEType:@"text/html"
                   expectedContentLength:0
                        textEncodingName:nil];
  bool callback_called = false;
  web::WebStatePolicyDecider::PolicyDecision policy_decision =
      web::WebStatePolicyDecider::PolicyDecision::Allow();
  auto callback = base::BindLambdaForTesting(
      [&](web::WebStatePolicyDecider::PolicyDecision decision) {
        policy_decision = decision;
        callback_called = true;
      });
  web::WebStatePolicyDecider::ResponseInfo response_info(
      /*for_main_frame=*/true);
  web_state_.ShouldAllowResponse(response, response_info, std::move(callback));

  // Verify callback was NOT called yet, as the on-demand check was just queued.
  EXPECT_FALSE(callback_called);

  // Run the queued Safe Browsing checks.
  client_.run_sync_callbacks();
  client_.run_async_callbacks();
  ASSERT_TRUE(base::test::RunUntil([&] { return callback_called; }));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(policy_decision.ShouldCancelNavigation());
  EXPECT_TRUE(policy_decision.ShouldDisplayError());
}

// Tests that client is notified when URL loaded in the main frame is unsafe.
TEST_P(SafeBrowsingTabHelperTest, UnsafeMainFrameRequestNotifiesClient) {
  GURL unsafe_url("http://" + FakeSafeBrowsingService::kUnsafeHost);

  EXPECT_TRUE(ShouldAllowRequestUrl(unsafe_url).ShouldAllowNavigation());

  // When `unsafe_url` is determined to be unsafe, the client should be
  // notified.
  EXPECT_FALSE(client_.main_frame_cancellation_decided_called());
  RunSyncCallbacksThenAsyncCallbacks();
  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(client_.main_frame_cancellation_decided_called());
  } else {
    web::WebStatePolicyDecider::PolicyDecision response_decision =
        ShouldAllowResponseUrl(unsafe_url);
    EXPECT_TRUE(response_decision.ShouldCancelNavigation());
    EXPECT_TRUE(client_.main_frame_cancellation_decided_called());
  }
}

// Tests that client is not notified when the main frame URL is safe.
TEST_P(SafeBrowsingTabHelperTest, SafeMainFrameRequestDoesNotNotifyClient) {
  GURL safe_url("http://chromium.test");

  EXPECT_TRUE(ShouldAllowRequestUrl(safe_url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  EXPECT_FALSE(client_.main_frame_cancellation_decided_called());
  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    base::RunLoop().RunUntilIdle();
  }

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(safe_url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
  EXPECT_FALSE(client_.main_frame_cancellation_decided_called());
}

// Tests sync check and ShouldAllowResponse() complete, but async
// check returns after a page loads. Tests that the async check forcefully
// reloads the page, and the reload request is directly cancelled.
TEST_P(SafeBrowsingTabHelperTest,
       UnsafeCommittedRedirectChainReloadAndResponse) {
  GURL url("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost);
  ASSERT_FALSE(navigation_manager_->ReloadWasCalled());
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));

  // TODO(crbug.com/359420122): Remove when clean up is complete.
  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    base::RunLoop().RunUntilIdle();
  }

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());

  // Simulate page loading and navigation being finished.
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&context);

  client_.run_async_callbacks();
  // TODO(crbug.com/359420122): Remove when clean up is complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(navigation_manager_->ReloadWasCalled());

  // Simulate forced reload and triggers blocking page logic.
  auto main_frame_reload_request_decision = ShouldAllowRequestUrl(
      url, /*for_main_frame=*/true, ui::PageTransition::PAGE_TRANSITION_RELOAD);
  EXPECT_TRUE(main_frame_reload_request_decision.ShouldCancelNavigation());
  EXPECT_TRUE(main_frame_reload_request_decision.ShouldDisplayError());
}

// Tests sync check and ShouldAllowResponse() complete, and async
// check completes before a page committed. Tests that the async check allows
// the navigation to go through and reloads when the page commits, with the
// reload request being directly cancelled.
TEST_P(SafeBrowsingTabHelperTest,
       UnsafeToBeCommittedRedirectChainReloadAndResponse) {
  GURL url("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost);
  ASSERT_FALSE(navigation_manager_->ReloadWasCalled());
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));

  // TODO(crbug.com/359420122): Remove when clean up is complete.
  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    base::RunLoop().RunUntilIdle();
  }
  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());

  client_.run_async_callbacks();
  // TODO(crbug.com/359420122): Remove when clean up is complete.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(navigation_manager_->ReloadWasCalled());

  // Simulate page loading and navigation being finished.
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&context);
  EXPECT_TRUE(navigation_manager_->ReloadWasCalled());

  // Simulate reload and triggers blocking page logic.
  auto main_frame_reload_request_decision = ShouldAllowRequestUrl(
      url, /*for_main_frame=*/true, ui::PageTransition::PAGE_TRANSITION_RELOAD);
  EXPECT_TRUE(main_frame_reload_request_decision.ShouldCancelNavigation());
  EXPECT_TRUE(main_frame_reload_request_decision.ShouldDisplayError());
}

// This is a control case for
// `PostCommitAsyncVerdictReloadShowsInterstitialWithReplaceState` below.
//
// Tests that without a `history.replaceState()` rewrite, the forced reload
// issued when a late async verdict flags an already-committed page targets
// the flagged URL itself, and the reload request is immediately cancelled
// to show the Safe Browsing interstitial.
TEST_P(SafeBrowsingTabHelperTest,
       PostCommitAsyncVerdictReloadShowsInterstitialWithoutReplaceState) {
  GURL phishing_url("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost +
                    "/phishing.html");
  ASSERT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Navigation to the phishing URL starts; the sync (local database) check
  // finds nothing, so the response is allowed while the async
  // (hash-real-time / URL-real-time) check is still in flight.
  EXPECT_TRUE(ShouldAllowRequestUrl(phishing_url).ShouldAllowNavigation());
  // The sync check callback is queued by the request above; run it from a
  // posted task so that it completes asynchronously, as in production.
  ASSERT_FALSE(client_.sync_completion_callbacks_.empty());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));

  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return client_.sync_completion_callbacks_.empty(); }));
  }
  EXPECT_TRUE(ShouldAllowResponseUrl(phishing_url).ShouldAllowNavigation());

  // The phishing page commits with a last-committed NavigationItem.
  navigation_manager_->AddItem(phishing_url, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item);
  web::FakeNavigationContext commit_context;
  commit_context.SetUrl(phishing_url);
  commit_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&commit_context);

  // The async verdict for the phishing URL arrives: unsafe. This triggers
  // ReloadPage(), which issues a fresh navigation to the last-committed
  // item's URL — still the phishing URL.
  client_.run_async_callbacks();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return navigation_manager_->LoadURLWithParamsWasCalled(); }));

  auto last_params = navigation_manager_->GetLastLoadURLWithParams();
  ASSERT_TRUE(last_params.has_value());
  EXPECT_EQ(phishing_url, last_params->url);

  // The reload request matches the stored UnsafeResource exactly, so it is
  // cancelled and the error page (interstitial) is displayed.
  auto reload_decision =
      ShouldAllowRequestUrl(phishing_url, /*for_main_frame=*/true,
                            ui::PageTransition::PAGE_TRANSITION_RELOAD);
  EXPECT_TRUE(reload_decision.ShouldCancelNavigation());
  EXPECT_TRUE(reload_decision.ShouldDisplayError());
}

// Tests that `history.replaceState()` cannot bypass a late async Safe Browsing
// verdict:
// 1. The page commits while the async check is in flight (sync check clean).
// 2. The page runs history.replaceState({}, '', '/innocent.html'), rewriting
//    the last-committed NavigationItem's URL.
// 3. The async verdict arrives (unsafe) and ReloadPage() navigates to the
//    flagged URL rather than the attacker-chosen replaceState URL.
// 4. The reload request for the flagged URL is cancelled with an error page
//    (interstitial).
// 5. In the residual weakness scenario where the tab's back/forward history
//    already contains the replaceState target and the web-layer error /
//    back-forward repair machinery tries to reload the current WebKit
//    back-forward item (whose URL was updated by replaceState to innocent_url),
//    ShouldAllowRequest() also cancels and displays the error page because
//    an unsafe navigation decision is pending for the host.
TEST_P(SafeBrowsingTabHelperTest,
       PostCommitAsyncVerdictReloadShowsInterstitialWithReplaceState) {
  GURL phishing_url("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost +
                    "/phishing.html");
  GURL innocent_url("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost +
                    "/innocent.html");
  ASSERT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Navigation to the phishing URL starts; the sync (local database) check
  // finds nothing, so the response is allowed while the async
  // (hash-real-time / URL-real-time) check is still in flight.
  EXPECT_TRUE(ShouldAllowRequestUrl(phishing_url).ShouldAllowNavigation());
  // The sync check callback is queued by the request above; run it from a
  // posted task so that it completes asynchronously, as in production.
  ASSERT_FALSE(client_.sync_completion_callbacks_.empty());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));

  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return client_.sync_completion_callbacks_.empty(); }));
  }
  EXPECT_TRUE(ShouldAllowResponseUrl(phishing_url).ShouldAllowNavigation());

  // The phishing page commits with a last-committed NavigationItem.
  navigation_manager_->AddItem(phishing_url, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item);
  web::FakeNavigationContext commit_context;
  commit_context.SetUrl(phishing_url);
  commit_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&commit_context);

  // The committed page runs `history.replaceState({}, '', '/innocent.html')`.
  // `NavigationManagerImpl::UpdateCurrentItemForReplaceState()` rewrites the
  // last-committed NavigationItem's URL in place, and the resulting
  // same-document navigation is ignored by DidFinishNavigation(), so the
  // committed redirect chain (with its pending async query for the phishing
  // URL) is preserved.
  item->SetURL(innocent_url);
  web::FakeNavigationContext replace_state_context;
  replace_state_context.SetUrl(innocent_url);
  replace_state_context.SetHasCommitted(true);
  replace_state_context.SetIsSameDocument(true);
  web_state_.OnNavigationFinished(&replace_state_context);

  // The async verdict for the phishing URL arrives: unsafe. This triggers
  // `ReloadPage()`, which issues a fresh navigation to the flagged URL.
  client_.run_async_callbacks();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return navigation_manager_->LoadURLWithParamsWasCalled(); }));

  // The forced navigation targets the flagged URL, not the attacker-chosen
  // replaceState URL.
  auto last_params = navigation_manager_->GetLastLoadURLWithParams();
  ASSERT_TRUE(last_params.has_value());
  EXPECT_EQ(phishing_url, last_params->url);

  // The reload request for the flagged URL matches the stored UnsafeResource,
  // so it is cancelled and the error page (interstitial) is displayed.
  auto reload_decision =
      ShouldAllowRequestUrl(phishing_url, /*for_main_frame=*/true,
                            ui::PageTransition::PAGE_TRANSITION_RELOAD);
  EXPECT_TRUE(reload_decision.ShouldCancelNavigation());
  EXPECT_TRUE(reload_decision.ShouldDisplayError());

  // When web-layer error / back-forward repair machinery tries to reload the
  // current WebKit back-forward item (rewritten to innocent_url),
  // `ShouldAllowRequest()` must also cancel and display the error page because
  // an unsafe navigation decision is pending for the host.
  auto repair_decision =
      ShouldAllowRequestUrl(innocent_url, /*for_main_frame=*/true,
                            ui::PageTransition::PAGE_TRANSITION_RELOAD);
  EXPECT_TRUE(repair_decision.ShouldCancelNavigation());
  EXPECT_TRUE(repair_decision.ShouldDisplayError());

  // That blocked repair load commits its own error page, which rebuilds the
  // decider's committed navigation state. A further repair load must still be
  // blocked, so the block must not depend on state that this commit replaces.
  web::FakeNavigationContext error_commit_context;
  error_commit_context.SetUrl(innocent_url);
  error_commit_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&error_commit_context);
  auto second_repair_decision =
      ShouldAllowRequestUrl(innocent_url, /*for_main_frame=*/true,
                            ui::PageTransition::PAGE_TRANSITION_RELOAD);
  EXPECT_TRUE(second_repair_decision.ShouldCancelNavigation());
  EXPECT_TRUE(second_repair_decision.ShouldDisplayError());

  // However, an unrelated clean origin without a pending decision is not
  // blocked.
  GURL clean_url("http://safe.chromium.test/clean.html");
  auto clean_decision =
      ShouldAllowRequestUrl(clean_url, /*for_main_frame=*/true,
                            ui::PageTransition::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(clean_decision.ShouldAllowNavigation());
  EXPECT_FALSE(clean_decision.ShouldDisplayError());
}

// Tests that an early navigation failure (simulating a local error) correctly
// caches the Safe Browsing state for a subsequent back/forward navigation.
TEST_P(SafeBrowsingTabHelperTest,
       UnsafeNavigationFailureFollowedByBackForward) {
  GURL unsafe_url("http://" + FakeSafeBrowsingService::kUnsafeHost);

  // Start navigation to unsafe URL.
  EXPECT_TRUE(ShouldAllowRequestUrl(unsafe_url).ShouldAllowNavigation());

  // Navigation fails pre-commit.
  web::FakeNavigationContext fail_context;
  fail_context.SetUrl(unsafe_url);
  fail_context.SetHasCommitted(false);
  web_state_.OnNavigationFinished(&fail_context);

  // Error page navigation begins (same URL).
  EXPECT_TRUE(ShouldAllowRequestUrl(unsafe_url).ShouldAllowNavigation());

  // Query finishes (unsafe). It sets reload_page_on_commit_.
  RunSyncCallbacksThenAsyncCallbacks();
  base::RunLoop().RunUntilIdle();

  // The error page commits.
  navigation_manager_->AddItem(unsafe_url, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item);

  web::FakeNavigationContext commit_context;
  commit_context.SetUrl(unsafe_url);
  commit_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&commit_context);

  // Clear reload flag by checking it, because this test simulates going forward
  // right away (ignoring the programmatic reload).
  // Wait, if it reloads, FakeNavigationManager just records
  // LoadURLWithParamsWasCalled. The test can just proceed.

  // Start another navigation (to safe URL).
  GURL safe_url("http://chromium.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(safe_url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  ShouldAllowResponseUrl(safe_url);
  navigation_manager_->AddItem(safe_url, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* safe_item = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(safe_item);

  web::FakeNavigationContext safe_context;
  safe_context.SetUrl(safe_url);
  safe_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&safe_context);

  // Go back to the unsafe URL.
  auto back_request_decision =
      ShouldAllowRequestUrl(unsafe_url, /*for_main_frame=*/true,
                            ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK);
  EXPECT_TRUE(back_request_decision.ShouldCancelNavigation());
  EXPECT_TRUE(back_request_decision.ShouldDisplayError());
}

// Tests that an early navigation failure (simulating a local error page load)
// correctly sets up a reload of the page when the async safe browsing check
// completes.
TEST_P(SafeBrowsingTabHelperTest, UnsafeNavigationFailureImmediateReload) {
  GURL unsafe_url("http://" + FakeSafeBrowsingService::kUnsafeHost);

  // Start navigation to unsafe URL.
  EXPECT_TRUE(ShouldAllowRequestUrl(unsafe_url).ShouldAllowNavigation());

  // Navigation fails pre-commit.
  web::FakeNavigationContext fail_context;
  fail_context.SetUrl(unsafe_url);
  fail_context.SetHasCommitted(false);
  web_state_.OnNavigationFinished(&fail_context);

  // Error page navigation begins (same URL).
  EXPECT_TRUE(ShouldAllowRequestUrl(unsafe_url).ShouldAllowNavigation());

  // Query finishes (unsafe). Because the error page navigation is pending
  // with the same URL, it matches the query and sets reload_page_on_commit_.
  RunSyncCallbacksThenAsyncCallbacks();
  base::RunLoop().RunUntilIdle();

  // The error page commits.
  navigation_manager_->AddItem(unsafe_url, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item);

  web::FakeNavigationContext commit_context;
  commit_context.SetUrl(unsafe_url);
  commit_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&commit_context);

  // Check if ReloadPage() triggered LoadURLWithParams.
  EXPECT_TRUE(navigation_manager_->LoadURLWithParamsWasCalled());
  auto last_params = navigation_manager_->GetLastLoadURLWithParams();
  ASSERT_TRUE(last_params.has_value());
  EXPECT_EQ(unsafe_url, last_params->url);
  EXPECT_EQ(static_cast<int>(ui::PAGE_TRANSITION_RELOAD),
            static_cast<int>(last_params->transition_type));

  // Simulate the reload intercept by Safe Browsing.
  auto reload_decision = ShouldAllowRequestUrl(unsafe_url);
  EXPECT_TRUE(reload_decision.ShouldCancelNavigation());
  EXPECT_TRUE(reload_decision.ShouldDisplayError());
}

// Tests that if an asynchronous Safe Browsing check finishes after a local
// error page has already fully committed, the page is immediately reloaded to
// show the warning.
TEST_P(SafeBrowsingTabHelperTest, UnsafeNavigationFailureLateVerdict) {
  GURL unsafe_url("http://" + FakeSafeBrowsingService::kUnsafeHost);

  // Start navigation to unsafe URL.
  EXPECT_TRUE(ShouldAllowRequestUrl(unsafe_url).ShouldAllowNavigation());

  // Navigation fails pre-commit.
  web::FakeNavigationContext fail_context;
  fail_context.SetUrl(unsafe_url);
  fail_context.SetHasCommitted(false);
  web_state_.OnNavigationFinished(&fail_context);

  // Error page navigation begins (same URL).
  EXPECT_TRUE(ShouldAllowRequestUrl(unsafe_url).ShouldAllowNavigation());

  // The error page commits BEFORE the Safe Browsing query finishes.
  navigation_manager_->AddItem(unsafe_url, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item);

  web::FakeNavigationContext commit_context;
  commit_context.SetUrl(unsafe_url);
  commit_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&commit_context);

  // Ensure no reload has happened yet.
  EXPECT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Now the Query finishes (unsafe). Since the error page is currently visible,
  // it should trigger an immediate reload.
  RunSyncCallbacksThenAsyncCallbacks();
  base::RunLoop().RunUntilIdle();

  // Check if ReloadPage() triggered LoadURLWithParams.
  EXPECT_TRUE(navigation_manager_->LoadURLWithParamsWasCalled());
  auto last_params = navigation_manager_->GetLastLoadURLWithParams();
  ASSERT_TRUE(last_params.has_value());
  EXPECT_EQ(unsafe_url, last_params->url);
  EXPECT_EQ(static_cast<int>(ui::PAGE_TRANSITION_RELOAD),
            static_cast<int>(last_params->transition_type));

  // Simulate the reload intercept by Safe Browsing.
  auto reload_decision = ShouldAllowRequestUrl(unsafe_url);
  EXPECT_TRUE(reload_decision.ShouldCancelNavigation());
  EXPECT_TRUE(reload_decision.ShouldDisplayError());
}

// Tests that when a navigation commits and then fails mid-stream (post-commit
// error page load, which bypasses `ShouldAllowRequest`),
// `committed_redirect_chain_` is preserved so a late async Safe Browsing
// verdict still reloads the flagged URL to display the interstitial.
TEST_P(SafeBrowsingTabHelperTest,
       PostCommitNavigationFailureLateAsyncVerdictShowsInterstitial) {
  GURL url("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost);
  ASSERT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Start navigation to `url`; sync check passes while async check remains in
  // flight.
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  ASSERT_FALSE(client_.sync_completion_callbacks_.empty());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));

  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return client_.sync_completion_callbacks_.empty(); }));
  }
  EXPECT_TRUE(ShouldAllowResponseUrl(url).ShouldAllowNavigation());

  // Call #1 (`webView:didCommitNavigation:`): the initial response commits,
  // moving `to_be_committed_redirect_chain_` into `committed_redirect_chain_`.
  navigation_manager_->AddItem(url, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item);
  web::FakeNavigationContext commit_context;
  commit_context.SetUrl(url);
  commit_context.SetHasCommitted(true);
  commit_context.SetError(nil);
  web_state_.OnNavigationFinished(&commit_context);

  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url}));

  // Call #2 (`webView:didFailNavigation:withError:` -> error page commit):
  // the error page commits without calling `ShouldAllowRequest`, reusing the
  // same navigation context/ID. Because `GetError()` is non-nil and matches the
  // committed navigation ID, `committed_redirect_chain_` is preserved.
  commit_context.SetError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorNetworkConnectionLost
             userInfo:nil]);
  web_state_.OnNavigationFinished(&commit_context);

  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url}));
  EXPECT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Now the async verdict arrives (unsafe) and reloads `url`.
  client_.run_async_callbacks();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return navigation_manager_->LoadURLWithParamsWasCalled(); }));

  auto last_params = navigation_manager_->GetLastLoadURLWithParams();
  ASSERT_TRUE(last_params.has_value());
  EXPECT_EQ(url, last_params->url);
  EXPECT_EQ(static_cast<int>(ui::PAGE_TRANSITION_RELOAD),
            static_cast<int>(last_params->transition_type));

  auto reload_decision = ShouldAllowRequestUrl(
      url, /*for_main_frame=*/true, ui::PageTransition::PAGE_TRANSITION_RELOAD);
  EXPECT_TRUE(reload_decision.ShouldCancelNavigation());
  EXPECT_TRUE(reload_decision.ShouldDisplayError());
}

// Tests that when a post-commit navigation failure occurs for a page whose
// in-flight asynchronous Safe Browsing verdict is SAFE, the redirect chain is
// preserved and no reload is triggered when the safe async verdict arrives.
TEST_P(SafeBrowsingTabHelperTest,
       PostCommitNavigationFailureLateSafeAsyncVerdictDoesNotReload) {
  GURL url("http://safe.chromium.test");
  ASSERT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Start navigation to `url`; sync check passes while async check remains in
  // flight.
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  ASSERT_FALSE(client_.sync_completion_callbacks_.empty());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));

  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return client_.sync_completion_callbacks_.empty(); }));
  }
  EXPECT_TRUE(ShouldAllowResponseUrl(url).ShouldAllowNavigation());

  // Call #1 (`webView:didCommitNavigation:`): the initial response commits.
  navigation_manager_->AddItem(url, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item);
  web::FakeNavigationContext commit_context;
  commit_context.SetUrl(url);
  commit_context.SetHasCommitted(true);
  commit_context.SetError(nil);
  web_state_.OnNavigationFinished(&commit_context);

  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url}));

  // Call #2 (`webView:didFailNavigation:withError:` -> error page commit):
  // the error page commits with a non-nil error reusing the same navigation
  // context/ID, preserving the redirect chain.
  commit_context.SetError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorNetworkConnectionLost
             userInfo:nil]);
  web_state_.OnNavigationFinished(&commit_context);

  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url}));
  EXPECT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Deliver the async verdict (SAFE). Unlike unsafe verdicts, which post a task
  // to display the blocking page, a safe verdict reaches
  // `SafeBrowsingTabHelper` synchronously within `run_async_callbacks()`.
  client_.run_async_callbacks();

  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url}));
  EXPECT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());
}

// Tests that when a navigation encounters a post-commit failure while an async
// UNSAFE check is pending, and the user then navigates to a new safe URL, the
// preserved redirect chain is replaced by the new URL, and a subsequent stale
// async verdict for the original unsafe URL does not trigger a reload.
TEST_P(SafeBrowsingTabHelperTest,
       PostCommitErrorPagePreservedChainReplacedByNextNavigation) {
  GURL unsafe_url("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost);
  ASSERT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Start navigation to `unsafe_url`; sync check passes while async check
  // remains in flight.
  EXPECT_TRUE(ShouldAllowRequestUrl(unsafe_url).ShouldAllowNavigation());
  ASSERT_FALSE(client_.sync_completion_callbacks_.empty());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));

  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return client_.sync_completion_callbacks_.empty(); }));
  }
  EXPECT_TRUE(ShouldAllowResponseUrl(unsafe_url).ShouldAllowNavigation());

  // Call #1: `unsafe_url` commits.
  navigation_manager_->AddItem(unsafe_url, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item1 = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item1);
  web::FakeNavigationContext commit_context1;
  commit_context1.SetUrl(unsafe_url);
  commit_context1.SetHasCommitted(true);
  commit_context1.SetError(nil);
  web_state_.OnNavigationFinished(&commit_context1);

  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({unsafe_url}));

  // Call #2: post-commit error page commits for `unsafe_url`, reusing the
  // initial navigation context/ID.
  commit_context1.SetError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorNetworkConnectionLost
             userInfo:nil]);
  web_state_.OnNavigationFinished(&commit_context1);

  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({unsafe_url}));
  EXPECT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // User now navigates to a new safe URL.
  GURL safe_url("http://safe.chromium.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(safe_url).ShouldAllowNavigation());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));
  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return client_.sync_completion_callbacks_.empty(); }));
  }
  EXPECT_TRUE(ShouldAllowResponseUrl(safe_url).ShouldAllowNavigation());

  navigation_manager_->AddItem(safe_url, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item2 = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item2);
  web::FakeNavigationContext commit_context2;
  commit_context2.SetUrl(safe_url);
  commit_context2.SetHasCommitted(true);
  commit_context2.SetError(nil);
  web_state_.OnNavigationFinished(&commit_context2);

  // The redirect chain is now replaced by `{safe_url}`.
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({safe_url}));

  // Deliver the stale async callbacks (containing the unsafe verdict for
  // `unsafe_url`).
  client_.run_async_callbacks();
  ASSERT_TRUE(WaitForPendingUnsafeNavigationDecision(unsafe_url));

  // No reload should have occurred because `unsafe_url` is no longer in the
  // committed redirect chain.
  EXPECT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());
}

// Tests that when a navigation to an async-unsafe URL commits, executes a
// same-document `history.replaceState` to rewrite the URL to an innocent URL,
// and subsequently suffers a post-commit navigation failure, the late async
// UNSAFE verdict reloads the original flagged URL (not the rewritten URL) with
// a `PAGE_TRANSITION_RELOAD` transition and displays the interstitial.
TEST_P(SafeBrowsingTabHelperTest,
       PostCommitNavigationFailureAfterReplaceStateReloadsFlaggedUrl) {
  GURL flagged_url("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost +
                   "/phishing.html");
  GURL innocent_url("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost +
                    "/innocent.html");
  ASSERT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Start navigation to `flagged_url`; sync check passes while async check
  // remains in flight.
  EXPECT_TRUE(ShouldAllowRequestUrl(flagged_url).ShouldAllowNavigation());
  ASSERT_FALSE(client_.sync_completion_callbacks_.empty());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));

  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return client_.sync_completion_callbacks_.empty(); }));
  }
  EXPECT_TRUE(ShouldAllowResponseUrl(flagged_url).ShouldAllowNavigation());

  // Call #1: `flagged_url` commits.
  navigation_manager_->AddItem(flagged_url, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item);
  web::FakeNavigationContext commit_context;
  commit_context.SetUrl(flagged_url);
  commit_context.SetHasCommitted(true);
  commit_context.SetError(nil);
  web_state_.OnNavigationFinished(&commit_context);

  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({flagged_url}));

  // Simulate `history.replaceState({}, '', '/innocent.html')`.
  item->SetURL(innocent_url);
  web::FakeNavigationContext replace_state_context;
  replace_state_context.SetUrl(innocent_url);
  replace_state_context.SetHasCommitted(true);
  replace_state_context.SetIsSameDocument(true);
  web_state_.OnNavigationFinished(&replace_state_context);

  // Call #2: post-commit error page commits (non-same-document, with error),
  // reusing the initial navigation context/ID.
  commit_context.SetError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorNetworkConnectionLost
             userInfo:nil]);
  web_state_.OnNavigationFinished(&commit_context);

  // The chain is preserved as `{flagged_url}`.
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({flagged_url}));
  EXPECT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Deliver the async verdict (UNSAFE for `flagged_url`).
  client_.run_async_callbacks();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return navigation_manager_->LoadURLWithParamsWasCalled(); }));

  auto last_params = navigation_manager_->GetLastLoadURLWithParams();
  ASSERT_TRUE(last_params.has_value());
  EXPECT_EQ(flagged_url, last_params->url);
  EXPECT_EQ(static_cast<int>(ui::PAGE_TRANSITION_RELOAD),
            static_cast<int>(last_params->transition_type));

  auto reload_decision =
      ShouldAllowRequestUrl(flagged_url, /*for_main_frame=*/true,
                            ui::PageTransition::PAGE_TRANSITION_RELOAD);
  EXPECT_TRUE(reload_decision.ShouldCancelNavigation());
  EXPECT_TRUE(reload_decision.ShouldDisplayError());
}

// Tests that navigating to and committing an unchecked URL (such as
// `about:blank`, which bypasses URL checks and has `GetError() == nil`) clears
// `committed_redirect_chain_` so a late async verdict for the previous page is
// treated as stale and does not reload over `about:blank`.
TEST_P(SafeBrowsingTabHelperTest,
       UncheckedUrlNavigationClearsCommittedRedirectChain) {
  GURL url("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost);
  ASSERT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Start and commit a navigation to `url` while its async check is in flight.
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  ASSERT_FALSE(client_.sync_completion_callbacks_.empty());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));

  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return client_.sync_completion_callbacks_.empty(); }));
  }
  EXPECT_TRUE(ShouldAllowResponseUrl(url).ShouldAllowNavigation());

  navigation_manager_->AddItem(url, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item);
  web::FakeNavigationContext commit_context;
  commit_context.SetUrl(url);
  commit_context.SetHasCommitted(true);
  commit_context.SetError(nil);
  web_state_.OnNavigationFinished(&commit_context);

  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url}));

  // Navigate to and commit an unchecked URL (`about:blank`).
  GURL about_blank("about:blank");
  EXPECT_TRUE(ShouldAllowRequestUrl(about_blank).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowResponseUrl(about_blank).ShouldAllowNavigation());
  navigation_manager_->AddItem(about_blank, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* about_item = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(about_item);
  web::FakeNavigationContext about_context;
  about_context.SetUrl(about_blank);
  about_context.SetHasCommitted(true);
  about_context.SetError(nil);
  web_state_.OnNavigationFinished(&about_context);

  EXPECT_TRUE(tab_helper->GetRedirectChain().empty());

  // Trigger the late async verdict for the previous page; it should be ignored
  // as stale and must not trigger a reload.
  client_.run_async_callbacks();
  ASSERT_TRUE(WaitForPendingUnsafeNavigationDecision(url));

  EXPECT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());
}

// Tests that when a new navigation to an unchecked URL (such as `about:blank`)
// fails and commits an error page, its different navigation ID causes
// `committed_redirect_chain_` to be cleared (rather than preserved), ensuring a
// late async verdict for the prior page is ignored and does not reload.
TEST_P(SafeBrowsingTabHelperTest,
       UncheckedUrlErrorPageClearsCommittedRedirectChain) {
  GURL url("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost);
  ASSERT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Start and commit a navigation to `url` while its async check is in flight.
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  ASSERT_FALSE(client_.sync_completion_callbacks_.empty());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));

  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return client_.sync_completion_callbacks_.empty(); }));
  }
  EXPECT_TRUE(ShouldAllowResponseUrl(url).ShouldAllowNavigation());

  navigation_manager_->AddItem(url, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item);
  web::FakeNavigationContext commit_context;
  commit_context.SetUrl(url);
  commit_context.SetHasCommitted(true);
  commit_context.SetError(nil);
  web_state_.OnNavigationFinished(&commit_context);

  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url}));

  // Simulate a new navigation to an unchecked URL (`about:blank`) that fails
  // and commits an error page. Because this is a new navigation with a
  // different navigation ID, `committed_redirect_chain_` is cleared.
  GURL about_blank("about:blank");
  navigation_manager_->AddItem(about_blank, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* about_item = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(about_item);
  web::FakeNavigationContext error_context;
  error_context.SetUrl(about_blank);
  error_context.SetHasCommitted(true);
  error_context.SetError([NSError errorWithDomain:NSURLErrorDomain
                                             code:NSURLErrorCannotFindHost
                                         userInfo:nil]);
  web_state_.OnNavigationFinished(&error_context);

  EXPECT_TRUE(tab_helper->GetRedirectChain().empty());

  // Trigger the late async verdict for the previous page; it should be ignored
  // as stale and must not trigger a reload.
  client_.run_async_callbacks();
  ASSERT_TRUE(WaitForPendingUnsafeNavigationDecision(url));

  EXPECT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());
}

// Tests that when a new checked navigation fails provisionally and commits an
// error page, its different navigation ID causes its pending query to replace
// the previously committed redirect chain (rather than preserving it), so a
// late async verdict for the prior page is ignored and does not reload.
TEST_P(SafeBrowsingTabHelperTest,
       ProvisionalFailureOfCheckedUrlReplacesCommittedRedirectChain) {
  GURL url_a("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost);
  GURL url_b("http://chromium-b.test");
  ASSERT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Start and commit a navigation to `url_a` while its async check is in
  // flight.
  EXPECT_TRUE(ShouldAllowRequestUrl(url_a).ShouldAllowNavigation());
  ASSERT_FALSE(client_.sync_completion_callbacks_.empty());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));

  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return client_.sync_completion_callbacks_.empty(); }));
  }
  EXPECT_TRUE(ShouldAllowResponseUrl(url_a).ShouldAllowNavigation());

  navigation_manager_->AddItem(url_a, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item_a = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item_a);
  web::FakeNavigationContext context_a;
  context_a.SetUrl(url_a);
  context_a.SetHasCommitted(true);
  context_a.SetError(nil);
  web_state_.OnNavigationFinished(&context_a);

  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url_a}));

  // Start a checked navigation to `url_b`, which populates
  // `pending_main_frame_query_`, then fail it before `ShouldAllowResponse()`
  // and commit its error page with a new navigation context/ID.
  EXPECT_TRUE(ShouldAllowRequestUrl(url_b).ShouldAllowNavigation());

  navigation_manager_->AddItem(url_b, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item_b = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item_b);
  web::FakeNavigationContext context_b;
  context_b.SetUrl(url_b);
  context_b.SetHasCommitted(true);
  context_b.SetError([NSError errorWithDomain:NSURLErrorDomain
                                         code:NSURLErrorCannotFindHost
                                     userInfo:nil]);
  ASSERT_NE(context_a.GetNavigationId(), context_b.GetNavigationId());
  web_state_.OnNavigationFinished(&context_b);

  // `url_b`'s pending query replaces `url_a`'s committed chain.
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url_b}));

  // The late async verdict for `url_a` is stale and must not trigger a reload.
  client_.run_async_callbacks();
  ASSERT_TRUE(WaitForPendingUnsafeNavigationDecision(url_a));

  EXPECT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());
}

// Tests that when an async unsafe verdict arrives for a page that already
// performed `history.replaceState` before another navigation committed, the
// stale verdict is recorded so that a subsequent back navigation to the
// replaced URL is blocked.
TEST_P(SafeBrowsingTabHelperTest,
       StaleAsyncUnsafeVerdictBlocksBackNavigationAfterReplaceState) {
  GURL flagged_url("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost +
                   "/phishing.html");
  GURL innocent_url("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost +
                    "/innocent.html");
  GURL url_b("http://chromium-b.test");
  ASSERT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Start and commit `flagged_url` while its async check is in flight.
  EXPECT_TRUE(ShouldAllowRequestUrl(flagged_url).ShouldAllowNavigation());
  ASSERT_FALSE(client_.sync_completion_callbacks_.empty());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));

  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return client_.sync_completion_callbacks_.empty(); }));
  }
  EXPECT_TRUE(ShouldAllowResponseUrl(flagged_url).ShouldAllowNavigation());

  navigation_manager_->AddItem(flagged_url, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item_a = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item_a);
  web::FakeNavigationContext context_a;
  context_a.SetUrl(flagged_url);
  context_a.SetHasCommitted(true);
  context_a.SetError(nil);
  web_state_.OnNavigationFinished(&context_a);

  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({flagged_url}));

  // Simulate `history.replaceState` to `innocent_url`.
  item_a->SetURL(innocent_url);
  web::FakeNavigationContext replace_state_context;
  replace_state_context.SetUrl(innocent_url);
  replace_state_context.SetHasCommitted(true);
  replace_state_context.SetIsSameDocument(true);
  web_state_.OnNavigationFinished(&replace_state_context);

  // Start a checked navigation to `url_b`, fail it provisionally before
  // `ShouldAllowResponse()`, and commit its error page with `context_b`.
  EXPECT_TRUE(ShouldAllowRequestUrl(url_b).ShouldAllowNavigation());

  navigation_manager_->AddItem(url_b, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item_b = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item_b);
  web::FakeNavigationContext context_b;
  context_b.SetUrl(url_b);
  context_b.SetHasCommitted(true);
  context_b.SetError([NSError errorWithDomain:NSURLErrorDomain
                                         code:NSURLErrorCannotFindHost
                                     userInfo:nil]);
  ASSERT_NE(context_a.GetNavigationId(), context_b.GetNavigationId());
  web_state_.OnNavigationFinished(&context_b);

  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url_b}));

  // Deliver late async verdict for `flagged_url`. It must not reload `url_b`.
  client_.run_async_callbacks();
  ASSERT_TRUE(WaitForPendingUnsafeNavigationDecision(flagged_url));

  EXPECT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Simulate a back navigation to `innocent_url`. The recorded unsafe decision
  // must block it.
  auto back_decision =
      ShouldAllowRequestUrl(innocent_url, /*for_main_frame=*/true,
                            ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK);
  EXPECT_TRUE(back_decision.ShouldCancelNavigation());
  EXPECT_TRUE(back_decision.ShouldDisplayError());
}

// Tests that when a navigation has reached `ShouldAllowResponse()` and
// populated `to_be_committed_redirect_chain_`, an interim navigation starting
// and populating `pending_main_frame_query_` does not supersede the committed
// redirect chain when the first navigation commits.
TEST_P(SafeBrowsingTabHelperTest,
       ToBeCommittedRedirectChainPreferredOverPendingMainFrameQuery) {
  GURL url_a("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost);
  GURL url_b("http://chromium-b.test");
  ASSERT_FALSE(navigation_manager_->LoadURLWithParamsWasCalled());

  // Start `url_a`, let sync check pass, and call
  // `ShouldAllowResponseUrl(url_a)` so `to_be_committed_redirect_chain_` holds
  // `{url_a}` while its async check remains in flight.
  EXPECT_TRUE(ShouldAllowRequestUrl(url_a).ShouldAllowNavigation());
  ASSERT_FALSE(client_.sync_completion_callbacks_.empty());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));

  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return client_.sync_completion_callbacks_.empty(); }));
  }
  EXPECT_TRUE(ShouldAllowResponseUrl(url_a).ShouldAllowNavigation());

  // Before `url_a` commits, `url_b` starts and populates
  // `pending_main_frame_query_`.
  EXPECT_TRUE(ShouldAllowRequestUrl(url_b).ShouldAllowNavigation());

  // Commit `url_a`.
  navigation_manager_->AddItem(url_a, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item_a = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item_a);
  web::FakeNavigationContext context_a;
  context_a.SetUrl(url_a);
  context_a.SetHasCommitted(true);
  context_a.SetError(nil);
  web_state_.OnNavigationFinished(&context_a);

  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url_a}));

  // Run async check which determines `url_a` is unsafe. It should reload
  // `url_a`.
  client_.run_async_callbacks();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return navigation_manager_->LoadURLWithParamsWasCalled(); }));
  auto last_params = navigation_manager_->GetLastLoadURLWithParams();
  ASSERT_TRUE(last_params.has_value());
  EXPECT_EQ(url_a, last_params->url);
  EXPECT_EQ(static_cast<int>(ui::PAGE_TRANSITION_RELOAD),
            static_cast<int>(last_params->transition_type));
}

// Tests that a post-commit error page matching the committed navigation ID
// preserves the committed redirect chain even if an uncommitted navigation
// started and populated `pending_main_frame_query_` in the interim.
TEST_P(SafeBrowsingTabHelperTest,
       PostCommitErrorPageIgnoresInterimUncommittedNavigation) {
  GURL url_a("http://" + FakeSafeBrowsingService::kAsyncUnsafeHost);
  GURL url_b("http://chromium-b.test");

  // Navigation A starts and response is allowed before async check completes.
  EXPECT_TRUE(ShouldAllowRequestUrl(url_a).ShouldAllowNavigation());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));
  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return client_.sync_completion_callbacks_.empty(); }));
  }
  EXPECT_TRUE(ShouldAllowResponseUrl(url_a).ShouldAllowNavigation());

  // Navigation A commits.
  navigation_manager_->AddItem(url_a, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item_a = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item_a);

  web::FakeNavigationContext context_a;
  context_a.SetUrl(url_a);
  context_a.SetHasCommitted(true);
  context_a.SetError(nil);
  web_state_.OnNavigationFinished(&context_a);

  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url_a}));

  // Interim navigation B starts and populates `pending_main_frame_query_`.
  EXPECT_TRUE(ShouldAllowRequestUrl(url_b).ShouldAllowNavigation());

  // Navigation A subsequently commits an error page (reusing `context_a`).
  context_a.SetError([NSError errorWithDomain:NSURLErrorDomain
                                         code:NSURLErrorTimedOut
                                     userInfo:nil]);
  web_state_.OnNavigationFinished(&context_a);

  // The committed redirect chain for A must be preserved and not overwritten
  // by interim query state for B.
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url_a}));

  // Run async check which determines `url_a` is unsafe.
  client_.run_async_callbacks();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return navigation_manager_->LoadURLWithParamsWasCalled(); }));
  auto last_params = navigation_manager_->GetLastLoadURLWithParams();
  ASSERT_TRUE(last_params.has_value());
  EXPECT_EQ(url_a, last_params->url);
}

// Tests that a post-commit error page does not clear pending main frame state
// for a newer navigation.
TEST_P(SafeBrowsingTabHelperTest,
       PostCommitErrorPageDoesNotClearNewerPendingQuery) {
  GURL url_a("http://chromium-a.test");
  GURL url_c("http://" + FakeSafeBrowsingService::kUnsafeHost);

  // Navigation A starts and response is allowed before async check completes.
  EXPECT_TRUE(ShouldAllowRequestUrl(url_a).ShouldAllowNavigation());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(^() {
        client_.run_sync_callbacks();
      }));
  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return client_.sync_completion_callbacks_.empty(); }));
  }
  EXPECT_TRUE(ShouldAllowResponseUrl(url_a).ShouldAllowNavigation());

  // Navigation A commits.
  navigation_manager_->AddItem(url_a, ui::PAGE_TRANSITION_LINK);
  web::NavigationItem* item_a = navigation_manager_->GetItemAtIndex(
      navigation_manager_->GetItemCount() - 1);
  navigation_manager_->SetLastCommittedItem(item_a);

  web::FakeNavigationContext context_a;
  context_a.SetUrl(url_a);
  context_a.SetHasCommitted(true);
  context_a.SetError(nil);
  web_state_.OnNavigationFinished(&context_a);

  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url_a}));

  // Request sync-UNSAFE URL C.
  EXPECT_TRUE(ShouldAllowRequestUrl(url_c).ShouldAllowNavigation());

  // Navigation A subsequently commits an error page (reusing `context_a`).
  context_a.SetError([NSError errorWithDomain:NSURLErrorDomain
                                         code:NSURLErrorTimedOut
                                     userInfo:nil]);
  web_state_.OnNavigationFinished(&context_a);

  // The committed redirect chain for A must be preserved.
  EXPECT_EQ(tab_helper->GetRedirectChain(), std::vector<GURL>({url_a}));

  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url_c);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());

  NSError* decision_error = response_decision.GetDisplayError();
  EXPECT_NSEQ(decision_error.domain, kSafeBrowsingErrorDomain);
  EXPECT_EQ(static_cast<SafeBrowsingErrorCode>(decision_error.code),
            SafeBrowsingErrorCode::kUnsafeResource);
}

// Tests that `HasPendingMainFrameChecks()` tracks both sync and async checks
// across pending, to-be-committed, and committed main-frame states.
TEST_P(SafeBrowsingTabHelperTest, HasPendingMainFrameChecksWithAsyncChecks) {
  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  EXPECT_FALSE(tab_helper->HasPendingMainFrameChecks());

  GURL url("http://chromium.test/page.html");
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  EXPECT_TRUE(tab_helper->HasPendingMainFrameChecks());

  // Completing only the sync check leaves the async check pending across
  // response and commit.
  client_.run_sync_callbacks();
  EXPECT_TRUE(tab_helper->HasPendingMainFrameChecks());

  EXPECT_TRUE(ShouldAllowResponseUrl(url).ShouldAllowNavigation());
  EXPECT_TRUE(tab_helper->HasPendingMainFrameChecks());

  web::FakeNavigationContext commit_context;
  commit_context.SetUrl(url);
  commit_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&commit_context);
  EXPECT_TRUE(tab_helper->HasPendingMainFrameChecks());

  // Completing the async check clears the pending check state.
  client_.run_async_callbacks();
  EXPECT_FALSE(tab_helper->HasPendingMainFrameChecks());
}

// Tests that `HasPendingMainFrameChecks()` returns `false` once the sync check
// completes when async checks are disabled.
TEST_P(SafeBrowsingTabHelperTest,
       HasPendingMainFrameChecksWhenAsyncChecksDisabled) {
  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);
  client_.set_should_force_sync_real_time_url_checks(true);

  GURL url("http://chromium.test/sync_only.html");
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  EXPECT_TRUE(tab_helper->HasPendingMainFrameChecks());

  // With async checks disabled, `FakeSafeBrowsingService::CreateUrlChecker`
  // posts completion on the current task runner.
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !tab_helper->HasPendingMainFrameChecks(); }));
}

// Tests that `IsUrlInCurrentOrPendingNavigation()` matches URLs (including
// redirects and URLs with fragments) across pending, to-be-committed, and
// committed states, and stops matching superseded or interrupted navigations.
TEST_P(SafeBrowsingTabHelperTest,
       IsUrlInCurrentOrPendingNavigationLifecycleAndRedirects) {
  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);

  GURL url1("http://chromium1.test/start.html");
  GURL url2("http://chromium2.test/final.html");
  GURL url2_with_ref("http://chromium2.test/final.html#section");
  GURL unrelated_url("http://other.test/");

  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(url1));

  // Start `url1` and redirect to `url2`.
  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(url1));
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(unrelated_url));

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  // Even before `DidRedirectNavigation()`, `url1` is tracked as the candidate
  // redirect source.
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(url1));
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(url2));
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(url2_with_ref));

  SimulateMainFrameRedirect();
  client_.run_sync_callbacks();
  client_.run_async_callbacks();

  EXPECT_TRUE(ShouldAllowResponseUrl(url2).ShouldAllowNavigation());
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(url1));
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(url2));

  web::FakeNavigationContext commit_context;
  commit_context.SetUrl(url2);
  commit_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&commit_context);
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(url1));
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(url2));

  // Start an interrupted request (`interrupted_url`), then supersede it without
  // redirecting by navigating to and committing `url3`.
  GURL interrupted_url("http://interrupted.test/");
  GURL url3("http://chromium3.test/");
  EXPECT_TRUE(ShouldAllowRequestUrl(interrupted_url).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  client_.run_sync_callbacks();
  client_.run_async_callbacks();
  EXPECT_TRUE(ShouldAllowResponseUrl(url3).ShouldAllowNavigation());

  web::FakeNavigationContext url3_commit_context;
  url3_commit_context.SetUrl(url3);
  url3_commit_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&url3_commit_context);

  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(url1));
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(url2));
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(interrupted_url));
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(url3));
}

// Tests that a redirect chain cancelled by a Safe Browsing blocking decision is
// excluded from both `HasPendingMainFrameChecks()` and
// `IsUrlInCurrentOrPendingNavigation()`.
TEST_P(SafeBrowsingTabHelperTest,
       CancelledNavigationExcludedFromPendingChecksAndCurrentNavigation) {
  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);

  GURL safe_redirect_source("http://chromium1.test/redirect.html");
  GURL unsafe_target("http://" + FakeSafeBrowsingService::kUnsafeHost);

  EXPECT_TRUE(
      ShouldAllowRequestUrl(safe_redirect_source).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(unsafe_target).ShouldAllowNavigation());
  SimulateMainFrameRedirect();

  // Complete sync checks so `unsafe_target` receives a cancel decision while
  // async callbacks are still unrun.
  client_.run_sync_callbacks();

  EXPECT_FALSE(tab_helper->HasPendingMainFrameChecks());
  EXPECT_FALSE(
      tab_helper->IsUrlInCurrentOrPendingNavigation(safe_redirect_source));
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(unsafe_target));
}

// Tests that when an unsafe redirect source redirects to a safe target and the
// blocking sync decision arrives before `ShouldAllowResponse()` cancels the
// navigation, `safe_target` is cleared rather than left pending or promoted on
// error-page commit.
TEST_P(SafeBrowsingTabHelperTest,
       UnsafeRedirectSourceToSafeTargetCancelledBeforeResponse) {
  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);

  GURL unsafe_source("http://" + FakeSafeBrowsingService::kUnsafeHost);
  GURL safe_target("http://chromium.test/safe_target.html");

  EXPECT_TRUE(ShouldAllowRequestUrl(unsafe_source).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(safe_target).ShouldAllowNavigation());
  SimulateMainFrameRedirect();
  client_.run_sync_callbacks();

  EXPECT_TRUE(ShouldAllowResponseUrl(safe_target).ShouldCancelNavigation());
  EXPECT_FALSE(tab_helper->HasPendingMainFrameChecks());
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(unsafe_source));
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(safe_target));

  // Committing the Safe Browsing error page must not promote `safe_target` into
  // the committed redirect chain.
  web::FakeNavigationContext error_commit_context;
  error_commit_context.SetUrl(unsafe_source);
  error_commit_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&error_commit_context);
  EXPECT_TRUE(tab_helper->GetRedirectChain().empty());
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(safe_target));
}

// Tests that when an unsafe redirect source redirects to a safe target and
// `ShouldAllowResponse()` is cancelled asynchronously once the blocking sync
// decision arrives, `safe_target` is cleared rather than left pending or
// promoted on error-page commit.
TEST_P(SafeBrowsingTabHelperTest,
       UnsafeRedirectSourceToSafeTargetCancelledAfterResponse) {
  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);

  GURL unsafe_source("http://" + FakeSafeBrowsingService::kUnsafeHost);
  GURL safe_target("http://chromium.test/safe_target.html");

  EXPECT_TRUE(ShouldAllowRequestUrl(unsafe_source).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(safe_target).ShouldAllowNavigation());
  SimulateMainFrameRedirect();

  NSURLResponse* response =
      [[NSURLResponse alloc] initWithURL:net::NSURLWithGURL(safe_target)
                                MIMEType:@"text/html"
                   expectedContentLength:0
                        textEncodingName:nil];
  base::test::TestFuture<web::WebStatePolicyDecider::PolicyDecision>
      decision_future;
  web_state_.ShouldAllowResponse(
      response,
      web::WebStatePolicyDecider::ResponseInfo(/*for_main_frame=*/true),
      decision_future.GetCallback());
  EXPECT_FALSE(decision_future.IsReady());

  client_.run_sync_callbacks();
  EXPECT_TRUE(decision_future.Get().ShouldCancelNavigation());
  EXPECT_FALSE(tab_helper->HasPendingMainFrameChecks());
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(unsafe_source));
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(safe_target));

  // Committing the Safe Browsing error page must not promote `safe_target` into
  // the committed redirect chain.
  web::FakeNavigationContext error_commit_context;
  error_commit_context.SetUrl(unsafe_source);
  error_commit_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&error_commit_context);
  EXPECT_TRUE(tab_helper->GetRedirectChain().empty());
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(safe_target));
}

// Tests that `HasPendingMainFrameChecks()` clears properly when a redirect
// source completes its sync and async checks while temporarily held in
// `previous_main_frame_query_` (after `ShouldAllowRequest()` for the redirect
// target, before `DidRedirectNavigation()`).
TEST_P(SafeBrowsingTabHelperTest,
       RedirectCheckCompletedBeforeDidRedirectNavigation) {
  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);

  GURL redirect_source("http://chromium1.test/redirect_source.html");
  GURL redirect_target("http://chromium2.test/redirect_target.html");

  EXPECT_TRUE(ShouldAllowRequestUrl(redirect_source).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(redirect_target).ShouldAllowNavigation());

  // Complete all currently queued callbacks before `DidRedirectNavigation()`
  // moves `redirect_source` into `pending_main_frame_redirect_chain_`.
  client_.run_sync_callbacks();
  client_.run_async_callbacks();

  SimulateMainFrameRedirect();
  EXPECT_TRUE(ShouldAllowResponseUrl(redirect_target).ShouldAllowNavigation());

  web::FakeNavigationContext commit_context;
  commit_context.SetUrl(redirect_target);
  commit_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&commit_context);

  EXPECT_FALSE(tab_helper->HasPendingMainFrameChecks());
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(redirect_source));
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(redirect_target));
}

// Tests that receiving a response for an unchecked URL (such as `about:blank`)
// after starting a checkable navigation clears the superseded pending query
// before commit.
TEST_P(SafeBrowsingTabHelperTest,
       UncheckedResponseClearsSupersededPendingQuery) {
  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);

  GURL aborted_url("http://chromium.test/aborted.html");
  GURL unchecked_url("about:blank");

  EXPECT_TRUE(ShouldAllowRequestUrl(aborted_url).ShouldAllowNavigation());
  EXPECT_TRUE(tab_helper->HasPendingMainFrameChecks());
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(aborted_url));

  EXPECT_TRUE(ShouldAllowRequestUrl(unchecked_url).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowResponseUrl(unchecked_url).ShouldAllowNavigation());

  EXPECT_FALSE(tab_helper->HasPendingMainFrameChecks());
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(aborted_url));
}

// Tests that committing an unchecked URL (such as `about:blank` or
// `chrome://newtab`) without a prior `ShouldAllowResponse()` call clears any
// superseded pending query from `HasPendingMainFrameChecks()` and
// `IsUrlInCurrentOrPendingNavigation()`.
TEST_P(SafeBrowsingTabHelperTest,
       UncheckedCommittedNavigationClearsSupersededPendingQuery) {
  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);

  GURL aborted_url("http://chromium.test/aborted.html");
  GURL unchecked_url("about:blank");

  EXPECT_TRUE(ShouldAllowRequestUrl(aborted_url).ShouldAllowNavigation());
  EXPECT_TRUE(tab_helper->HasPendingMainFrameChecks());
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(aborted_url));

  // Preempt `aborted_url` by navigating to and committing `about:blank`.
  EXPECT_TRUE(ShouldAllowRequestUrl(unchecked_url).ShouldAllowNavigation());
  web::FakeNavigationContext unchecked_commit_context;
  unchecked_commit_context.SetUrl(unchecked_url);
  unchecked_commit_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&unchecked_commit_context);

  EXPECT_FALSE(tab_helper->HasPendingMainFrameChecks());
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(aborted_url));
}

// Tests that when a sync-unsafe redirect source completes its check while held
// in `previous_main_frame_query_` (before `DidRedirectNavigation()`), the
// redirect target response is cancelled with an error and pending state is
// cleared.
TEST_P(SafeBrowsingTabHelperTest,
       SyncUnsafeRedirectCheckCompletedBeforeDidRedirectNavigation) {
  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);

  GURL unsafe_source("http://" + FakeSafeBrowsingService::kUnsafeHost);
  GURL safe_target("http://chromium.test/safe_target.html");

  EXPECT_TRUE(ShouldAllowRequestUrl(unsafe_source).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(safe_target).ShouldAllowNavigation());

  client_.run_sync_callbacks();
  ASSERT_TRUE(WaitForPendingUnsafeNavigationDecision(unsafe_source));
  EXPECT_FALSE(client_.main_frame_cancellation_decided_called());

  SimulateMainFrameRedirect();
  EXPECT_TRUE(client_.main_frame_cancellation_decided_called());
  EXPECT_FALSE(tab_helper->HasPendingMainFrameChecks());
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(unsafe_source));
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(safe_target));

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(safe_target);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
  EXPECT_TRUE(response_decision.ShouldDisplayError());
}

// Tests that when an async-unsafe redirect source completes both its sync and
// async checks while held in `previous_main_frame_query_` (before
// `DidRedirectNavigation()`), the redirect target response is cancelled with an
// error and `HasPendingMainFrameChecks()` does not stay wedged at `true`.
TEST_P(SafeBrowsingTabHelperTest,
       AsyncUnsafeRedirectCheckCompletedBeforeDidRedirectNavigation) {
  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);

  GURL async_unsafe_source("http://" +
                           FakeSafeBrowsingService::kAsyncUnsafeHost);
  GURL safe_target("http://chromium.test/safe_target.html");

  EXPECT_TRUE(
      ShouldAllowRequestUrl(async_unsafe_source).ShouldAllowNavigation());
  EXPECT_TRUE(ShouldAllowRequestUrl(safe_target).ShouldAllowNavigation());

  client_.run_sync_callbacks();
  client_.run_async_callbacks();
  ASSERT_TRUE(WaitForPendingUnsafeNavigationDecision(async_unsafe_source));
  EXPECT_FALSE(client_.main_frame_cancellation_decided_called());

  SimulateMainFrameRedirect();
  EXPECT_TRUE(client_.main_frame_cancellation_decided_called());
  EXPECT_FALSE(tab_helper->HasPendingMainFrameChecks());
  EXPECT_FALSE(
      tab_helper->IsUrlInCurrentOrPendingNavigation(async_unsafe_source));
  EXPECT_FALSE(tab_helper->IsUrlInCurrentOrPendingNavigation(safe_target));

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(safe_target);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
  EXPECT_TRUE(response_decision.ShouldDisplayError());
}

// Tests that `HasPendingMainFrameChecks()` and
// `IsUrlInCurrentOrPendingNavigation()` remain active across a post-commit
// error page reusing the committed `NavigationContext` until the in-flight
// async check finishes.
TEST_P(SafeBrowsingTabHelperTest,
       HasPendingMainFrameChecksPreservedAcrossPostCommitErrorPage) {
  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(&web_state_);
  ASSERT_TRUE(tab_helper);

  GURL url("http://chromium.test/page.html");
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  client_.run_sync_callbacks();
  EXPECT_TRUE(ShouldAllowResponseUrl(url).ShouldAllowNavigation());

  web::FakeNavigationContext commit_context;
  commit_context.SetUrl(url);
  commit_context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&commit_context);
  EXPECT_TRUE(tab_helper->HasPendingMainFrameChecks());
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(url));

  // Simulate a post-commit error page reusing `commit_context`.
  commit_context.SetError([NSError errorWithDomain:NSURLErrorDomain
                                              code:NSURLErrorTimedOut
                                          userInfo:nil]);
  web_state_.OnNavigationFinished(&commit_context);
  EXPECT_TRUE(tab_helper->HasPendingMainFrameChecks());
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(url));

  client_.run_async_callbacks();
  EXPECT_FALSE(tab_helper->HasPendingMainFrameChecks());
  EXPECT_TRUE(tab_helper->IsUrlInCurrentOrPendingNavigation(url));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SafeBrowsingTabHelperTest,
    testing::Combine(
        testing::Values(
            SafeBrowsingDecisionTimingWithAsync::kBeforeResponseAsyncDisabled,
            SafeBrowsingDecisionTimingWithAsync::kBeforeResponseAsyncEnabled,
            SafeBrowsingDecisionTimingWithAsync::kAfterResponseAsyncDisabled,
            SafeBrowsingDecisionTimingWithAsync::kAfterResponseAsyncEnabled),
        testing::Bool()));
