// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"

#import <Foundation/Foundation.h>

#import "base/test/scoped_feature_list.h"
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
#import "net/base/mac/url_conversions.h"
#import "services/network/public/mojom/fetch_api.mojom.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
enum class SafeBrowsingDecisionTiming { kBeforeResponse, kAfterResponse };
}

class SafeBrowsingTabHelperTest
    : public testing::TestWithParam<SafeBrowsingDecisionTiming> {
 protected:
  SafeBrowsingTabHelperTest()
      : task_environment_(web::WebTaskEnvironment::IO_MAINLOOP),
        browser_state_(std::make_unique<web::FakeBrowserState>()) {
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
    return GetParam() == SafeBrowsingDecisionTiming::kBeforeResponse;
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
        /*has_user_gesture=*/false);
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

  // Stores an UnsafeResource for `url` in the query manager.  It is expected
  // that an UnsafeResource is stored before check completion for unsafe URLs
  // that show an error page.
  void StoreUnsafeResource(const GURL& url, bool is_main_frame = true) {
    security_interstitials::UnsafeResource resource;
    resource.url = url;
    resource.request_destination =
        is_main_frame ? network::mojom::RequestDestination::kDocument
                      : network::mojom::RequestDestination::kIframe;
    resource.weak_web_state = web_state_.GetWeakPtr();
    SafeBrowsingQueryManager::FromWebState(&web_state_)
        ->StoreUnsafeResource(resource);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::FakeBrowserState> browser_state_;
  web::FakeWebState web_state_;
  web::FakeNavigationManager* navigation_manager_ = nullptr;
  FakeSafeBrowsingClient client_;
};

// Tests the case of a single navigation request and response, for a URL that is
// safe.
TEST_P(SafeBrowsingTabHelperTest, SingleSafeRequestAndResponse) {
  GURL url("http://chromium.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of a single navigation request and response, for a URL that is
// unsafe.
TEST_P(SafeBrowsingTabHelperTest, SingleUnsafeRequestAndResponse) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  StoreUnsafeResource(url);

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

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

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

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
  StoreUnsafeResource(request_url);

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(response_url);
  EXPECT_TRUE(response_decision.ShouldCancelNavigation());
}

// Tests the case of a single navigation request followed by multiple responses
// for the same URL.
TEST_P(SafeBrowsingTabHelperTest, RepeatedResponse) {
  GURL url("http://chromium.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

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
  StoreUnsafeResource(url3);

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

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
  StoreUnsafeResource(url);

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url);
  EXPECT_TRUE(response_decision.ShouldDisplayError());

  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

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

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

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

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(response_url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of a single sub frame navigation request and response, for a
// URL that is safe.
TEST_P(SafeBrowsingTabHelperTest, SafeSubFrameRequestAndResponse) {
  GURL url("http://chromium_sub_frame.test");
  SimulateSafeMainFrameLoad();

  // Execute ShouldAllowRequest() for a safe subframe navigation.
  auto sub_frame_request_decision =
      ShouldAllowRequestUrl(url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_request_decision.ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  // Verify that the sub frame navigation is allowed.
  web::WebStatePolicyDecider::PolicyDecision sub_frame_response_decision =
      ShouldAllowResponseUrl(url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_response_decision.ShouldAllowNavigation());
}

// Tests the case of a single safe sub frame navigation request and response,
// where the response URL has a different hash fragment than the request.
TEST_P(SafeBrowsingTabHelperTest,
       SafeSubFrameRequestAndResponseWithDifferingRef) {
  GURL request_url("http://chromium_sub_frame.test");
  GURL response_url("http://chromium_sub_frame.test#different_hash");
  SimulateSafeMainFrameLoad();

  // Execute ShouldAllowRequest() for a safe subframe navigation.
  auto sub_frame_request_decision =
      ShouldAllowRequestUrl(request_url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_request_decision.ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  // Verify that the sub frame navigation is allowed.
  web::WebStatePolicyDecider::PolicyDecision sub_frame_response_decision =
      ShouldAllowResponseUrl(response_url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_response_decision.ShouldAllowNavigation());
}

// Tests the case of a single safe sub frame navigation request and response,
// where the response URL has a different query string than the request.
TEST_P(SafeBrowsingTabHelperTest,
       SafeSubFrameRequestAndResponseWithDifferingQueryString) {
  GURL request_url("http://chromium_sub_frame.test?foo=bar");
  GURL response_url("http://chromium_sub_frame.test?something=else");
  SimulateSafeMainFrameLoad();

  // Execute ShouldAllowRequest() for a safe subframe navigation.
  auto sub_frame_request_decision =
      ShouldAllowRequestUrl(request_url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_request_decision.ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  // Verify that the sub frame navigation is allowed.
  web::WebStatePolicyDecider::PolicyDecision sub_frame_response_decision =
      ShouldAllowResponseUrl(response_url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_response_decision.ShouldAllowNavigation());
}

// Tests the case where multiple sub frames navigating to safe URLs are all
// allowed.
TEST_P(SafeBrowsingTabHelperTest, RepeatedSafeSubFrameResponses) {
  GURL url("http://chromium_sub_frame.test");
  SimulateSafeMainFrameLoad();

  // Execute ShouldAllowRequest() for a safe subframe navigation.
  auto sub_frame_request_decision =
      ShouldAllowRequestUrl(url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_request_decision.ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  // Verify that the sub frame navigation is allowed.
  web::WebStatePolicyDecider::PolicyDecision sub_frame_response_decision_1 =
      ShouldAllowResponseUrl(url, /*for_main_frame=*/false);
  web::WebStatePolicyDecider::PolicyDecision sub_frame_response_decision_2 =
      ShouldAllowResponseUrl(url, /*for_main_frame=*/false);
  web::WebStatePolicyDecider::PolicyDecision sub_frame_response_decision_3 =
      ShouldAllowResponseUrl(url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_response_decision_1.ShouldAllowNavigation());
  EXPECT_TRUE(sub_frame_response_decision_2.ShouldAllowNavigation());
  EXPECT_TRUE(sub_frame_response_decision_3.ShouldAllowNavigation());
}

// Tests that a subframe navigation to a previously-allowed safe URL is still
// allowed after a same-document navigation.
TEST_P(SafeBrowsingTabHelperTest,
       RepeatedSafeSubFrameResponseAfterSafeDocumentNavigation) {
  GURL url("http://chromium_sub_frame.test");
  SimulateSafeMainFrameLoad();

  // Execute ShouldAllowRequest() for a safe subframe navigation.
  auto sub_frame_request_decision =
      ShouldAllowRequestUrl(url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_request_decision.ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  // Verify that the sub frame navigation is allowed.
  web::WebStatePolicyDecider::PolicyDecision sub_frame_response_decision =
      ShouldAllowResponseUrl(url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_response_decision.ShouldAllowNavigation());

  // Simulate a same-document navigation.
  web::FakeNavigationContext context;
  context.SetHasCommitted(true);
  context.SetIsSameDocument(true);
  web_state_.OnNavigationFinished(&context);

  // Verify that the sub frame navigation is still allowed.
  web::WebStatePolicyDecider::PolicyDecision decision_after_navigation =
      ShouldAllowResponseUrl(url, /*for_main_frame=*/false);
  EXPECT_TRUE(decision_after_navigation.ShouldAllowNavigation());
}

// Tests that a subframe navigation request that arrives just after a main-frame
// navigation to restore session URL doesn't crash
TEST_P(SafeBrowsingTabHelperTest,
       SubframeRequestAfterRestoreSessionNavigation) {
  GURL url("http://chromium_sub_frame.test");
  SimulateSafeMainFrameLoad();

  // Simulate loading a restore session URL, which will cause the navigation
  // manager to return a null last committed item.
  navigation_manager_->SetLastCommittedItem(nullptr);

  // Verify that ShouldAllowNavigation() doesn't crash.
  web::WebStatePolicyDecider::PolicyDecision sub_frame_response_decision =
      ShouldAllowRequestUrl(url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_response_decision.ShouldAllowNavigation());

  // Wait until the Safe Browsing decision is received, ensuring that there's
  // no crash when this happens.
  base::RunLoop().RunUntilIdle();
}

// Tests that a subframe navigation query whose request arrives just before a
// main-frame navigation to a restore session URL and whose Safe Browsing
// decision arrives just after this navigation doesn't crash.
TEST_P(SafeBrowsingTabHelperTest, SubframeResultAfterRestoreSessionNavigation) {
  GURL url("http://chromium_sub_frame.test");
  SimulateSafeMainFrameLoad();

  web::WebStatePolicyDecider::PolicyDecision sub_frame_response_decision =
      ShouldAllowRequestUrl(url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_response_decision.ShouldAllowNavigation());

  // Simulate loading a restore session URL, which will cause the navigation
  // manager to return a null last committed item.
  navigation_manager_->SetLastCommittedItem(nullptr);

  // Wait until the Safe Browsing decision is received, ensuring that there's no
  // no crash when this happens.
  base::RunLoop().RunUntilIdle();
}

// Tests the case of a single sub frame navigation request and response, for a
// URL that is unsafe.
TEST_P(SafeBrowsingTabHelperTest, UnsafeSubFrameRequestAndResponse) {
  GURL url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  ASSERT_FALSE(navigation_manager_->ReloadWasCalled());
  web::NavigationItem* main_frame_item = SimulateSafeMainFrameLoad();

  // Execute ShouldAllowRequest() for an unsafe subframe navigation.
  auto sub_frame_request_decision =
      ShouldAllowRequestUrl(url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_request_decision.ShouldAllowNavigation());
  StoreUnsafeResource(url, /*is_main_frame*/ false);

  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    // If a sub frame navigation is deemed unsafe before its response policy
    // decision is requested, the last committed NavigationItem is immediately
    // reloaded.  This would cancel future sub frame loads, so
    // ShouldAllowResponse() is not expected to be executed in this case.
    base::RunLoop().RunUntilIdle();
  } else {
    // Verify that the sub frame navigation is not allowed.
    web::WebStatePolicyDecider::PolicyDecision sub_frame_response_decision =
        ShouldAllowResponseUrl(url, /*for_main_frame=*/false);
    EXPECT_FALSE(sub_frame_response_decision.ShouldAllowNavigation());
  }

  // The unsafe sub frame should trigger a reload.
  EXPECT_TRUE(navigation_manager_->ReloadWasCalled());

  // Simulate the main frame reload caused by the unsafe sub frame resource.
  navigation_manager_->SetPendingItem(main_frame_item);
  auto main_frame_reload_request_decision =
      ShouldAllowRequestUrl(main_frame_item->GetURL(), /*for_main_frame=*/true,
                            ui::PageTransition::PAGE_TRANSITION_RELOAD);
  EXPECT_TRUE(main_frame_reload_request_decision.ShouldAllowNavigation());

  // The URL check is skipped for safe browsing error pages caused by reloading
  // the main frame for unsafe sub frame resources, so there is no need to run
  // the runloop.
  auto main_frame_reload_response_decision =
      ShouldAllowResponseUrl(main_frame_item->GetURL());
  EXPECT_TRUE(main_frame_reload_response_decision.ShouldCancelNavigation());
  EXPECT_TRUE(main_frame_reload_response_decision.ShouldDisplayError());
  NSError* error = main_frame_reload_response_decision.GetDisplayError();
  EXPECT_NSEQ(kSafeBrowsingErrorDomain, error.domain);
  EXPECT_EQ(kUnsafeResourceErrorCode, error.code);
}

// Tests the case of a single unsafesafe sub frame navigation request and
// response, where the response URL has a different hash fragment than the
// request.
TEST_P(SafeBrowsingTabHelperTest,
       UnsafeSubFrameRequestAndResponseWithDifferingRef) {
  GURL request_url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  GURL response_url("http://" + FakeSafeBrowsingService::kUnsafeHost + "#ref");
  ASSERT_FALSE(navigation_manager_->ReloadWasCalled());
  web::NavigationItem* main_frame_item = SimulateSafeMainFrameLoad();

  // Execute ShouldAllowRequest() for an unsafe subframe navigation.
  auto sub_frame_request_decision =
      ShouldAllowRequestUrl(request_url, /*for_main_frame=*/false);
  EXPECT_TRUE(sub_frame_request_decision.ShouldAllowNavigation());
  StoreUnsafeResource(request_url, /*is_main_frame*/ false);

  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    // If a sub frame navigation is deemed unsafe before its response policy
    // decision is requested, the last committed NavigationItem is immediately
    // reloaded.  This would cancel future sub frame loads, so
    // ShouldAllowResponse() is not expected to be executed in this case.
    base::RunLoop().RunUntilIdle();
  } else {
    // Verify that the sub frame navigation is not allowed.
    web::WebStatePolicyDecider::PolicyDecision sub_frame_response_decision =
        ShouldAllowResponseUrl(response_url, /*for_main_frame=*/false);
    EXPECT_FALSE(sub_frame_response_decision.ShouldAllowNavigation());
  }

  // The unsafe sub frame should trigger a reload.
  EXPECT_TRUE(navigation_manager_->ReloadWasCalled());

  // Simulate the main frame reload caused by the unsafe sub frame resource.
  navigation_manager_->SetPendingItem(main_frame_item);
  auto main_frame_reload_request_decision =
      ShouldAllowRequestUrl(main_frame_item->GetURL(), /*for_main_frame=*/true,
                            ui::PageTransition::PAGE_TRANSITION_RELOAD);
  EXPECT_TRUE(main_frame_reload_request_decision.ShouldAllowNavigation());

  // The URL check is skipped for safe browsing error pages caused by reloading
  // the main frame for unsafe sub frame resources, so there is no need to run
  // the runloop.
  auto main_frame_reload_response_decision =
      ShouldAllowResponseUrl(main_frame_item->GetURL());
  EXPECT_TRUE(main_frame_reload_response_decision.ShouldCancelNavigation());
  EXPECT_TRUE(main_frame_reload_response_decision.ShouldDisplayError());
  NSError* error = main_frame_reload_response_decision.GetDisplayError();
  EXPECT_NSEQ(kSafeBrowsingErrorDomain, error.domain);
  EXPECT_EQ(kUnsafeResourceErrorCode, error.code);
}

// Tests the case of a subframe reload request that arrives when both the last
// committed item and pending items are null, which happens in practice during
// a back/forward navigation to a restore_session URL.
TEST_P(SafeBrowsingTabHelperTest, StaleIframeReload) {
  GURL unsafe_url("http://" + FakeSafeBrowsingService::kUnsafeHost);

  // Ensure that the navigation manager's state is consistent with what happens
  // during a back/forward navigation to a restore_session URL.
  ASSERT_FALSE(navigation_manager_->GetLastCommittedItem());
  ASSERT_FALSE(navigation_manager_->GetPendingItem());

  auto request_decision =
      ShouldAllowRequestUrl(unsafe_url, /*for_main_frame=*/false,
                            ui::PageTransition::PAGE_TRANSITION_RELOAD);
  EXPECT_TRUE(request_decision.ShouldAllowNavigation());

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  // Ensure that a subsequent main frame request is handled correctly.
  GURL safe_url("http://chromium.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(safe_url).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
  auto response_decision = ShouldAllowResponseUrl(safe_url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
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

  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

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
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
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
  StoreUnsafeResource(url1);
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
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
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  StoreUnsafeResource(url2);
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
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
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  StoreUnsafeResource(url3);
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
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
  StoreUnsafeResource(url1);
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  StoreUnsafeResource(url2);
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  StoreUnsafeResource(url3);
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
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
  StoreUnsafeResource(url1);
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  StoreUnsafeResource(url2);
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

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
  StoreUnsafeResource(url1);
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  StoreUnsafeResource(url2);
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
  SimulateMainFrameRedirect();

  // Interrupt the redirection chain with a brand new unrelated query. This
  // should be treated as safe.
  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(url3);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
}

// Tests the case of a redirection chain where a safe URL redirects to itself.
TEST_P(SafeBrowsingTabHelperTest, RedirectToSameSafeURL) {
  GURL url("http://chromium.test");
  EXPECT_TRUE(ShouldAllowRequestUrl(url).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

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
  StoreUnsafeResource(url);
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

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
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
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
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  StoreUnsafeResource(url2);
  base::RunLoop().RunUntilIdle();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url3).ShouldAllowNavigation());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
  SimulateMainFrameRedirect();

  EXPECT_TRUE(ShouldAllowRequestUrl(url2).ShouldAllowNavigation());
  StoreUnsafeResource(url2);
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();
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
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(ShouldAllowRequestUrl(url1).ShouldAllowNavigation());

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
  StoreUnsafeResource(unsafe_url);

  // When `unsafe_url` is determined to be unsafe, the client should be
  // notified.
  EXPECT_FALSE(client_.main_frame_cancellation_decided_called());
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

// Tests that client is notified when URL loaded in a subframe is unsafe.
TEST_P(SafeBrowsingTabHelperTest, UnsafeSubframeRequestNotifiesClient) {
  GURL unsafe_url("http://" + FakeSafeBrowsingService::kUnsafeHost);
  SimulateSafeMainFrameLoad();

  EXPECT_TRUE(ShouldAllowRequestUrl(unsafe_url, /*for_main_frame=*/false)
                  .ShouldAllowNavigation());

  StoreUnsafeResource(unsafe_url, /*is_main_frame*/ false);

  // When `unsafe_url` is determined to be unsafe, the client should be
  // notified.
  EXPECT_FALSE(client_.sub_frame_cancellation_decided_called());
  if (SafeBrowsingDecisionArrivesBeforeResponse()) {
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(client_.sub_frame_cancellation_decided_called());
  } else {
    web::WebStatePolicyDecider::PolicyDecision response_decision =
        ShouldAllowResponseUrl(unsafe_url, /*for_main_frame=*/false);
    EXPECT_TRUE(response_decision.ShouldCancelNavigation());
    EXPECT_TRUE(client_.sub_frame_cancellation_decided_called());
  }
}

// Tests that client is not notified when the main frame URL is safe.
TEST_P(SafeBrowsingTabHelperTest, SafeMainFrameRequestDoesNotNotifyClient) {
  GURL safe_url("http://chromium.test");

  EXPECT_TRUE(ShouldAllowRequestUrl(safe_url).ShouldAllowNavigation());

  EXPECT_FALSE(client_.main_frame_cancellation_decided_called());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(safe_url);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
  EXPECT_FALSE(client_.main_frame_cancellation_decided_called());
}

// Tests that client is not notified when a sub frame URL is safe.
TEST_P(SafeBrowsingTabHelperTest, SafeSubframeRequestDoesNotNotifyClient) {
  GURL safe_url("http://chromium.test");
  SimulateSafeMainFrameLoad();

  EXPECT_TRUE(ShouldAllowRequestUrl(safe_url, /*for_main_frame=*/false)
                  .ShouldAllowNavigation());

  EXPECT_FALSE(client_.sub_frame_cancellation_decided_called());
  if (SafeBrowsingDecisionArrivesBeforeResponse())
    base::RunLoop().RunUntilIdle();

  web::WebStatePolicyDecider::PolicyDecision response_decision =
      ShouldAllowResponseUrl(safe_url, /*for_main_frame=*/false);
  EXPECT_TRUE(response_decision.ShouldAllowNavigation());
  EXPECT_FALSE(client_.sub_frame_cancellation_decided_called());
}

INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName */,
    SafeBrowsingTabHelperTest,
    testing::Values(SafeBrowsingDecisionTiming::kBeforeResponse,
                    SafeBrowsingDecisionTiming::kAfterResponse));
