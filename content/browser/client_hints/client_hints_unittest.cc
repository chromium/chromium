// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/client_hints/client_hints.h"

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
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
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_public_key.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace content {

namespace {

using ClientHintsVector = std::vector<network::mojom::WebClientHintsType>;
using network::mojom::WebClientHintsType;

static const blink::OriginTrialPublicKey kTestPublicKey = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

}  // namespace

class TestOriginTrialPolicy : public blink::OriginTrialPolicy {
 public:
  bool IsOriginTrialsSupported() const override { return true; }
  bool IsOriginSecure(const GURL& url) const override {
    return network::IsUrlPotentiallyTrustworthy(url);
  }
  const std::vector<blink::OriginTrialPublicKey>& GetPublicKeys()
      const override {
    return keys_;
  }
  void SetPublicKeys(const std::vector<blink::OriginTrialPublicKey>& keys) {
    keys_ = keys;
  }

 private:
  std::vector<blink::OriginTrialPublicKey> keys_;
};

class ClientHintsTest : public RenderViewHostImplTestHarness {
 public:
  ClientHintsTest() {
    blink::TrialTokenValidator::SetOriginTrialPolicyGetter(base::BindRepeating(
        [](blink::OriginTrialPolicy* policy) { return policy; },
        base::Unretained(&policy_)));
    policy_.SetPublicKeys({kTestPublicKey});
  }
  ClientHintsTest(const ClientHintsTest&) = delete;
  ClientHintsTest& operator=(const ClientHintsTest&) = delete;
  ~ClientHintsTest() override {
    blink::TrialTokenValidator::ResetOriginTrialPolicyGetter();
  }

  static constexpr char kOriginUrl[] = "https://example.com";
  // generate_token.py https://example.com SendFullUserAgentAfterReduction
  // --expire-timestamp=2000000000
  static constexpr char kValidOriginTrialToken[] =
      "AzA4UoK24p1LV/y6B032+L/M50GZfI4zx0aTri3ZGJpiq/"
      "o4eLMdErZ9p4YzbsCY9nrjmccZe12bs80EON8/"
      "eAYAAABpeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6"
      "ICJTZW5kRnVsbFVzZXJBZ2VudEFmdGVyUmVkdWN0aW9uIiwgImV4cGlyeSI6IDIwMDAwMDAw"
      "MDB9";

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

  absl::optional<ClientHintsVector> ParseAndPersist(
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

  std::string HintsToString(absl::optional<ClientHintsVector> hints) {
    if (!hints)
      return "";

    std::vector<std::string> hints_list;
    const auto& map = network::GetClientHintToNameMap();
    base::ranges::transform(hints.value(), std::back_inserter(hints_list),
                            [&map](network::mojom::WebClientHintsType hint) {
                              return map.at(hint);
                            });

    return base::JoinString(hints_list, ",");
  }

  std::pair<std::string, ClientHintsVector> GetAllNonOriginTrialHints() {
    std::vector<std::string> accept_ch_tokens;
    ClientHintsVector hints_list;
    for (const auto& pair : network::GetClientHintToNameMap()) {
      // Skip client hints used to origin trial.
      if (pair.first == WebClientHintsType::kFullUserAgent ||
          pair.first == WebClientHintsType::kUAReduced)
        continue;
      hints_list.push_back(pair.first);
      accept_ch_tokens.push_back(pair.second);
    }
    return {base::JoinString(accept_ch_tokens, ","), hints_list};
  }

 private:
  TestOriginTrialPolicy policy_;
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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({blink::features::kUserAgentClientHint},
                                       {});

  GURL url = GURL(ClientHintsTest::kOriginUrl);
  contents()->NavigateAndCommit(url);
  FrameTree& frame_tree = contents()->GetPrimaryFrameTree();
  FrameTreeNode* main_frame_node = frame_tree.root();
  AddOneChildNode();
  FrameTreeNode* sub_frame_node = main_frame_node->child_at(0);

  blink::UserAgentMetadata ua_metadata;
  MockClientHintsControllerDelegate delegate(ua_metadata);

  const auto& all_non_origin_trial_hints_pair = GetAllNonOriginTrialHints();
  const struct {
    std::string description;
    absl::optional<std::string> origin_trial_token;
    std::string accept_ch_str;
    raw_ptr<FrameTreeNode> frame_tree_node;
    absl::optional<ClientHintsVector> expect_hints;
    ClientHintsVector expect_commit_hints;
  } tests[] = {
      {"Persist hints for main frame", absl::nullopt,
       "sec-ch-ua-platform, sec-ch-ua-bitness", main_frame_node,
       absl::make_optional(ClientHintsVector{WebClientHintsType::kUAPlatform,
                                             WebClientHintsType::kUABitness}),
       ClientHintsVector{WebClientHintsType::kUAPlatform,
                         WebClientHintsType::kUABitness}},
      {"No persist hints for sub frame", absl::nullopt,
       "sec-ch-ua-platform, sec-ch-ua-bitness", sub_frame_node, absl::nullopt,
       ClientHintsVector{WebClientHintsType::kUAPlatform,
                         WebClientHintsType::kUABitness}},
      {"Origin trial hint for main frame",
       absl::nullopt,
       "sec-ch-ua-full",
       main_frame_node,
       absl::make_optional(ClientHintsVector{}),
       {}},
      {"Origin trial hint for sub frame",
       absl::nullopt,
       "sec-ch-ua-full",
       sub_frame_node,
       absl::nullopt,
       {}},
      {"Response with valid origin trial for main frame",
       kValidOriginTrialToken,
       "sec-ch-ua-full",
       main_frame_node,
       absl::make_optional(
           ClientHintsVector{WebClientHintsType::kFullUserAgent}),
       {WebClientHintsType::kFullUserAgent}},
      {"Response with valid origin trial for sub frame",
       kValidOriginTrialToken,
       "sec-ch-ua-full",
       sub_frame_node,
       absl::make_optional(
           ClientHintsVector{WebClientHintsType::kFullUserAgent}),
       {WebClientHintsType::kFullUserAgent}},
      {"All non origin trial client hints for main frame", absl::nullopt,
       all_non_origin_trial_hints_pair.first, main_frame_node,
       absl::make_optional(all_non_origin_trial_hints_pair.second),
       all_non_origin_trial_hints_pair.second},
      {"All non origin trial client hints for sub frame", absl::nullopt,
       all_non_origin_trial_hints_pair.first, sub_frame_node, absl::nullopt,
       all_non_origin_trial_hints_pair.second},
  };

  for (const auto& test : tests) {
    auto response_headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK\n");

    if (test.origin_trial_token)
      response_headers->SetHeader("Origin-Trial",
                                  test.origin_trial_token.value());

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

TEST_F(ClientHintsTest, SubFrameOptInOriginTrail) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({blink::features::kUserAgentClientHint},
                                       {});

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
  std::string accept_ch_str = "sec-ch-ua-full";

  // Without valid origin trial token for sub_frame_node, it should not
  // overwrite existing hints.
  auto actual_updated_hints = ParseAndPersist(
      url, response_headers.get(), accept_ch_str, sub_frame_node, &delegate);

  EXPECT_EQ(absl::nullopt, actual_updated_hints);
  blink::EnabledClientHints current_hints;
  delegate.GetAllowedClientHintsFromSource(url::Origin::Create(url),
                                           &current_hints);
  EXPECT_EQ(existing_hints, current_hints.GetEnabledHints());

  // With valid origin trial token for sub_frame_node, it should append origin
  // trial client hints to existing client hints cache.
  response_headers->SetHeader("Origin-Trial", kValidOriginTrialToken);
  actual_updated_hints = ParseAndPersist(
      url, response_headers.get(), accept_ch_str, sub_frame_node, &delegate);
  auto expect_update_hints = ClientHintsVector{
      WebClientHintsType::kUAPlatform, WebClientHintsType::kUABitness,
      WebClientHintsType::kFullUserAgent};
  EXPECT_EQ(expect_update_hints, actual_updated_hints);
  delegate.GetAllowedClientHintsFromSource(url::Origin::Create(url),
                                           &current_hints);
  EXPECT_EQ(expect_update_hints, current_hints.GetEnabledHints());
}

}  // namespace content
