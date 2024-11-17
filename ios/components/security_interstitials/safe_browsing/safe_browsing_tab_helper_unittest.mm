// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
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
      : browser_state_(std::make_unique<web::FakeBrowserState>()) {
    use_async_safe_browsing_ = IsAsyncEnabled();
    scoped_feature_list_.InitWithFeatureState(
        safe_browsing::kSafeBrowsingAsyncRealTimeCheck,
        use_async_safe_browsing_);
    SafeBrowsingQueryManager::CreateForWebState(&web_state_, &client_);
    SafeBrowsingTabHelper::CreateForWebState(&web_state_, &client_);
    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);
    SafeBrowsingUnsafeResourceContainer::CreateForWebState(&web_state_);
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    web_state_.SetNavigationManager(std::move(navigation_manager));
    web_state_.SetBrowserState(browser_state_.get());
  }

  // Converts testing paramater to a boolean which represents if the feature
  // should be enabled or disabled for test run.
  bool IsAsyncEnabled() {
    return GetParam() == SafeBrowsingDecisionTimingWithAsync::
                             kBeforeResponseAsyncEnabled ||
           GetParam() ==
               SafeBrowsingDecisionTimingWithAsync::kAfterResponseAsyncEnabled;
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
    if (base::FeatureList::IsEnabled(
            safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(^() {
            client_.run_sync_callbacks();
          }));
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(^() {
            client_.run_async_callbacks();
          }));
    }

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
  bool use_async_safe_browsing_;
  base::test::ScopedFeatureList scoped_feature_list_;
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
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
    GURL url("http://chromium.test");
    EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
    RunAsyncCallbacksThenSyncCallbacks();

    web::WebStatePolicyDecider::PolicyDecision response_decision =
        ShouldAllowResponseUrl(url);
    EXPECT_TRUE(response_decision.ShouldAllowNavigation());
  }
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
}

// Tests the case of a single navigation request and response, for a URL that is
// unsafe where the async query is completed before the sync query.
TEST_P(SafeBrowsingTabHelperTest,
       SingleUnsafeRequestAndResponseAsyncQueryFirst) {
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
    GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
    EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
    RunAsyncCallbacksThenSyncCallbacks();

    web::WebStatePolicyDecider::PolicyDecision response_decision =
        ShouldAllowResponseUrl(url);
    EXPECT_TRUE(response_decision.ShouldCancelNavigation());
  }
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
// responses are not re-used.
TEST_P(SafeBrowsingTabHelperTest, RepeatedRequestsGetDistinctResponse) {
  // Compare the NSError objects.
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldDisplayError());

  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();

  web::WebStatePolicyDecider::PolicyDecision response_decision2 =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision2.ShouldDisplayError());
  EXPECT_NE(response_decision.GetDisplayError(),
            response_decision2.GetDisplayError());
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

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(response_url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
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
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
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
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
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
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
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
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
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
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
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

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  RunSyncCallbacksThenAsyncCallbacks();
  SimulateMainFrameRedirect();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url2);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
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
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url2);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
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
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(safe_url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
  EXPECT_FALSE(client_.main_frame_cancellation_decided_called());
}

// Tests sync check and ShouldAllowResponse() complete, but async
// check returns after a page loads. Tests that the async check forcefully
// reloads the page.
TEST_P(SafeBrowsingTabHelperTest,
       UnsafeCommittedRedirectChainReloadAndResponse) {
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
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
    auto main_frame_reload_request_decision =
        ShouldAllowRequestUrl(url, /*for_main_frame=*/true,
                              ui::PageTransition::PAGE_TRANSITION_RELOAD);
    EXPECT_TRUE(main_frame_reload_request_decision.ShouldAllowNavigation());
    RunSyncCallbacksThenAsyncCallbacks();
    auto main_frame_reload_response_decision = ShouldAllowResponseUrl(url);
    EXPECT_TRUE(main_frame_reload_response_decision.ShouldCancelNavigation());
    EXPECT_TRUE(main_frame_reload_response_decision.ShouldDisplayError());
  }
}

// Tests sync check and ShouldAllowResponse() complete, and async
// check completes before a page committed. Tests that the async check allows
// the navigation to go through and reloads when the page commits.
TEST_P(SafeBrowsingTabHelperTest,
       UnsafeToBeCommittedRedirectChainReloadAndResponse) {
  if (base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
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
    auto main_frame_reload_request_decision =
        ShouldAllowRequestUrl(url, /*for_main_frame=*/true,
                              ui::PageTransition::PAGE_TRANSITION_RELOAD);
    EXPECT_TRUE(main_frame_reload_request_decision.ShouldAllowNavigation());
    RunSyncCallbacksThenAsyncCallbacks();
    auto main_frame_reload_response_decision = ShouldAllowResponseUrl(url);
    EXPECT_TRUE(main_frame_reload_response_decision.ShouldCancelNavigation());
    EXPECT_TRUE(main_frame_reload_response_decision.ShouldDisplayError());
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName */,
    SafeBrowsingTabHelperTest,
    testing::Values(
        SafeBrowsingDecisionTimingWithAsync::kBeforeResponseAsyncDisabled,
        SafeBrowsingDecisionTimingWithAsync::kBeforeResponseAsyncEnabled,
        SafeBrowsingDecisionTimingWithAsync::kAfterResponseAsyncDisabled,
        SafeBrowsingDecisionTimingWithAsync::kAfterResponseAsyncEnabled));
