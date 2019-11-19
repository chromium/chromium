// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/origin_policy_throttle.h"

#include <set>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/origin_policy.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
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
  }
  void TearDown() override {
    nav_handle_.reset();
    RenderViewHostTestHarness::TearDown();
  }
  bool enabled() {
    return base::FeatureList::IsEnabled(features::kOriginPolicy);
  }

  void CreateHandleFor(const GURL& url) {
    net::HttpRequestHeaders headers;
    if (OriginPolicyThrottle::ShouldRequestOriginPolicy(url))
      headers.SetHeader(net::HttpRequestHeaders::kSecOriginPolicy, "0");

    nav_handle_ = std::make_unique<MockNavigationHandle>(web_contents());
    nav_handle_->set_url(url);
    nav_handle_->set_request_headers(headers);
  }

 protected:
  std::unique_ptr<MockNavigationHandle> nav_handle_;
  base::test::ScopedFeatureList features_;
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
  } test_cases[] = {
      {network::OriginPolicyState::kLoaded,
       NavigationThrottle::ThrottleAction::PROCEED},
      {network::OriginPolicyState::kNoPolicyApplies,
       NavigationThrottle::ThrottleAction::PROCEED},
      {network::OriginPolicyState::kInvalidRedirect,
       NavigationThrottle::ThrottleAction::CANCEL},
      {network::OriginPolicyState::kCannotLoadPolicy,
       NavigationThrottle::ThrottleAction::CANCEL},
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
    OriginPolicyThrottle::ResetOriginPolicyForTesting();
  }
}

}  // namespace content
