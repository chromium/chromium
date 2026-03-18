// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "url/gurl.h"

namespace content {
namespace {

constexpr char kConnectionAllowlistEnabledUrl[] = "https://example.com";

// Generated with:
// tools/origin_trials/generate_token.py https://example.com ConnectionAllowlist
// --expire-timestamp=2000000000
constexpr char kConnectionAllowlistToken[] =
    "Aw0A43iyvh+gC6PTUbyaU60uRUWTiurCnn5M8mfk7Or3q/"
    "AShKKs3BqZMi+yswF+"
    "AMzO7CG1nhpKOJyzMKIqEwYAAABdeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0ND"
    "MiLCAiZmVhdHVyZSI6ICJDb25uZWN0aW9uQWxsb3dsaXN0IiwgImV4cGlyeSI6IDIwMDAwMDAw"
    "MDB9";

constexpr char kAnotherConnectionAllowlistEnabledUrl[] =
    "https://another-ot-enabled.com";

// tools/origin_trials/generate_token.py https://another-ot-enabled.com
// ConnectionAllowlist --expire-timestamp=2000000000
constexpr char kAnotherConnectionAllowlistToken[] =
    "A1DWodz3Jbi7pbgHaF8+1KP73Y67LkKia4c8kdnM65wKCv37zAffQYhVy/"
    "Kxd0rvTmAd+"
    "wWkcPeyITHDjZzO3QcAAABoeyJvcmlnaW4iOiAiaHR0cHM6Ly9hbm90aGVyLW90LWVuYWJsZWQ"
    "uY29tOjQ0MyIsICJmZWF0dXJlIjogIkNvbm5lY3Rpb25BbGxvd2xpc3QiLCAiZXhwaXJ5IjogM"
    "jAwMDAwMDAwMH0=";

}  // namespace

// Tests for exercising connection allowlist's origin trial behaviors. It is
// different from the normal origin trial behavior for most web platform
// features.
class ConnectionAllowlistOriginTrialTest
    : public RenderViewHostImplTestHarness {
 public:
  ConnectionAllowlistOriginTrialTest() = default;
  ~ConnectionAllowlistOriginTrialTest() override = default;

  const PolicyContainerPolicies& GetPolicyContainerPolicies(
      const RenderFrameHost* rfh) const {
    return static_cast<const RenderFrameHostImpl*>(rfh)
        ->policy_container_host()
        ->policies();
  }

  // Only if the document has trial enabled, then it can have a connection
  // allowlist in its policy container.
  bool HasConnectionAllowlist(const RenderFrameHost* rfh) const {
    return GetPolicyContainerPolicies(rfh)
        .connection_allowlists.enforced.has_value();
  }

 private:
  blink::ScopedTestOriginTrialPolicy scoped_test_origin_trial_policy_;
};

// Testers should be able to test connection allowlist by overriding the origin
// trial requirements locally by enabling
// `blink::features::kOverrideConnectionAllowlistOriginTrial`.
TEST_F(ConnectionAllowlistOriginTrialTest, OriginTrialOverride) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{network::features::kConnectionAllowlists,
                            blink::features::
                                kOverrideConnectionAllowlistOriginTrial},
      /*disabled_features=*/{});

  const GURL url = GURL("https://no-origin-trial.com");
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Connection-Allowlist", "(response-origin)");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  EXPECT_TRUE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));
}

// Even though the trial is enabled, the base::Feature as a kill switch should
// disable the connection allowlist.
TEST_F(ConnectionAllowlistOriginTrialTest, BaseFeatureKillSwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(network::features::kConnectionAllowlists);

  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kConnectionAllowlistEnabledUrl), main_rfh());

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Origin-Trial", kConnectionAllowlistToken);
  response_headers->SetHeader("Connection-Allowlist", "(response-origin)");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  EXPECT_FALSE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));
}

// No trial token in the response, the connection allowlist is disabled. The
// allowlist in the response is not stored to the policy container.
TEST_F(ConnectionAllowlistOriginTrialTest, NoTokenTrialDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  const GURL url = GURL("https://no-origin-trial.com");
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(url, main_rfh());
  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Connection-Allowlist", "(response-origin)");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  EXPECT_FALSE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));
}

