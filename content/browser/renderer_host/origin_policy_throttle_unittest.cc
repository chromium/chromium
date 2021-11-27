// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/origin_policy_throttle.h"

#include <set>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_render_frame_host.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/origin_policy.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kInterstitialContent[] = "error page content";

class OriginPolicyErrorPageContentBrowserClient : public ContentBrowserClient {
 public:
  absl::optional<std::string> GetOriginPolicyErrorPage(
      network::OriginPolicyState error_reason,
      content::NavigationHandle* navigation_handle) override {
    return kInterstitialContent;
  }
};

}  // namespace

class OriginPolicyThrottleTest : public RenderViewHostTestHarness,
                                 public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    // Some tests below should be run with the feature en- and disabled, since
    // they test the feature functionality when enabled and feature
    // non-funcionality (that is, that the feature is inert) when disabled.
    // Hence, we run this test in both variants.
    features_.InitWithFeatureState(features::kOriginPolicy, GetParam());

    old_client_ = SetBrowserClientForTesting(&test_client_);

    RenderViewHostTestHarness::SetUp();
  }
  void TearDown() override {
    nav_handle_.reset();
    SetBrowserClientForTesting(old_client_);
    RenderViewHostTestHarness::TearDown();
  }
  bool enabled() {
    return base::FeatureList::IsEnabled(features::kOriginPolicy);
  }

  void CreateHandleFor(const GURL& url) {
    nav_handle_ = std::make_unique<MockNavigationHandle>(web_contents());
    nav_handle_->set_url(url);
  }

 protected:
  std::unique_ptr<MockNavigationHandle> nav_handle_;
  base::test::ScopedFeatureList features_;
  OriginPolicyErrorPageContentBrowserClient test_client_;
  raw_ptr<ContentBrowserClient> old_client_;
};

INSTANTIATE_TEST_SUITE_P(OriginPolicyThrottleTests,
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
    EXPECT_EQ(
        enabled() && test_case.expect,
        OriginPolicyThrottle::ShouldRequestOriginPolicy(GURL(test_case.url)));
  }
}

TEST_P(OriginPolicyThrottleTest, MaybeCreateThrottleFor) {
  CreateHandleFor(GURL("https://example.org/bla"));
  EXPECT_EQ(enabled(),
            !!OriginPolicyThrottle::MaybeCreateThrottleFor(nav_handle_.get()));

  CreateHandleFor(GURL("http://insecure.org/bla"));
  EXPECT_FALSE(
      !!OriginPolicyThrottle::MaybeCreateThrottleFor(nav_handle_.get()));
}

TEST_P(OriginPolicyThrottleTest, WillProcessResponse) {
  if (!enabled())
    return;

  struct {
    // The state of the origin policy returned in the response.
    network::OriginPolicyState state;
    // The expected navigation thottle action.
    NavigationThrottle::ThrottleAction expected_action;
    // The expected network error.
    net::Error expected_net_error_code;
    // The expected custom error message.
    absl::optional<std::string> expected_error_page_content;
  } test_cases[] = {
      {
          network::OriginPolicyState::kLoaded,
          NavigationThrottle::ThrottleAction::PROCEED,
          net::OK,
          absl::nullopt,
      },
      {
          network::OriginPolicyState::kNoPolicyApplies,
          NavigationThrottle::ThrottleAction::PROCEED,
          net::OK,
          absl::nullopt,
      },
      {
          network::OriginPolicyState::kCannotLoadPolicy,
          NavigationThrottle::ThrottleAction::CANCEL,
          net::ERR_BLOCKED_BY_CLIENT,
          kInterstitialContent,
      },
      {
          network::OriginPolicyState::kCannotParseHeader,
          NavigationThrottle::ThrottleAction::CANCEL,
          net::ERR_BLOCKED_BY_CLIENT,
          kInterstitialContent,
      },
  };

  for (const auto& test : test_cases) {
    network::OriginPolicy result;
    result.state = test.state;

    OriginPolicyThrottle::SetOriginPolicyForTesting(result);

    auto navigation = NavigationSimulator::CreateBrowserInitiated(
        GURL("https://example.org/bla"), web_contents());

    navigation->Start();
    navigation->ReadyToCommit();

    EXPECT_EQ(test.expected_action,
              navigation->GetLastThrottleCheckResult().action());
    EXPECT_EQ(test.expected_net_error_code,
              navigation->GetLastThrottleCheckResult().net_error_code());
    EXPECT_EQ(test.expected_error_page_content,
              navigation->GetLastThrottleCheckResult().error_page_content());
    OriginPolicyThrottle::ResetOriginPolicyForTesting();
  }
}

TEST_P(OriginPolicyThrottleTest, Iframe) {
  if (!enabled())
    return;

  NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://example.org/main"));
  RenderFrameHost* iframe_rfh = static_cast<TestRenderFrameHost*>(main_rfh())
                                    ->AppendChild("child_iframe");

  network::OriginPolicy result;
  result.state = network::OriginPolicyState::kCannotLoadPolicy;
  OriginPolicyThrottle::SetOriginPolicyForTesting(result);

  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL("https://example.org/iframe"), iframe_rfh);
  navigation->Start();
  navigation->ReadyToCommit();

  EXPECT_EQ(NavigationThrottle::ThrottleAction::CANCEL,
            navigation->GetLastThrottleCheckResult().action());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT,
            navigation->GetLastThrottleCheckResult().net_error_code());
  EXPECT_FALSE(navigation->GetLastThrottleCheckResult()
                   .error_page_content()
                   .has_value());
  OriginPolicyThrottle::ResetOriginPolicyForTesting();
}

}  // namespace content
