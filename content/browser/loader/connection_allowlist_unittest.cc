// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/connection_allowlist_utils.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/scoped_test_origin_trial_policy.h"
#include "url/gurl.h"

namespace content {
namespace {

constexpr char kConnectionAllowlistEnabledUrl[] = "https://example.com";
}  // namespace

class ConnectionAllowlistTest : public RenderViewHostImplTestHarness {
 public:
  ConnectionAllowlistTest() = default;
  ~ConnectionAllowlistTest() override = default;

  const PolicyContainerPolicies& GetPolicyContainerPolicies(
      const RenderFrameHost* rfh) const {
    return static_cast<const RenderFrameHostImpl*>(rfh)
        ->policy_container_host()
        ->policies();
  }

  bool HasConnectionAllowlist(const RenderFrameHost* rfh) const {
    const PolicyContainerPolicies& policies = GetPolicyContainerPolicies(rfh);
    return policies.connection_allowlists.enforced.has_value() ||
           policies.connection_allowlists.report_only.has_value();
  }

 private:
  blink::ScopedTestOriginTrialPolicy scoped_test_origin_trial_policy_;
};

// The base::Feature as a kill switch should disable the connection allowlist.
TEST_F(ConnectionAllowlistTest, BaseFeatureKillSwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(network::features::kConnectionAllowlists);

  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kConnectionAllowlistEnabledUrl), main_rfh());

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Connection-Allowlist", "(response-origin)");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  EXPECT_FALSE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));
}

// When there is a copy of the policy container, the connection allowlist stored
// in the policy container is also copied unconditionally. This is expected
// behavior for connection Allowlists to check that creating an empty or local
// scheme iframe should not be used as a workaround to the network restrictions
// of the parent frame.
TEST_F(ConnectionAllowlistTest, MainFrameCreatesEmptyIframeInheritsAllowlist) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  auto navigation = NavigationSimulator::CreateRendererInitiated(
      GURL(kConnectionAllowlistEnabledUrl), main_rfh());

  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  response_headers->SetHeader("Connection-Allowlist", "(response-origin)");
  navigation->SetResponseHeaders(response_headers);
  navigation->Commit();

  EXPECT_TRUE(HasConnectionAllowlist(navigation->GetFinalRenderFrameHost()));

  // The main frame creates an iframe.
  RenderFrameHost* main_frame = navigation->GetFinalRenderFrameHost();
  RenderFrameHostTester* main_frame_tester =
      RenderFrameHostTester::For(main_frame);
  RenderFrameHost* child_rfh = main_frame_tester->AppendChild("child");

  // Verify the iframe enables the connection allowlist.
  EXPECT_TRUE(HasConnectionAllowlist(child_rfh));
}

class ConnectionAllowlistTestNetworkContext
    : public network::TestNetworkContext {
 public:
  ConnectionAllowlistTestNetworkContext() = default;

  struct Report {
    std::string type;
    std::string group;
    GURL url;
    std::optional<base::UnguessableToken> reporting_source;
    net::NetworkAnonymizationKey network_anonymization_key;
    base::DictValue body;
  };

  const std::vector<Report>& reports() const { return reports_; }

  void QueueReport(
      const std::string& type,
      const std::string& group,
      const GURL& url,
      const std::optional<base::UnguessableToken>& reporting_source,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      base::DictValue body) override {
    reports_.push_back({type, group, url, reporting_source,
                        network_anonymization_key, std::move(body)});
  }

 private:
  std::vector<Report> reports_;
};

TEST(ConnectionAllowlistReportingTest, ReportsViolations) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(network::features::kConnectionAllowlists);

  ConnectionAllowlistTestNetworkContext network_context;
  net::NetworkAnonymizationKey network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(
          net::SchemefulSite(GURL("https://example.com")));
  base::UnguessableToken reporting_source = base::UnguessableToken::Create();

  PolicyContainerPolicies policies;
  policies.connection_allowlists.response_url =
      GURL("https://example.com/page");
  policies.connection_allowlists.reporting_source = reporting_source;

  network::ConnectionAllowlist enforced;
  enforced.allowlist = {"https://allowed.com"};
  enforced.reporting_endpoint = "endpoint-group";
  enforced.redirect_behavior =
      network::ConnectionAllowlist::RedirectBehavior::kBlock;
  policies.connection_allowlists.enforced = enforced;

  // Test IsRedirectAllowedByConnectionAllowlist
  GURL original_url("https://blocked.com/redirect");
  EXPECT_FALSE(IsRedirectAllowedByConnectionAllowlist(
      policies, original_url, &network_context, network_anonymization_key,
      reporting_source));

  ASSERT_EQ(network_context.reports().size(), 1u);
  EXPECT_EQ(network_context.reports()[0].type, "connection-allowlist");
  EXPECT_EQ(network_context.reports()[0].group, "endpoint-group");
  EXPECT_EQ(network_context.reports()[0].url, GURL("https://example.com/page"));
  EXPECT_EQ(network_context.reports()[0].reporting_source, reporting_source);
  EXPECT_EQ(network_context.reports()[0].network_anonymization_key,
            network_anonymization_key);
  EXPECT_EQ(*network_context.reports()[0].body.FindString("connection"),
            "https://blocked.com/redirect");
  EXPECT_EQ(*network_context.reports()[0].body.FindString("disposition"),
            "enforce");

  // Test ConnectionAllowlistAllowsUrlAndReportIfNeeded
  GURL allowed_url("https://allowed.com/path");
  EXPECT_TRUE(ConnectionAllowlistAllowsUrlAndReportIfNeeded(
      policies, allowed_url, &network_context, network_anonymization_key,
      reporting_source));
  EXPECT_EQ(network_context.reports().size(), 1u);  // No new report

  GURL blocked_url("https://blocked.com/path");
  EXPECT_FALSE(ConnectionAllowlistAllowsUrlAndReportIfNeeded(
      policies, blocked_url, &network_context, network_anonymization_key,
      reporting_source));
  ASSERT_EQ(network_context.reports().size(), 2u);
  EXPECT_EQ(network_context.reports()[1].type, "connection-allowlist");
  EXPECT_EQ(network_context.reports()[1].group, "endpoint-group");
  EXPECT_EQ(network_context.reports()[1].url, GURL("https://example.com/page"));
  EXPECT_EQ(network_context.reports()[1].reporting_source, reporting_source);
  EXPECT_EQ(network_context.reports()[1].network_anonymization_key,
            network_anonymization_key);
  EXPECT_EQ(*network_context.reports()[1].body.FindString("connection"),
            "https://blocked.com/path");
  EXPECT_EQ(*network_context.reports()[1].body.FindString("disposition"),
            "enforce");
}

}  // namespace content
