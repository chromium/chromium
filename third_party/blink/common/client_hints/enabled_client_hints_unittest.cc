// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_public_key.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace blink {

using ::network::mojom::WebClientHintsType;
using ::testing::ElementsAre;

static constexpr char kOriginUrl[] = "https://127.0.0.1:44444";
static constexpr char kThirdPartyOriginUrl[] = "https://127.0.0.1:44445";
static const OriginTrialPublicKey kTestPublicKey = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

namespace {

void AddHeader(net::HttpResponseHeaders* response_headers,
               const std::string& header,
               const std::string& value) {
  response_headers->AddHeader(header, value);
}

void VerifyClientHintEnabledWithOriginTrialTokenInner(
    net::HttpResponseHeaders* response_headers,
    const std::string& token,
    const GURL* third_party_url,
    const WebClientHintsType client_hint_type,
    bool expected_client_hint_enabled) {
  AddHeader(response_headers, "Origin-Trial", token);
  absl::optional<GURL> maybe_third_party_url;
  if (third_party_url)
    maybe_third_party_url = absl::make_optional(*third_party_url);

  EnabledClientHints hints;
  hints.SetIsEnabled(GURL(kOriginUrl), maybe_third_party_url, response_headers,
                     client_hint_type, true);
  EXPECT_TRUE(hints.IsEnabled(client_hint_type) ==
              expected_client_hint_enabled);
}

}  // namespace

class TestOriginTrialPolicy : public OriginTrialPolicy {
 public:
  bool IsOriginTrialsSupported() const override { return true; }
  bool IsOriginSecure(const GURL& url) const override {
    return url.SchemeIs("https");
  }
  const std::vector<OriginTrialPublicKey>& GetPublicKeys() const override {
    return keys_;
  }
  void SetPublicKeys(const std::vector<OriginTrialPublicKey>& keys) {
    keys_ = keys;
  }

 private:
  std::vector<OriginTrialPublicKey> keys_;
};

class EnabledClientHintsTest : public testing::Test {
 public:
  EnabledClientHintsTest()
      : response_headers_(base::MakeRefCounted<net::HttpResponseHeaders>("")) {
    // The UserAgentClientHint feature is enabled, and the
    // PrefersColorSchemeClientHintHeader feature is disabled.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kUserAgentClientHint},
        /*disabled_features=*/{
            blink::features::kPrefersColorSchemeClientHintHeader});
    TrialTokenValidator::SetOriginTrialPolicyGetter(
        base::BindRepeating([](OriginTrialPolicy* policy) { return policy; },
                            base::Unretained(&policy_)));
    policy_.SetPublicKeys({kTestPublicKey});
  }

  ~EnabledClientHintsTest() override {
    TrialTokenValidator::ResetOriginTrialPolicyGetter();
  }

  const net::HttpResponseHeaders* response_headers() const {
    return response_headers_.get();
  }

  void VerifyClientHintEnabledWithOriginTrialToken(
      const std::string& token,
      const GURL* third_party_url,
      const WebClientHintsType client_hint_type,
      bool expected_client_hint_enabled) {
    VerifyClientHintEnabledWithOriginTrialTokenInner(
        response_headers_.get(), token, third_party_url, client_hint_type,
        expected_client_hint_enabled);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestOriginTrialPolicy policy_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
};

TEST_F(EnabledClientHintsTest, EnabledClientHint) {
  EnabledClientHints hints;
  hints.SetIsEnabled(WebClientHintsType::kUAFullVersion, true);
  hints.SetIsEnabled(WebClientHintsType::kUAFullVersionList, true);
  hints.SetIsEnabled(WebClientHintsType::kRtt_DEPRECATED, true);
  EXPECT_TRUE(hints.IsEnabled(WebClientHintsType::kUAFullVersion));
  EXPECT_TRUE(hints.IsEnabled(WebClientHintsType::kUAFullVersionList));
  EXPECT_TRUE(hints.IsEnabled(WebClientHintsType::kRtt_DEPRECATED));
}