// Invalid trial token cannot enable connection allowlist.
TEST_F(ConnectionAllowlistOriginTrialTest, InvalidTokenTrialDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kConnectionAllowlistEnabledUrl), main_rfh());

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Origin-Trial", "Invalid");
  response_headers->SetHeader("Connection-Allowlist", "(response-origin)");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  EXPECT_FALSE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));
}

// Valid trial token enables connection allowlist.
TEST_F(ConnectionAllowlistOriginTrialTest, ValidTokenTrialEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kConnectionAllowlistEnabledUrl), main_rfh());

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Origin-Trial", kConnectionAllowlistToken);
  response_headers->SetHeader("Connection-Allowlist", "(response-origin)");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  EXPECT_TRUE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));
}

// Response contains a valid trial token but the "Connection-Allowlist" header
// field is an empty string. The trial is not enabled.
TEST_F(ConnectionAllowlistOriginTrialTest,
       ValidTokenAllowlistEmptyTrialDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kConnectionAllowlistEnabledUrl), main_rfh());

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Origin-Trial", kConnectionAllowlistToken);
  // Note the header field value is an empty string. This is different from "()"
  // which represents a valid allowlist that disallows all network requests.
  response_headers->SetHeader("Connection-Allowlist", "");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  EXPECT_FALSE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));
}

// Response contains a valid trial token but the "Connection-Allowlist" header
// is missing. Only the "Connection-Allowlist-Report-Only" is present. The trial
// is not enabled.
TEST_F(ConnectionAllowlistOriginTrialTest,
       ValidTokenWithReportOnlyHeaderTrialDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kConnectionAllowlistEnabledUrl), main_rfh());

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Origin-Trial", kConnectionAllowlistToken);
  response_headers->SetHeader("Connection-Allowlist-Report-Only",
                              "(response-origin)");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  EXPECT_FALSE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));
}

// When there is a copy of the policy container, the connection allowlist stored
// in the policy container is also copied unconditionally. This is expected
// behavior for connection Allowlists to check that creating an empty or local
// scheme iframe should not be used as a workaround to the network restrictions
// of the parent frame.
TEST_F(ConnectionAllowlistOriginTrialTest,
       TrialEnabledMainFrameCreatesEmptyIframeAlsoTrialEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kConnectionAllowlistEnabledUrl), main_rfh());

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Origin-Trial", kConnectionAllowlistToken);
  response_headers->SetHeader("Connection-Allowlist", "(response-origin)");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  EXPECT_TRUE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));

  // The main frame creates an iframe.
  RenderFrameHost* main_frame = navigation->GetFinalRenderFrameHost();
  RenderFrameHostTester* main_frame_tester =
      RenderFrameHostTester::For(main_frame);
  RenderFrameHost* child_rfh = main_frame_tester->AppendChild("child");

  // Verify the iframe enables the connection allowlist. Note this behavior is
  // different from most other origin trial features because the connection
  // allowlist trial is only checked when the allowlist header is written to the
  // policy container. Once written, the allowlist is free to propagate together
  // with the policy container.
  EXPECT_TRUE(HasConnectionAllowlist(child_rfh));
}

// An empty iframe navigates and receives a response with valid trial token. The
// connection allowlist is enabled for the iframe.
TEST_F(ConnectionAllowlistOriginTrialTest, IframeNavigationEnablesTrial) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  // 1. The main frame navigates to a page which receives a response that
  // does not contain the origin trial token.
  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kConnectionAllowlistEnabledUrl), main_rfh());
  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Connection-Allowlist", "(response-origin)");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  // 2. Verify the main frame has connection allowlist disabled.
  EXPECT_FALSE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));

  // 3. The main frame creates an iframe.
  RenderFrameHost* main_frame = navigation->GetFinalRenderFrameHost();
  RenderFrameHostTester* main_frame_tester =
      RenderFrameHostTester::For(main_frame);
  RenderFrameHost* child_rfh = main_frame_tester->AppendChild("child");

  // 4. Verify the iframe has connection allowlist disabled.
  EXPECT_FALSE(HasConnectionAllowlist(child_rfh));

  // 5. Navigate the iframe, the response has the origin trial token.
  auto child_navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kConnectionAllowlistEnabledUrl), child_rfh);
  auto child_response_header =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  child_response_header->SetHeader("Origin-Trial", kConnectionAllowlistToken);
  child_response_header->SetHeader("Connection-Allowlist", "(response-origin)");
  child_navigation->SetResponseHeaders(child_response_header);
  child_navigation->SetInitiatorFrame(main_frame);
  child_navigation->Commit();

  // 6. Verify the iframe has connection allowlist enabled.
  EXPECT_TRUE(
      HasConnectionAllowlist(child_navigation->GetFinalRenderFrameHost()));
}

