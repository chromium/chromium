// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_navigation_throttle.h"

#include <ostream>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "content/browser/webid/test/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/test/webid_test_content_browser_client.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

constexpr char kOauthRequestParams[] =
    "?client_id=12345&scope=67890&"
    "redirect_uri=https%3A%2F%2Frp.example%2F";

struct CrossSiteTestCase {
  std::string test_name;
  std::string idp_origin;
  std::string rp_origin;
  NavigationThrottle::ThrottleAction expected_action;
};

std::ostream& operator<<(std::ostream& os, const CrossSiteTestCase& testcase) {
  std::string name;
  base::ReplaceChars(testcase.test_name, " ", "", &name);
  return os << name;
}

static const CrossSiteTestCase kCrossSiteTests[]{
    {"RP is subdomain of registered IdP domain", "https://idp.example/",
     "https://subdomain.idp.example", NavigationThrottle::PROCEED},
    {"IdP is subdomain of registered RP domain",
     "https://subdomain.idp.example/", "https://idp.example",
     NavigationThrottle::PROCEED},
    {"Sibling subdomains of registered domain", "https://a.idp.example/",
     "https://b.idp.example", NavigationThrottle::PROCEED},
    {"Subdomain of listed private eTLD", "https://x.github.io/",
     "https://y.github.io/", NavigationThrottle::DEFER},
    {"Subdomain of listed public eTLD", "https://example1.co.uk/",
     "https://example2.co.uk/", NavigationThrottle::DEFER},
    {"Same domain with different port number", "https://idp.example:1234/",
     "https://idp.example", NavigationThrottle::PROCEED},
    // This result should change when first-party sets are accommodated.
    {"Same first party set", "https://accounts.google.com/",
     "https://youtube.com", NavigationThrottle::DEFER},
    {"Same domain with different scheme", "http://idp.example/",
     "https://idp.example", NavigationThrottle::DEFER},
};

}  // namespace

class FederatedAuthNavigationThrottleTest : public RenderViewHostTestHarness {
 public:
  FederatedAuthNavigationThrottleTest() = default;
  ~FederatedAuthNavigationThrottleTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    test_browser_client_ = std::make_unique<WebIdTestContentBrowserClient>();
    auto controller = std::make_unique<FakeIdentityRequestDialogController>(
        absl::nullopt, absl::nullopt, "id_token");
    test_browser_client_->SetIdentityRequestDialogController(
        std::move(controller));
    old_client_ = SetBrowserClientForTesting(test_browser_client_.get());
  }

  void TearDown() override {
    CHECK_EQ(SetBrowserClientForTesting(old_client_),
             test_browser_client_.get());
    RenderViewHostTestHarness::TearDown();
  }

 private:
  std::unique_ptr<WebIdTestContentBrowserClient> test_browser_client_;
  ContentBrowserClient* old_client_ = nullptr;
};

// Test that the throttle is only created when the WebID feature is turned on
// and when the frame is a main frame.
TEST_F(FederatedAuthNavigationThrottleTest, Instantiate) {
  GURL url("https://idp.example");
  GURL url_child("https://child.example");

  content::RenderFrameHostTester::For(main_rfh())
      ->InitializeRenderFrameIfNeeded();
  RenderFrameHost* child_rfh =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("child");

  MockNavigationHandle top_frame_handle(url, main_rfh());
  MockNavigationHandle child_frame_handle(url_child, child_rfh);

  // Attempt to create throttle for the main frame without features::kWebID set.
  auto throttle = FederatedAuthNavigationThrottle::MaybeCreateThrottleFor(
      &top_frame_handle);
  ASSERT_FALSE(throttle);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kWebID);

  // Attempt to create throttle for a child frame with features::kWebID set.
  throttle = FederatedAuthNavigationThrottle::MaybeCreateThrottleFor(
      &child_frame_handle);
  ASSERT_FALSE(throttle);

  // Attempt to create throttle for the main frame with features::kWebID set.
  throttle = FederatedAuthNavigationThrottle::MaybeCreateThrottleFor(
      &top_frame_handle);
  ASSERT_TRUE(throttle);
}

// Verify an OAuth request is throttled.
TEST_F(FederatedAuthNavigationThrottleTest, ThrottleAuthRequest) {
  GURL idp_url(
      "https://idp.example/?client_id=12345&scope=67890&"
      "redirect_uri=https%3A%2F%2Frp.example%2F");

  MockNavigationHandle handle(idp_url, main_rfh());
  handle.set_initiator_origin(url::Origin::Create(GURL("https://rp.example")));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kWebID);

  auto throttle =
      FederatedAuthNavigationThrottle::MaybeCreateThrottleFor(&handle);
  ASSERT_TRUE(throttle);

  EXPECT_EQ(NavigationThrottle::DEFER, throttle->WillStartRequest().action());
}

class CrossSiteFederatedAuthNavigationThrottleTest
    : public FederatedAuthNavigationThrottleTest,
      public ::testing::WithParamInterface<CrossSiteTestCase> {};

INSTANTIATE_TEST_SUITE_P(CrossSiteThrottlingTests,
                         CrossSiteFederatedAuthNavigationThrottleTest,
                         ::testing::ValuesIn(kCrossSiteTests),
                         ::testing::PrintToStringParamName());

// Verify same-site OAuth requests are not throttled.
TEST_P(CrossSiteFederatedAuthNavigationThrottleTest, SameSiteAuthRequest) {
  CrossSiteTestCase test_case = GetParam();
  GURL idp_url(test_case.idp_origin + kOauthRequestParams);

  MockNavigationHandle handle(idp_url, main_rfh());
  handle.set_initiator_origin(url::Origin::Create(GURL(test_case.rp_origin)));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kWebID);

  auto throttle =
      FederatedAuthNavigationThrottle::MaybeCreateThrottleFor(&handle);
  ASSERT_TRUE(throttle);

  EXPECT_EQ(test_case.expected_action, throttle->WillStartRequest().action());
}

}  // namespace content