TEST_F(EnabledClientHintsTest, DisabledClientHint) {
  EnabledClientHints hints;
  hints.SetIsEnabled(WebClientHintsType::kUAFullVersion, false);
  hints.SetIsEnabled(WebClientHintsType::kUAFullVersionList, false);
  hints.SetIsEnabled(WebClientHintsType::kRtt_DEPRECATED, false);
  EXPECT_FALSE(hints.IsEnabled(WebClientHintsType::kUAFullVersion));
  EXPECT_FALSE(hints.IsEnabled(WebClientHintsType::kUAFullVersionList));
  EXPECT_FALSE(hints.IsEnabled(WebClientHintsType::kRtt_DEPRECATED));
}

TEST_F(EnabledClientHintsTest, EnabledClientHintOnDisabledFeature) {
  EnabledClientHints hints;
  // Attempting to enable the PrefersColorScheme client hint, but the runtime
  // flag for it is disabled.
  hints.SetIsEnabled(WebClientHintsType::kPrefersColorScheme, true);
  EXPECT_FALSE(hints.IsEnabled(WebClientHintsType::kPrefersColorScheme));
}

TEST_F(EnabledClientHintsTest,
       EnabledUaReducedClientHintWithValidOriginTrialToken) {
  // Generated by running (in tools/origin_trials):
  // generate_token.py https://127.0.0.1:44444 UserAgentReduction
  // --expire-timestamp=2000000000
  //
  // The Origin Trial token expires in 2033.  Generate a new token by then, or
  // find a better way to re-generate a test trial token.
  static constexpr char kValidOriginTrialToken[] =
      "A93QtcQ0CRKf5ioPasUwNbweXQWgbI4ZEshiz+"
      "YS7dkQEWVfW9Ua2pTnA866sZwRzuElkPwsUdGdIaW0fRUP8AwAAABceyJvcmlnaW4iOiAiaH"
      "R0"
      "cHM6Ly8xMjcuMC4wLjE6NDQ0NDQiLCAiZmVhdHVyZSI6ICJVc2VyQWdlbnRSZWR1Y3Rpb24i"
      "LC"
      "AiZXhwaXJ5IjogMjAwMDAwMDAwMH0=";

  VerifyClientHintEnabledWithOriginTrialToken(
      kValidOriginTrialToken,
      /*third_party_url=*/nullptr, WebClientHintsType::kUAReduced,
      /*expected_client_hint_enabled=*/true);
}

TEST_F(EnabledClientHintsTest,
       EnabledUaReducedClientHintWithInvalidOriginTrialToken) {
  // A slight corruption (changing a character) of a valid OT token.
  static constexpr char kInvalidOriginTrialToken[] =
      "A93QtcQ0CRKf5ioPasUwNbweXQWgbI4ZEshiz+"
      "YS7dkQEWVfW9Ua2pTnA866sZwRzuElkPwsUdGdIaW0fRUP8AwAAABceyJvcmlnaW4iOiAiaH"
      "R0"
      "cHM6Ly8xMjcuMC4wLjE6NDQ0NDQiLCAiZmVhdHVyzSI6ICJVc2VyQWdlbnRSZWR1Y3Rpb24i"
      "LC"
      "AiZXhwaXJ5IjogMjAwMDAwMDAwMH0=";

  VerifyClientHintEnabledWithOriginTrialToken(
      kInvalidOriginTrialToken,
      /*third_party_url=*/nullptr, WebClientHintsType::kUAReduced,
      /*expected_client_hint_enabled=*/false);
}