// A trial enabled iframe navigates and does not receive trial token, the
// connection allowlist is disabled for the iframe.
TEST_F(ConnectionAllowlistOriginTrialTest,
       IframeNavigateNoTokenReceivedTrialDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  // 1. The main frame navigates to a page which receives a response that
  // contains the origin trial token.
  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kConnectionAllowlistEnabledUrl), main_rfh());

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Origin-Trial", kConnectionAllowlistToken);
  response_headers->SetHeader(
      "Connection-Allowlist",
      R"((response-origin "https://no-origin-trial.com"))");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  // 2. Verify the main frame has connection allowlist enabled.
  EXPECT_TRUE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));

  // 3. The main frame creates an iframe.
  RenderFrameHost* main_frame = navigation->GetFinalRenderFrameHost();
  RenderFrameHostTester* main_frame_tester =
      RenderFrameHostTester::For(main_frame);
  RenderFrameHost* child_rfh = main_frame_tester->AppendChild("child");

  // 4. Verify the iframe has connection allowlist enabled. Note this behavior
  // is different from most other origin trial features.
  EXPECT_TRUE(HasConnectionAllowlist(child_rfh));

  // 5. Navigate the iframe, the response is without the origin trial token.
  auto child_navigation = NavigationSimulator::CreateRendererInitiated(
      GURL("https://no-origin-trial.com"), child_rfh);
  auto child_response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  child_response_headers->SetHeader("Connection-Allowlist",
                                    "(response-origin)");
  child_navigation->SetResponseHeaders(child_response_headers);
  child_navigation->SetInitiatorFrame(main_frame);
  child_navigation->Commit();

  // 6. Verify the iframe has connection allowlist disabled.
  EXPECT_FALSE(
      HasConnectionAllowlist(child_navigation->GetFinalRenderFrameHost()));
}

// Main frame has trial enabled. The iframe navigates and receives a different
// trial token, the connection allowlist is enabled for the iframe.
TEST_F(ConnectionAllowlistOriginTrialTest,
       IframeNavigateDifferentTokenReceivedTrialEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  // 1. The main frame navigates to a page which receives a response that
  // contains the origin trial token.
  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kConnectionAllowlistEnabledUrl), main_rfh());

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Origin-Trial", kConnectionAllowlistToken);
  response_headers->SetHeader(
      "Connection-Allowlist",
      R"((response-origin "https://another-ot-enabled.com"))");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  // 2. Verify the main frame has connection allowlist enabled.
  EXPECT_TRUE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));

  // 3. The main frame creates an iframe.
  RenderFrameHost* main_frame = navigation->GetFinalRenderFrameHost();
  RenderFrameHostTester* main_frame_tester =
      RenderFrameHostTester::For(main_frame);
  RenderFrameHost* child_rfh = main_frame_tester->AppendChild("child");

  // 4. Verify the iframe has connection allowlist enabled. Note this behavior
  // is different from most other origin trial features.
  EXPECT_TRUE(HasConnectionAllowlist(child_rfh));

  // 5. Navigate the iframe, the response has a different origin trial token.
  auto child_navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kAnotherConnectionAllowlistEnabledUrl), child_rfh);
  child_navigation->SetInitiatorFrame(main_frame);
  auto child_response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  child_response_headers->SetHeader("Origin-Trial",
                                    kAnotherConnectionAllowlistToken);
  child_response_headers->SetHeader("Connection-Allowlist",
                                    "(response-origin)");
  child_navigation->SetResponseHeaders(child_response_headers);
  child_navigation->Commit();

  // 6. Verify the iframe has connection allowlist enabled.
  EXPECT_TRUE(
      HasConnectionAllowlist(child_navigation->GetFinalRenderFrameHost()));
}

}  // namespace content
