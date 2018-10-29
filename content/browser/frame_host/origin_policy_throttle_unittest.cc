// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/origin_policy_throttle.h"

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class OriginPolicyThrottleTest : public RenderViewHostTestHarness,
                                 public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    // Some tests below should be run with the feature en- and disabled, since
    // they test the feature functionality when enabled and feature
    // non-funcionality (that is, that the feature is inert) when disabled.
    // Hence, we run this test in both variants.
    features_.InitWithFeatureState(features::kOriginPolicy, GetParam());

    RenderViewHostTestHarness::SetUp();
    OriginPolicyThrottle::GetKnownVersionsForTesting().clear();
  }
  void TearDown() override {
    OriginPolicyThrottle::GetKnownVersionsForTesting().clear();
    nav_handle_.reset();
    RenderViewHostTestHarness::TearDown();
  }
  bool enabled() {
    return base::FeatureList::IsEnabled(features::kOriginPolicy);
  }

  void CreateHandleFor(const GURL& url) {
    net::HttpRequestHeaders headers;
    if (OriginPolicyThrottle::ShouldRequestOriginPolicy(url, nullptr))
      headers.SetHeader(net::HttpRequestHeaders::kSecOriginPolicy, "1");

    // Except for url and headers (which are determined by the test case)
    // all parameters below are cargo-culted from
    // NavigationHandleImplTest::CreateNavigationHandle.
    nav_handle_ = NavigationHandleImpl::Create(
        url,                  // url, as requested by caller
        std::vector<GURL>(),  // redirect chain
        static_cast<WebContentsImpl*>(web_contents())
            ->GetFrameTree()
            ->root(),            // render_tree_node
        true,                    // is_renderer_initialized
        false,                   // is_same_document
        base::TimeTicks::Now(),  // navigation_start
        0,                       // pending_nav_entry_id
        false,                   // started_from_context_menu
        CSPDisposition::CHECK,   // should_check_main_world_csp
        false,                   // is_form_submission
        nullptr,                 // navigation_ui_data
        "GET",                   // HTTP method
        headers);                // headers, as created above
  }

 protected:
  std::unique_ptr<NavigationHandleImpl> nav_handle_;
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_CASE_P(OriginPolicyThrottleTests,
                        OriginPolicyThrottleTest,
                        testing::Bool());

TEST_P(OriginPolicyThrottleTest, ShouldRequestOriginPolicy) {
  struct {
    const char* url;
    bool expect;
  } test_cases[] = {
      {"https://example.org/bla", true},
      {"http://example.org/bla", false},
      {"file:///etc/passwd", false},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(testing::Message() << "URL: " << test_case.url);
    EXPECT_EQ(enabled() && test_case.expect,
              OriginPolicyThrottle::ShouldRequestOriginPolicy(
                  GURL(test_case.url), nullptr));
  }
}

TEST_P(OriginPolicyThrottleTest, ShouldRequestLastKnownVersion) {
  if (!enabled())
    return;

  GURL url("https://example.org/bla");
  EXPECT_TRUE(OriginPolicyThrottle::ShouldRequestOriginPolicy(url, nullptr));

  std::string version;

  OriginPolicyThrottle::ShouldRequestOriginPolicy(url, &version);
  EXPECT_EQ(version, "1");

  OriginPolicyThrottle::GetKnownVersionsForTesting()[url::Origin::Create(url)] =
      "abcd";
  OriginPolicyThrottle::ShouldRequestOriginPolicy(url, &version);
  EXPECT_EQ(version, "abcd");
}

TEST_P(OriginPolicyThrottleTest, MaybeCreateThrottleFor) {
  CreateHandleFor(GURL("https://example.org/bla"));
  EXPECT_EQ(enabled(),
            !!OriginPolicyThrottle::MaybeCreateThrottleFor(nav_handle_.get()));

  CreateHandleFor(GURL("http://insecure.org/bla"));
  EXPECT_FALSE(
      !!OriginPolicyThrottle::MaybeCreateThrottleFor(nav_handle_.get()));
}

TEST_P(OriginPolicyThrottleTest, RunRequestEndToEnd) {
  if (!enabled())
    return;

  // Create a handle and start the request.
  CreateHandleFor(GURL("https://example.org/bla"));
  EXPECT_EQ(NavigationThrottle::PROCEED,
            nav_handle_->CallWillStartRequestForTesting().action());

  // Fake a response with a policy header. Check whether the navigation
  // is deferred.
  const char* headers = "HTTP/1.1 200 OK\nSec-Origin-Policy: policy-1\n\n";
  EXPECT_EQ(NavigationThrottle::DEFER,
            nav_handle_
                ->CallWillProcessResponseForTesting(
                    main_rfh(),
                    net::HttpUtil::AssembleRawHeaders(headers, strlen(headers)),
                    false, net::ProxyServer::Direct())
                .action());
  EXPECT_EQ(NavigationHandleImpl::DEFERRING_RESPONSE,
            nav_handle_->state_for_testing());

  // For the purpose of this unit test we don't care about policy content,
  // only that it's non-empty. We check whether the throttle will pass it on.
  const char* policy = "{}";
  static_cast<OriginPolicyThrottle*>(
      nav_handle_->GetDeferringThrottleForTesting())
      ->InjectPolicyForTesting(policy);

  // At the end of the navigation, the navigation handle should have a copy
  // of the origin policy.
  EXPECT_EQ(policy, nav_handle_->origin_policy());
}

}  // namespace content