TEST_F(EnabledClientHintsTest,
       EnabledUaReducedClientHintWithValidThirdPartyOriginTrialToken) {
  // Generated by running (in tools/origin_trials):
  // generate_token.py https://127.0.0.1:44445 UserAgentReduction
  // --expire-timestamp=2000000000 --is-third-party
  //
  // The Origin Trial token expires in 2033.  Generate a new token by then, or
  // find a better way to re-generate a test trial token.
  static constexpr char kValidThirdPartyOriginTrialToken[] =
      "A4lFNHY7SM+O+"
      "OARSmPBQE5KVgHzUC72OeNriQnoAPyEWT4pvrXh7z95x7C4gZpb7Qf7JL7E2b2lV2jvHzPwD"
      "goAAAByeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NDUiLCAiaXNUaGlyZFBhc"
      "nR5IjogdHJ1ZSwgImZlYXR1cmUiOiAiVXNlckFnZW50UmVkdWN0aW9uIiwgImV4cGlyeSI6I"
      "DIwMDAwMDAwMDB9";

  const GURL third_party_url = GURL(kThirdPartyOriginUrl);
  VerifyClientHintEnabledWithOriginTrialToken(
      kValidThirdPartyOriginTrialToken, &third_party_url,
      WebClientHintsType::kUAReduced, /*expected_client_hint_enabled=*/true);
}

TEST_F(EnabledClientHintsTest,
       EnabledUaReducedClientHintThirdPartyWithValidOriginTrialToken) {
  // Generated by running (in tools/origin_trials):
  // generate_token.py https://127.0.0.1:44445 UserAgentReduction
  // --expire-timestamp=2000000000
  //
  // We are using a valid first party OT token for the third-party URL, which
  // should be rejected by the TrialTokenValidator for third-party contexts.
  //
  // The Origin Trial token expires in 2033.  Generate a new token by then, or
  // find a better way to re-generate a test trial token.
  static constexpr char kValidOriginTrialTokenForThirdPartyUrl[] =
      "Axn1AIzTAEaYasceEQyucSkZpDyq6bH4NxYHECXpSXr9LBG22yjc8NXkRZWbiWzTIrjEtHkm"
      "PJSbocBts5BLnAYAAABceyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NDUiLCAi"
      "ZmVhdHVyZSI6ICJVc2VyQWdlbnRSZWR1Y3Rpb24iLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0"
      "=";

  const GURL third_party_url = GURL(kThirdPartyOriginUrl);
  VerifyClientHintEnabledWithOriginTrialToken(
      kValidOriginTrialTokenForThirdPartyUrl, &third_party_url,
      WebClientHintsType::kUAReduced, /*expected_client_hint_enabled=*/false);
}

TEST_F(EnabledClientHintsTest,
       EnabledUADeprecationClientHintWithValidOriginTrialToken) {
  // Generated by running (in tools/origin_trials):
  // generate_token.py https://127.0.0.1:44444 SendFullUserAgentAfterReduction
  // --expire-timestamp=2000000000
  //
  // The Origin Trial token expires in 2033.  Generate a new token by then, or
  // find a better way to re-generate a test trial token.
  static constexpr char kValidOriginTrialToken[] =
      "A6+Ti/9KuXTgmFzOQwkTuO8k0QFH8vUaxmv0CllAET1/"
      "307KShF6fhskMuBqFUvqO7ViAkZ+"
      "NSeJhQI0n5aLggsAAABpeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NDQiLCAi"
      "ZmVhdHVyZSI6ICJTZW5kRnVsbFVzZXJBZ2VudEFmdGVyUmVkdWN0aW9uIiwgImV4cGlyeSI6"
      "IDIwMDAwMDAwMDB9";

  VerifyClientHintEnabledWithOriginTrialToken(
      kValidOriginTrialToken,
      /*third_party_url=*/nullptr, WebClientHintsType::kFullUserAgent,
      /*expected_client_hint_enabled=*/true);
}

TEST_F(EnabledClientHintsTest,
       EnabledUADeprecationClientHintWithInvalidOriginTrialToken) {
  // A slight corruption (changing a character) of a valid OT token.
  static constexpr char kInvalidOriginTrialToken[] =
      "A6+Ti/9KuXTgmFzOQwkTuO8k0QFH8vUaxmv0CllAET1/"
      "307KShF6fhskMuBqFUvqO7ViAkZ+"
      "NSeJhQI0n5aLggsAAABpeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NDQiLCAi"
      "ZmVhdHVyZSI6ICTZW5kRnVsbFVzZXJBZ2VudEFmdGVyUmVkdWN0aW9uIiwgImV4cGlyeSI6"
      "IDIwMDAwMDAwMDB9";

  VerifyClientHintEnabledWithOriginTrialToken(
      kInvalidOriginTrialToken,
      /*third_party_url=*/nullptr, WebClientHintsType::kFullUserAgent,
      /*expected_client_hint_enabled=*/false);
}

