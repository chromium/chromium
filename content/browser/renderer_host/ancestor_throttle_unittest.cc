// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/ancestor_throttle.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_navigation_url_loader.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class AncestorThrottleNavigationTest : public RenderViewHostTestHarness {
 public:
  AncestorThrottleNavigationTest() {
    scoped_feature_list.InitAndEnableFeature(features::kEmbeddingRequiresOptIn);
  }
  ~AncestorThrottleNavigationTest() override = default;

  AncestorThrottleNavigationTest(const AncestorThrottleNavigationTest& other) =
      delete;
  AncestorThrottleNavigationTest& operator=(
      const AncestorThrottleNavigationTest& other) = delete;

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

TEST_F(AncestorThrottleNavigationTest, EmbeddingOptInRequirement) {
  // Set up the main frame. We'll add nested frames to it in the tests below.
  NavigateAndCommit(GURL("https://www.example.org"));
  auto* main_frame = static_cast<TestRenderFrameHost*>(main_rfh());

  struct TestCase {
    const char* name;
    const char* frame_url;
    const char* xfo;
    const char* csp;
    NavigationThrottle::ThrottleAction expected_result;
  } cases[] = {{
                   "Same-origin, no XFO, no CSP",
                   "https://www.example.org",
                   nullptr,
                   nullptr,
                   NavigationThrottle::PROCEED,
               },
               {
                   "Same-site, no XFO, no CSP",
                   "https://not.example.org",
                   nullptr,
                   nullptr,
                   NavigationThrottle::BLOCK_RESPONSE,
               },
               {
                   "Same-site, XFO: ALLOWALL, no CSP",
                   "https://not.example.org",
                   "ALLOWALL",
                   nullptr,
                   NavigationThrottle::PROCEED,
               },
               {
                   "Same-site, XFO: INVALID, no CSP",
                   "https://not.example.org",
                   "INVALID",
                   nullptr,
                   NavigationThrottle::PROCEED,
               },
               {
                   "Same-site, no XFO, CSP: frame-ancestors *",
                   "https://not.example.org",
                   nullptr,
                   "frame-ancestors *",
                   NavigationThrottle::PROCEED,
               },
               {
                   "Same-site, no XFO, CSP without frame-ancestors",
                   "https://not.example.org",
                   nullptr,
                   "img-src 'self'",
                   NavigationThrottle::BLOCK_RESPONSE,
               },
               {
                   "Cross-origin, no XFO, no CSP",
                   "https://www.not-example.org",
                   nullptr,
                   nullptr,
                   NavigationThrottle::BLOCK_RESPONSE,
               },
               {
                   "Cross-origin, XFO: ALLOWALL, no CSP",
                   "https://www.not-example.org",
                   "ALLOWALL",
                   nullptr,
                   NavigationThrottle::PROCEED,
               },
               {
                   "Cross-origin, XFO: INVALID, no CSP",
                   "https://www.not-example.org",
                   "INVALID",
                   nullptr,
                   NavigationThrottle::PROCEED,
               },
               {
                   "Cross-origin, no XFO, CSP: frame-ancestors *",
                   "https://www.not-example.org",
                   nullptr,
                   "frame-ancestors *",
                   NavigationThrottle::PROCEED,
               },
               {
                   "Cross-origin, no XFO, CSP without frame-ancestors",
                   "https://www.not-example.org",
                   nullptr,
                   "img-src 'self'",
                   NavigationThrottle::BLOCK_RESPONSE,
               }};

  for (auto test : cases) {
    SCOPED_TRACE(test.name);
    auto* frame = static_cast<TestRenderFrameHost*>(
        content::RenderFrameHostTester::For(main_frame)
            ->AppendChild(test.name));
    std::unique_ptr<NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(
            GURL(test.frame_url), frame);

    auto response_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    if (test.xfo)
      response_headers->SetHeader("X-Frame-Options", test.xfo);
    if (test.csp)
      response_headers->SetHeader("Content-Security-Policy", test.csp);

    simulator->SetResponseHeaders(response_headers);
    simulator->ReadyToCommit();

    EXPECT_EQ(test.expected_result, simulator->GetLastThrottleCheckResult());
  }
}

}  // namespace content
