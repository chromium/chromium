// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/client_hints/client_hints.h"

#include <algorithm>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "content/public/test/mock_client_hints_controller_delegate.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/http/http_response_headers.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"

namespace content {

namespace {

using ClientHintsVector = std::vector<network::mojom::WebClientHintsType>;
using network::mojom::WebClientHintsType;

}  // namespace

class ClientHintsTest : public RenderViewHostImplTestHarness {
 public:
  ClientHintsTest() = default;
  ClientHintsTest(const ClientHintsTest&) = delete;
  ClientHintsTest& operator=(const ClientHintsTest&) = delete;
  ~ClientHintsTest() override {
    blink::TrialTokenValidator::ResetOriginTrialPolicyGetter();
  }

  static constexpr char kOriginUrl[] = "https://example.com";

  void AddOneChildNode() {
    main_test_rfh()->OnCreateChildFrame(
        /*new_routing_id=*/14, TestRenderFrameHost::CreateStubFrameRemote(),
        TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
        TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
        TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
        /*scope=*/blink::mojom::TreeScopeType::kDocument, /*frame_name=*/"",
        /*frame_unique_name=*/"uniqueName0", /*is_created_by_script=*/false,
        /*frame_token=*/blink::LocalFrameToken(),
        /*devtools_frame_token=*/base::UnguessableToken::Create(),
        /*document_token=*/blink::DocumentToken(),
        /*frame_policy=*/blink::FramePolicy(),
        /*frame_owner_properties=*/blink::mojom::FrameOwnerProperties(),
        /*owner_type=*/blink::FrameOwnerElementType::kIframe,
        /*document_ukm_source_id=*/ukm::kInvalidSourceId);
  }

  std::optional<ClientHintsVector> ParseAndPersist(
      const GURL& url,
      const net::HttpResponseHeaders* response_header,
      const std::string& accept_ch_str,
      FrameTreeNode* frame_tree_node,
      MockClientHintsControllerDelegate* delegate) {
    auto parsed_headers = network::mojom::ParsedHeaders::New();
    parsed_headers->accept_ch = network::ParseClientHintsHeader(accept_ch_str);

    return ParseAndPersistAcceptCHForNavigation(
        url::Origin::Create(url), parsed_headers, response_header,
        browser_context(), delegate, frame_tree_node);
  }

  std::string HintsToString(std::optional<ClientHintsVector> hints) {
    if (!hints)
      return "";

    std::vector<std::string> hints_list;
    const auto& map = network::GetClientHintToNameMap();
    std::ranges::transform(hints.value(), std::back_inserter(hints_list),
                           [&map](network::mojom::WebClientHintsType hint) {
                             return map.at(hint);
                           });

    return base::JoinString(hints_list, ",");
  }