TEST_F(EnabledClientHintsTest,
       EnabledUADeprecationClientHintWithValidThirdPartyOriginTrialToken) {
  // Generated by running (in tools/origin_trials):
  // generate_token.py https://127.0.0.1:44445 SendFullUserAgentAfterReduction
  // --expire-timestamp=2000000000 --is-third-party
  //
  // The Origin Trial token expires in 2033.  Generate a new token by then, or
  // find a better way to re-generate a test trial token.
  static constexpr char kValidThirdPartyOriginTrialToken[] =
      "A0q1jQxOoBMkORITt4dborMF2TE0MVT71JbLomfT4tg8nKuEiRcDNTLVEfSffhxcwqMYEmXs"
      "p4CXXypHENjrvwgAAAB/"
      "eyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NDUiLCAiaXNUaGlyZFBhcnR5Ijog"
      "dHJ1ZSwgImZlYXR1cmUiOiAiU2VuZEZ1bGxVc2VyQWdlbnRBZnRlclJlZHVjdGlvbiIsICJl"
      "eHBpcnkiOiAyMDAwMDAwMDAwfQ==";

  const GURL third_party_url = GURL(kThirdPartyOriginUrl);
  VerifyClientHintEnabledWithOriginTrialToken(
      kValidThirdPartyOriginTrialToken, &third_party_url,
      WebClientHintsType::kFullUserAgent,
      /*expected_client_hint_enabled=*/true);
}

TEST_F(EnabledClientHintsTest,
       EnabledUADeprecationClientHintThirdPartyWithValidOriginTrialToken) {
  // Generated by running (in tools/origin_trials):
  // generate_token.py https://127.0.0.1:44445 SendFullUserAgentAfterReduction
  // --expire-timestamp=2000000000
  //
  // We are using a valid first party OT token for the third-party URL, which
  // should be rejected by the TrialTokenValidator for third-party contexts.
  //
  // The Origin Trial token expires in 2033.  Generate a new token by then, or
  // find a better way to re-generate a test trial token.
  static constexpr char kValidOriginTrialTokenForThirdPartyUrl[] =
      "A0kGFcySC9Pfb0ouX/"
      "Ks2SYCmUEIkhU0aje4kHgLaCTgeOKoUaIwcrVSsiZgs3Im2vmPHwcaoqwzr/"
      "d0YqDtzQQAAABpeyJvcmlnaW4iOiAiaHR0cHM6Ly8xMjcuMC4wLjE6NDQ0NDUiLCAiZmVhdH"
      "VyZSI6ICJTZW5kRnVsbFVzZXJBZ2VudEFmdGVyUmVkdWN0aW9uIiwgImV4cGlyeSI6IDIwMD"
      "AwMDAwMDB9";

  const GURL third_party_url = GURL(kThirdPartyOriginUrl);
  VerifyClientHintEnabledWithOriginTrialToken(
      kValidOriginTrialTokenForThirdPartyUrl, &third_party_url,
      WebClientHintsType::kFullUserAgent,
      /*expected_client_hint_enabled=*/false);
}

TEST_F(EnabledClientHintsTest, GetEnabledHints) {
  EnabledClientHints hints;
  hints.SetIsEnabled(WebClientHintsType::kUAFullVersion, true);
  hints.SetIsEnabled(WebClientHintsType::kUAFullVersionList, true);
  hints.SetIsEnabled(WebClientHintsType::kRtt_DEPRECATED, true);
  EXPECT_THAT(hints.GetEnabledHints(),
              ElementsAre(WebClientHintsType::kRtt_DEPRECATED,
                          WebClientHintsType::kUAFullVersion,
                          WebClientHintsType::kUAFullVersionList));
}

}  // namespace blink
