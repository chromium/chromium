// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/test/bind.h"
#import "base/test/run_until.h"
#import "base/test/scoped_feature_list.h"
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
    : public testing::TestWithParam<SafeBrowsingDecisionTimingWithAsync> {
 protected:
  SafeBrowsingTabHelperTest()
      : browser_state_(std::make_unique<web::FakeBrowserState>()),
        client_(/*pref_service=*/nullptr) {
    SafeBrowsingQueryManager::CreateForWebState(&web_state_, &client_);
    SafeBrowsingTabHelper::CreateForWebState(&web_state_, &client_);
    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);
    SafeBrowsingUnsafeResourceContainer::CreateForWebState(&web_state_);
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    web_state_.SetBrowserState(browser_state_.get());
  }

  // Whether Safe Browsing decisions arrive before calls to
  // ShouldAllowResponseUrl().
  bool SafeBrowsingDecisionArrivesBeforeResponse() const {
    return GetParam() == SafeBrowsingDecisionTimingWithAsync::
                             kBeforeResponseAsyncDisabled ||
           GetParam() ==
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

  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
    RunSyncCallbacksThenAsyncCallbacks();
    SimulateMainFrameRedirect();

    web::WebStatePolicyDecider::PolicyDecision response_decision =
        ShouldAllowResponseUrl(url2);
    EXPECT_TRUE(response_decision.ShouldCancelNavigation());
    EXPECT_TRUE(response_decision.ShouldDisplayError());
  } else {
    auto request_decision = ShouldAllowRequestUrl(url2);
    EXPECT_TRUE(request_decision.ShouldCancelNavigation());
    EXPECT_TRUE(request_decision.ShouldDisplayError());
  }
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

INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName */,
    SafeBrowsingTabHelperTest,
    testing::Values(
        SafeBrowsingDecisionTimingWithAsync::kBeforeResponseAsyncDisabled,
        SafeBrowsingDecisionTimingWithAsync::kBeforeResponseAsyncEnabled,
        SafeBrowsingDecisionTimingWithAsync::kAfterResponseAsyncDisabled,
        SafeBrowsingDecisionTimingWithAsync::kAfterResponseAsyncEnabled));