  std::pair<std::string, ClientHintsVector> GetAllClientHints() {
    std::vector<std::string> accept_ch_tokens;
    ClientHintsVector hints_list;
    for (const auto& pair : network::GetClientHintToNameMap()) {
      hints_list.push_back(pair.first);
      accept_ch_tokens.push_back(pair.second);
    }
    return {base::JoinString(accept_ch_tokens, ","), hints_list};
  }
};

TEST_F(ClientHintsTest, RttRoundedOff) {
  EXPECT_EQ(0u, RoundRttForTesting("", base::Milliseconds(1023)) % 50);
  EXPECT_EQ(0u, RoundRttForTesting("", base::Milliseconds(6787)) % 50);
  EXPECT_EQ(0u, RoundRttForTesting("", base::Milliseconds(12)) % 50);
  EXPECT_EQ(0u, RoundRttForTesting("foo.com", base::Milliseconds(1023)) % 50);
  EXPECT_EQ(0u, RoundRttForTesting("foo.com", base::Milliseconds(1193)) % 50);
  EXPECT_EQ(0u, RoundRttForTesting("foo.com", base::Milliseconds(12)) % 50);
}

TEST_F(ClientHintsTest, DownlinkRoundedOff) {
  EXPECT_GE(1,
            static_cast<int>(RoundKbpsToMbpsForTesting("", 102) * 1000) % 50);
  EXPECT_GE(1, static_cast<int>(RoundKbpsToMbpsForTesting("", 12) * 1000) % 50);
  EXPECT_GE(1,
            static_cast<int>(RoundKbpsToMbpsForTesting("", 2102) * 1000) % 50);

  EXPECT_GE(
      1,
      static_cast<int>(RoundKbpsToMbpsForTesting("foo.com", 102) * 1000) % 50);
  EXPECT_GE(
      1,
      static_cast<int>(RoundKbpsToMbpsForTesting("foo.com", 12) * 1000) % 50);
  EXPECT_GE(
      1,
      static_cast<int>(RoundKbpsToMbpsForTesting("foo.com", 2102) * 1000) % 50);
  EXPECT_GE(
      1, static_cast<int>(RoundKbpsToMbpsForTesting("foo.com", 12102) * 1000) %
             50);
}

// Verify that the value of RTT after adding noise is within approximately 10%
// of the original value. Note that the difference between the final value of
// RTT and the original value may be slightly more than 10% due to rounding off.
// To handle that, the maximum absolute difference allowed is set to a value
// slightly larger than 10% of the original metric value.
TEST_F(ClientHintsTest, FinalRttWithin10PercentValue) {
  EXPECT_NEAR(98, RoundRttForTesting("", base::Milliseconds(98)), 100);
  EXPECT_NEAR(1023, RoundRttForTesting("", base::Milliseconds(1023)), 200);
  EXPECT_NEAR(1193, RoundRttForTesting("", base::Milliseconds(1193)), 200);
  EXPECT_NEAR(2750, RoundRttForTesting("", base::Milliseconds(2750)), 400);
}

// Verify that the value of downlink after adding noise is within approximately
// 10% of the original value. Note that the difference between the final value
// of downlink and the original value may be slightly more than 10% due to
// rounding off. To handle that, the maximum absolute difference allowed is set
// to a value slightly larger than 10% of the original metric value.
TEST_F(ClientHintsTest, FinalDownlinkWithin10PercentValue) {
  EXPECT_NEAR(0.098, RoundKbpsToMbpsForTesting("", 98), 0.1);
  EXPECT_NEAR(1.023, RoundKbpsToMbpsForTesting("", 1023), 0.2);
  EXPECT_NEAR(1.193, RoundKbpsToMbpsForTesting("", 1193), 0.2);
  EXPECT_NEAR(7.523, RoundKbpsToMbpsForTesting("", 7523), 0.9);
  EXPECT_NEAR(9.999, RoundKbpsToMbpsForTesting("", 9999), 1.2);
}

TEST_F(ClientHintsTest, RttMaxValue) {
  EXPECT_GE(3000u, RoundRttForTesting("", base::Milliseconds(1023)));
  EXPECT_GE(3000u, RoundRttForTesting("", base::Milliseconds(2789)));
  EXPECT_GE(3000u, RoundRttForTesting("", base::Milliseconds(6023)));
  EXPECT_EQ(0u, RoundRttForTesting("", base::Milliseconds(1023)) % 50);
  EXPECT_EQ(0u, RoundRttForTesting("", base::Milliseconds(2789)) % 50);
  EXPECT_EQ(0u, RoundRttForTesting("", base::Milliseconds(6023)) % 50);
}

TEST_F(ClientHintsTest, DownlinkMaxValue) {
  EXPECT_GE(10.0, RoundKbpsToMbpsForTesting("", 102));
  EXPECT_GE(10.0, RoundKbpsToMbpsForTesting("", 2102));
  EXPECT_GE(10.0, RoundKbpsToMbpsForTesting("", 100102));
  EXPECT_GE(1,
            static_cast<int>(RoundKbpsToMbpsForTesting("", 102) * 1000) % 50);
  EXPECT_GE(1,
            static_cast<int>(RoundKbpsToMbpsForTesting("", 2102) * 1000) % 50);
  EXPECT_GE(
      1, static_cast<int>(RoundKbpsToMbpsForTesting("", 100102) * 1000) % 50);
}

TEST_F(ClientHintsTest, RttRandomized) {
  const int initial_value =
      RoundRttForTesting("example.com", base::Milliseconds(1023));
  bool network_quality_randomized_by_host = false;
  // There is a 1/20 chance that the same random noise is selected for two
  // different hosts. Run this test across 20 hosts to reduce the chances of
  // test failing to (1/20)^20.
  for (size_t i = 0; i < 20; ++i) {
    int value =
        RoundRttForTesting(base::NumberToString(i), base::Milliseconds(1023));
    // If |value| is different than |initial_value|, it implies that RTT is
    // randomized by host. This verifies the behavior, and test can be ended.
    if (value != initial_value)
      network_quality_randomized_by_host = true;
  }
  EXPECT_TRUE(network_quality_randomized_by_host);

  // Calling RoundRttForTesting for same host should return the same result.
  for (size_t i = 0; i < 20; ++i) {
    int value = RoundRttForTesting("example.com", base::Milliseconds(1023));
    EXPECT_EQ(initial_value, value);
  }
}

TEST_F(ClientHintsTest, DownlinkRandomized) {
  const int initial_value = RoundKbpsToMbpsForTesting("example.com", 1023);
  bool network_quality_randomized_by_host = false;
  // There is a 1/20 chance that the same random noise is selected for two
  // different hosts. Run this test across 20 hosts to reduce the chances of
  // test failing to (1/20)^20.
  for (size_t i = 0; i < 20; ++i) {
    int value = RoundKbpsToMbpsForTesting(base::NumberToString(i), 1023);
    // If |value| is different than |initial_value|, it implies that downlink is
    // randomized by host. This verifies the behavior, and test can be ended.
    if (value != initial_value)
      network_quality_randomized_by_host = true;
  }
  EXPECT_TRUE(network_quality_randomized_by_host);

  // Calling RoundMbps for same host should return the same result.
  for (size_t i = 0; i < 20; ++i) {
    int value = RoundKbpsToMbpsForTesting("example.com", 1023);
    EXPECT_EQ(initial_value, value);
  }
}

TEST_F(ClientHintsTest, IntegrationTestsOnParseLookUp) {
  GURL url = GURL(ClientHintsTest::kOriginUrl);
  contents()->NavigateAndCommit(url);
  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* main_frame_node = frame_tree.root();
  AddOneChildNode();
  FrameTreeNode* sub_frame_node = main_frame_node->child_at(0);

  blink::UserAgentMetadata ua_metadata;
  MockClientHintsControllerDelegate delegate(ua_metadata);

  const auto& all_non_origin_trial_hints_pair = GetAllClientHints();
  const struct {
    std::string description;
    std::string accept_ch_str;
    raw_ptr<FrameTreeNode> frame_tree_node;
    std::optional<ClientHintsVector> expect_hints;
    ClientHintsVector expect_commit_hints;
  } tests[] = {
      {"Persist hints for main frame", "sec-ch-ua-platform, sec-ch-ua-bitness",
       main_frame_node,
       std::make_optional(ClientHintsVector{WebClientHintsType::kUAPlatform,
                                            WebClientHintsType::kUABitness}),
       ClientHintsVector{WebClientHintsType::kUAPlatform,
                         WebClientHintsType::kUABitness}},
      {"No persist hints for sub frame",
       "sec-ch-ua-platform, sec-ch-ua-bitness", sub_frame_node, std::nullopt,
       ClientHintsVector{WebClientHintsType::kUAPlatform,
                         WebClientHintsType::kUABitness}},
      {"All client hints for main frame", all_non_origin_trial_hints_pair.first,
       main_frame_node,
       std::make_optional(all_non_origin_trial_hints_pair.second),
       all_non_origin_trial_hints_pair.second},
      {"All client hints for sub frame", all_non_origin_trial_hints_pair.first,
       sub_frame_node, std::nullopt, all_non_origin_trial_hints_pair.second},
  };

  for (const auto& test : tests) {
    auto response_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");

    auto actual_hints =
        ParseAndPersist(url, response_headers.get(), test.accept_ch_str,
                        test.frame_tree_node, &delegate);
    EXPECT_EQ(test.expect_hints, actual_hints)
        << "Test case [" << test.description << "]: expected hints "
        << HintsToString(test.expect_hints) << " but got "
        << HintsToString(actual_hints) << ".";

    // Verify commit hints.
    ClientHintsVector actual_commit_hints = LookupAcceptCHForCommit(
        url::Origin::Create(url), &delegate, test.frame_tree_node);
    EXPECT_EQ(test.expect_commit_hints, actual_commit_hints)
        << "Test case [" << test.description << "]: expected commit hints "
        << HintsToString(test.expect_commit_hints) << " but got "
        << HintsToString(actual_commit_hints) << ".";
  }
}

TEST_F(ClientHintsTest, SubFrame) {
  GURL url = GURL(ClientHintsTest::kOriginUrl);
  contents()->NavigateAndCommit(url);
  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* main_frame_node = frame_tree.root();
  AddOneChildNode();
  FrameTreeNode* sub_frame_node = main_frame_node->child_at(0);

  blink::UserAgentMetadata ua_metadata;
  MockClientHintsControllerDelegate delegate(ua_metadata);

  // Persist existing hint to accept-ch cache.
  ClientHintsVector existing_hints = ClientHintsVector{
      WebClientHintsType::kUAPlatform, WebClientHintsType::kUABitness};
  delegate.PersistClientHints(url::Origin::Create(url),
                              main_frame_node->GetParentOrOuterDocument(),
                              existing_hints);
  auto response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");
  std::string accept_ch_str = "sec-ch-ua-platform-version";

  // We shouldn't parse accept-ch in subframe, it should not overwrite existing
  // hints.
  auto actual_updated_hints = ParseAndPersist(
      url, response_headers.get(), accept_ch_str, sub_frame_node, &delegate);

  EXPECT_EQ(std::nullopt, actual_updated_hints);
  blink::EnabledClientHints current_hints;
  delegate.GetAllowedClientHintsFromSource(url::Origin::Create(url),
                                           &current_hints);
  EXPECT_EQ(existing_hints, current_hints.GetEnabledHints());
}

TEST_F(ClientHintsTest, GetEnabledClientHintsMainFrame) {
  GURL main_url(kOriginUrl);
  contents()->NavigateAndCommit(main_url);
  url::Origin origin = url::Origin::Create(main_url);

  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* main_frame_node = frame_tree.root();

  blink::UserAgentMetadata ua_metadata;
  MockClientHintsControllerDelegate delegate(ua_metadata);

  std::vector<network::mojom::WebClientHintsType> expected_types = {
      network::mojom::WebClientHintsType::kUAArch,
      network::mojom::WebClientHintsType::kUAWoW64,
  };
  delegate.SetAdditionalClientHints(expected_types);
  // Add default ClientHints.
  for (const auto& [hint, _] : network::GetClientHintToNameMap()) {
    if (blink::IsClientHintSentByDefault(hint)) {
      expected_types.push_back(hint);
    }
  }

  const auto& actual_hints =
      GetEnabledClientHints(origin, main_frame_node, &delegate);

  EXPECT_EQ(origin, actual_hints.origin);
  EXPECT_TRUE(actual_hints.is_outermost_main_frame);
  // We do not care the order of contents.
  EXPECT_THAT(actual_hints.hints,
              testing::UnorderedElementsAreArray(expected_types));
  EXPECT_TRUE(actual_hints.not_allowed_hints.empty());
}

TEST_F(ClientHintsTest, GetEnabledClientHintsSubframe) {
  GURL main_url(kOriginUrl);
  contents()->NavigateAndCommit(main_url);
  url::Origin origin = url::Origin::Create(main_url);

  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* main_frame_node = frame_tree.root();
  AddOneChildNode();
  FrameTreeNode* sub_frame_node = main_frame_node->child_at(0);

  blink::UserAgentMetadata ua_metadata;
  MockClientHintsControllerDelegate delegate(ua_metadata);

  // Add default ClientHints.
  std::vector<network::mojom::WebClientHintsType> expected_types;
  for (const auto& [hint, _] : network::GetClientHintToNameMap()) {
    if (blink::IsClientHintSentByDefault(hint)) {
      expected_types.push_back(hint);
    }
  }

  // The origin passed to GetEnabledClientHints is the resource origin.
  // The origin in the result is the main frame origin.
  GURL sub_url("https://sub.example.com");
  const auto& actual_hints = GetEnabledClientHints(url::Origin::Create(sub_url),
                                                   sub_frame_node, &delegate);

  EXPECT_EQ(origin, actual_hints.origin);
  EXPECT_FALSE(actual_hints.is_outermost_main_frame);
  // Just check if the hints has the default low entropy client hints.
  // We do not care the order of contents.
  EXPECT_THAT(actual_hints.hints,
              testing::UnorderedElementsAreArray(expected_types));
}

TEST_F(ClientHintsTest, AddPrefetchNavigationRequestClientHintsHeaders) {
  GURL url = GURL(ClientHintsTest::kOriginUrl);
  url::Origin origin = url::Origin::Create(url);

  blink::UserAgentMetadata ua_metadata;
  ua_metadata.architecture = "x86";
  MockClientHintsControllerDelegate delegate(ua_metadata);

  // Enable a variety of hints.
  delegate.SetAdditionalClientHints(
      {WebClientHintsType::kDeviceMemory, WebClientHintsType::kDpr,
       WebClientHintsType::kUAArch, WebClientHintsType::kPrefersColorScheme});

  // TODO(crbug.com/427866914): Add tests for UA overrides by DevTools.
  net::HttpRequestHeaders headers;
  AddPrefetchNavigationRequestClientHintsHeaders(
      origin, &headers, browser_context(), &delegate,
      /*is_ua_override_on=*/false, /*ftn_for_devtools_override=*/nullptr);

  // Low-entropy UA hints are sent by default.
  EXPECT_TRUE(headers.HasHeader("sec-ch-ua"));
  EXPECT_TRUE(headers.HasHeader("sec-ch-ua-mobile"));

  // High-entropy hints that were enabled.
  std::optional<std::string> header_value = headers.GetHeader("sec-ch-ua-arch");
  EXPECT_TRUE(header_value.has_value());
  EXPECT_EQ("\"x86\"", header_value.value());

  EXPECT_TRUE(headers.HasHeader("sec-ch-device-memory"));
  EXPECT_TRUE(headers.HasHeader("sec-ch-dpr"));

  // Prefers-color-scheme depends on a frame, which is null for prefetch.
  EXPECT_FALSE(headers.HasHeader("sec-ch-prefers-color-scheme"));

  // Verify hints not in the requested list.
  EXPECT_FALSE(headers.HasHeader("sec-ch-ua-bitness"));
}

TEST_F(ClientHintsTest, AddNavigationRequestClientHintsHeaders_MainFrame) {
  GURL main_url(kOriginUrl);
  contents()->NavigateAndCommit(main_url);
  url::Origin origin = url::Origin::Create(main_url);
  FrameTreeNode* main_frame_node = contents()->GetPrimaryFrameTree().root();

  blink::UserAgentMetadata ua_metadata;
  ua_metadata.architecture = "x86";
  MockClientHintsControllerDelegate delegate(ua_metadata);

  // Enable a variety of hints.
  delegate.SetAdditionalClientHints(
      {WebClientHintsType::kDeviceMemory, WebClientHintsType::kDpr,
       WebClientHintsType::kUAArch, WebClientHintsType::kPrefersColorScheme});

  net::HttpRequestHeaders headers;
  AddNavigationRequestClientHintsHeaders(
      origin, &headers, browser_context(), &delegate,
      /*is_ua_override_on=*/false, main_frame_node,
      /*container_policy=*/{});

  // Low-entropy UA hints are sent by default.
  EXPECT_TRUE(headers.HasHeader("sec-ch-ua"));
  EXPECT_TRUE(headers.HasHeader("sec-ch-ua-mobile"));

  // High-entropy hints that were enabled.
  std::optional<std::string> header_value = headers.GetHeader("sec-ch-ua-arch");
  EXPECT_TRUE(header_value.has_value());
  EXPECT_EQ("\"x86\"", header_value.value());

  EXPECT_TRUE(headers.HasHeader("sec-ch-device-memory"));
  EXPECT_TRUE(headers.HasHeader("sec-ch-dpr"));

  // Prefers-color-scheme should have value for navigation request.
  EXPECT_TRUE(headers.HasHeader("sec-ch-prefers-color-scheme"));

  // Verify hints not in the requested list.
  EXPECT_FALSE(headers.HasHeader("sec-ch-ua-bitness"));
}

TEST_F(ClientHintsTest, AddNavigationRequestClientHintsHeaders_Subframe) {
  GURL url = GURL(ClientHintsTest::kOriginUrl);
  contents()->NavigateAndCommit(url);
  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* main_frame_node = frame_tree.root();
  AddOneChildNode();
  FrameTreeNode* sub_frame_node = main_frame_node->child_at(0);

  GURL subframe_url("https://sub.example.com");
  url::Origin subframe_origin = url::Origin::Create(subframe_url);

  blink::UserAgentMetadata ua_metadata;
  MockClientHintsControllerDelegate delegate(ua_metadata);

  // Enable a hint that requires delegation.
  delegate.SetAdditionalClientHints({WebClientHintsType::kDeviceMemory});

  net::HttpRequestHeaders headers;
  // Case 1: No delegation policy. Hint should not be added.
  AddNavigationRequestClientHintsHeaders(
      subframe_origin, &headers, browser_context(), &delegate,
      /*is_ua_override_on=*/false, sub_frame_node,
      /*container_policy=*/{});
  EXPECT_FALSE(headers.HasHeader("sec-ch-device-memory"));

  // Case 2: With delegation policy. Hint should be added.
  headers.Clear();
  network::ParsedPermissionsPolicy container_policy;
  network::ParsedPermissionsPolicyDeclaration decl(
      network::mojom::PermissionsPolicyFeature::kClientHintDeviceMemory,
      {*network::OriginWithPossibleWildcards::FromOrigin(subframe_origin)},
      /*self_if_matches=*/std::nullopt,
      /*matches_all_origins=*/false, /*matches_opaque_src=*/false);
  container_policy.push_back(std::move(decl));

  AddNavigationRequestClientHintsHeaders(
      subframe_origin, &headers, browser_context(), &delegate,
      /*is_ua_override_on=*/false, sub_frame_node, container_policy);

  EXPECT_TRUE(headers.HasHeader("sec-ch-device-memory"));
}

TEST_F(ClientHintsTest, GetCriticalHintsMissingStatus) {
  const GURL main_url(kOriginUrl);
  contents()->NavigateAndCommit(main_url);
  const url::Origin origin = url::Origin::Create(main_url);

  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* main_frame_node = frame_tree.root();

  blink::UserAgentMetadata ua_metadata;
  MockClientHintsControllerDelegate delegate(ua_metadata);

  const ClientHintsVector critical_hints = {WebClientHintsType::kDeviceMemory,
                                            WebClientHintsType::kDpr};

  // No hints persisted.
  EXPECT_EQ(GetCriticalHintsMissingStatus(origin, main_frame_node, &delegate,
                                          critical_hints),
            CriticalHintsMissingStatus::kMissing);

  // One hint persisted.
  delegate.PersistClientHints(origin,
                              main_frame_node->GetParentOrOuterDocument(),
                              {WebClientHintsType::kDeviceMemory});
  EXPECT_EQ(GetCriticalHintsMissingStatus(origin, main_frame_node, &delegate,
                                          critical_hints),
            CriticalHintsMissingStatus::kMissing);

  // All hints persisted.
  delegate.PersistClientHints(
      origin, main_frame_node->GetParentOrOuterDocument(), critical_hints);
  EXPECT_EQ(GetCriticalHintsMissingStatus(origin, main_frame_node, &delegate,
                                          critical_hints),
            CriticalHintsMissingStatus::kPresent);
}

TEST_F(ClientHintsTest, GetEnabledClientHintsSubframeNotAllowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{network::features::kOffloadAcceptCHFrameCheck,
        {{network::features::kAcceptCHFrameOffloadNotAllowedHints.name,
          "true"}}}},
      {});

  GURL main_url(kOriginUrl);
  contents()->NavigateAndCommit(main_url);
  url::Origin main_origin = url::Origin::Create(main_url);

  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* main_frame_node = frame_tree.root();
  AddOneChildNode();
  FrameTreeNode* sub_frame_node = main_frame_node->child_at(0);

  blink::UserAgentMetadata ua_metadata;
  MockClientHintsControllerDelegate delegate(ua_metadata);

  GURL sub_url("https://sub.example.com");
  const auto& actual_hints = GetEnabledClientHints(url::Origin::Create(sub_url),
                                                   sub_frame_node, &delegate);

  std::vector<network::mojom::WebClientHintsType> expected_allowed_hints;
  std::vector<network::mojom::WebClientHintsType> expected_not_allowed_hints;

  for (const auto& [hint, _] : network::GetClientHintToNameMap()) {
    if (blink::IsClientHintSentByDefault(hint)) {
      expected_allowed_hints.push_back(hint);
    } else {
      expected_not_allowed_hints.push_back(hint);
    }
  }

  EXPECT_EQ(main_origin, actual_hints.origin);
  EXPECT_FALSE(actual_hints.is_outermost_main_frame);
  EXPECT_THAT(actual_hints.hints,
              testing::UnorderedElementsAreArray(expected_allowed_hints));
  EXPECT_THAT(actual_hints.not_allowed_hints,
              testing::UnorderedElementsAreArray(expected_not_allowed_hints));
}

}  // namespace content
