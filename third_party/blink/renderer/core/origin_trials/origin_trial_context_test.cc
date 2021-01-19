// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

const char kFrobulateTrialName[] = "Frobulate";
const char kFrobulateDeprecationTrialName[] = "FrobulateDeprecation";
const char kFrobulateNavigationTrialName[] = "FrobulateNavigation";
const char kFrobulateThirdPartyTrialName[] = "FrobulateThirdParty";
const char kFrobulateEnabledOrigin[] = "https://www.example.com";
const char kFrobulateEnabledOriginUnsecure[] = "http://www.example.com";

// Names of UMA histograms
const char kResultHistogram[] = "OriginTrials.ValidationResult";

// Trial token placeholder for mocked calls to validator
const char kTokenPlaceholder[] = "The token contents are not used";

class MockTokenValidator : public TrialTokenValidator {
 public:
  MockTokenValidator()
      : response_(OriginTrialTokenStatus::kNotSupported), call_count_(0) {}
  ~MockTokenValidator() override = default;

  // blink::WebTrialTokenValidator implementation
  TrialTokenResult ValidateToken(base::StringPiece token,
                                 const url::Origin& origin,
                                 base::Time current_time) const override {
    call_count_++;
    return response_;
  }
  TrialTokenResult ValidateToken(base::StringPiece token,
                                 const url::Origin& origin,
                                 const url::Origin* script_origin,
                                 base::Time current_time) const override {
    return ValidateToken(token, origin, current_time);
  }

  // Useful methods for controlling the validator
  void SetResponse(OriginTrialTokenStatus status,
                   const std::string& feature,
                   base::Time expiry = base::Time(),
                   bool is_third_party = false) {
    response_.status = status;
    response_.feature_name = feature;
    response_.expiry_time = expiry;
    response_.is_third_party = is_third_party;
  }
  int CallCount() { return call_count_; }

 private:
  TrialTokenResult response_;

  mutable int call_count_;

  DISALLOW_COPY_AND_ASSIGN(MockTokenValidator);
};

}  // namespace

class OriginTrialContextTest : public testing::Test {
 protected:
  OriginTrialContextTest()
      : token_validator_(new MockTokenValidator),
        execution_context_(MakeGarbageCollected<NullExecutionContext>()),
        histogram_tester_(new HistogramTester()) {
    execution_context_->GetOriginTrialContext()
        ->SetTrialTokenValidatorForTesting(
            std::unique_ptr<MockTokenValidator>(token_validator_));
  }
  ~OriginTrialContextTest() override {
    execution_context_->NotifyContextDestroyed();
  }

  MockTokenValidator* TokenValidator() { return token_validator_; }

  void UpdateSecurityOrigin(const String& origin) {
    KURL page_url(origin);
    scoped_refptr<SecurityOrigin> page_origin =
        SecurityOrigin::Create(page_url);
    execution_context_->GetSecurityContext().SetSecurityOrigin(page_origin);
  }

  bool IsFeatureEnabled(const String& origin, OriginTrialFeature feature) {
    UpdateSecurityOrigin(origin);
    return IsFeatureEnabled(feature);
  }

  bool IsFeatureEnabled(OriginTrialFeature feature) {
    // Need at least one token to ensure the token validator is called.
    execution_context_->GetOriginTrialContext()->AddToken(kTokenPlaceholder);
    return execution_context_->GetOriginTrialContext()->IsFeatureEnabled(
        feature);
  }

  bool IsFeatureEnabledForThirdPartyOrigin(const String& origin,
                                           const String& script_origin,
                                           OriginTrialFeature feature) {
    UpdateSecurityOrigin(origin);
    KURL script_url(script_origin);
    scoped_refptr<const SecurityOrigin> script_security_origin =
        SecurityOrigin::Create(script_url);
    // Need at least one token to ensure the token validator is called.
    execution_context_->GetOriginTrialContext()->AddTokenFromExternalScript(
        kTokenPlaceholder, script_security_origin.get());
    return execution_context_->GetOriginTrialContext()->IsFeatureEnabled(
        feature);
  }

  base::Time GetFeatureExpiry(OriginTrialFeature feature) {
    return execution_context_->GetOriginTrialContext()->GetFeatureExpiry(
        feature);
  }

  std::unique_ptr<Vector<OriginTrialFeature>> GetEnabledNavigationFeatures() {
    return execution_context_->GetOriginTrialContext()
        ->GetEnabledNavigationFeatures();
  }

  bool ActivateNavigationFeature(OriginTrialFeature feature) {
    execution_context_->GetOriginTrialContext()
        ->ActivateNavigationFeaturesFromInitiator({feature});
    return execution_context_->GetOriginTrialContext()
        ->IsNavigationFeatureActivated(feature);
  }

  void ExpectStatusUniqueMetric(OriginTrialTokenStatus status, int count) {
    histogram_tester_->ExpectUniqueSample(kResultHistogram,
                                          static_cast<int>(status), count);
  }

  void ExpecStatusTotalMetric(int total) {
    histogram_tester_->ExpectTotalCount(kResultHistogram, total);
  }

 private:
  MockTokenValidator* token_validator_;
  Persistent<NullExecutionContext> execution_context_;
  std::unique_ptr<HistogramTester> histogram_tester_;
};

TEST_F(OriginTrialContextTest, EnabledNonExistingTrial) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateTrialName);
  bool is_non_existing_feature_enabled = IsFeatureEnabled(
      kFrobulateEnabledOrigin, OriginTrialFeature::kNonExisting);
  EXPECT_FALSE(is_non_existing_feature_enabled);

  // Status metric should be updated.
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kSuccess, 1);
}

// The feature should be enabled if a valid token for the origin is provided
TEST_F(OriginTrialContextTest, EnabledSecureRegisteredOrigin) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateTrialName);
  bool is_origin_enabled = IsFeatureEnabled(
      kFrobulateEnabledOrigin, OriginTrialFeature::kOriginTrialsSampleAPI);
  EXPECT_TRUE(is_origin_enabled);
  EXPECT_EQ(1, TokenValidator()->CallCount());

  // Status metric should be updated.
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kSuccess, 1);

  // kOriginTrialsSampleAPI is not a navigation feature, so shouldn't be
  // included in GetEnabledNavigationFeatures().
  EXPECT_EQ(nullptr, GetEnabledNavigationFeatures());
}

// The feature should be enabled if a valid token for a deprecation trial for
// the origin is provided.
TEST_F(OriginTrialContextTest, EnabledSecureRegisteredOriginDeprecation) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateDeprecationTrialName);
  bool is_origin_enabled =
      IsFeatureEnabled(kFrobulateEnabledOrigin,
                       OriginTrialFeature::kOriginTrialsSampleAPIDeprecation);
  EXPECT_TRUE(is_origin_enabled);
  EXPECT_EQ(1, TokenValidator()->CallCount());

  // Status metric should be updated.
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kSuccess, 1);

  // kOriginTrialsSampleAPIDeprecation is not a navigation feature, so shouldn't
  // be included in GetEnabledNavigationFeatures().
  EXPECT_EQ(nullptr, GetEnabledNavigationFeatures());
}

// ... but if the browser says it's invalid for any reason, that's enough to
// reject.
TEST_F(OriginTrialContextTest, InvalidTokenResponseFromPlatform) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kMalformed,
                                kFrobulateTrialName);
  bool is_origin_enabled = IsFeatureEnabled(
      kFrobulateEnabledOrigin, OriginTrialFeature::kOriginTrialsSampleAPI);
  EXPECT_FALSE(is_origin_enabled);
  EXPECT_EQ(1, TokenValidator()->CallCount());

  // Status metric should be updated.
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kMalformed, 1);
}

// The feature should not be enabled if the origin is insecure, even if a valid
// token for the origin is provided
TEST_F(OriginTrialContextTest, EnabledNonSecureRegisteredOrigin) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateTrialName);
  bool is_origin_enabled =
      IsFeatureEnabled(kFrobulateEnabledOriginUnsecure,
                       OriginTrialFeature::kOriginTrialsSampleAPI);
  EXPECT_FALSE(is_origin_enabled);
  EXPECT_EQ(1, TokenValidator()->CallCount());
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kInsecure, 1);
}

// The feature should be enabled if the origin is insecure, for a valid token
// for a deprecation trial.
TEST_F(OriginTrialContextTest,
       EnabledNonSecureRegisteredOriginDeprecationWithToken) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateDeprecationTrialName);
  bool is_origin_enabled =
      IsFeatureEnabled(kFrobulateEnabledOriginUnsecure,
                       OriginTrialFeature::kOriginTrialsSampleAPIDeprecation);
  EXPECT_TRUE(is_origin_enabled);
  EXPECT_EQ(1, TokenValidator()->CallCount());
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kSuccess, 1);
}

// The feature should not be enabled if the origin is insecure, without a valid
// token for a deprecation trial.
TEST_F(OriginTrialContextTest,
       EnabledNonSecureRegisteredOriginDeprecationNoToken) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateTrialName);
  bool is_origin_enabled =
      IsFeatureEnabled(kFrobulateEnabledOriginUnsecure,
                       OriginTrialFeature::kOriginTrialsSampleAPIDeprecation);
  EXPECT_FALSE(is_origin_enabled);
}

// The feature should not be enabled if token is valid and enabled for third
// party origin but trial is not enabled for third party origin.
TEST_F(OriginTrialContextTest, EnabledNonThirdPartyTrialWithThirdPartyToken) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateTrialName, base::Time(), true);
  bool is_origin_enabled = IsFeatureEnabledForThirdPartyOrigin(
      kFrobulateEnabledOrigin, kFrobulateEnabledOrigin,
      OriginTrialFeature::kOriginTrialsSampleAPI);
  EXPECT_FALSE(is_origin_enabled);
  EXPECT_EQ(1, TokenValidator()->CallCount());
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kFeatureDisabled, 1);
}

// The feature should not be enabled if token is enabled for third
// party origin but it's not injected by external script.
TEST_F(OriginTrialContextTest, ThirdPartyTokenNotFromExternalScript) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kWrongOrigin,
                                kFrobulateThirdPartyTrialName, base::Time(),
                                true);
  bool is_origin_enabled = IsFeatureEnabledForThirdPartyOrigin(
      kFrobulateEnabledOrigin, kFrobulateEnabledOrigin,
      OriginTrialFeature::kOriginTrialsSampleAPIThirdParty);
  EXPECT_FALSE(is_origin_enabled);
  EXPECT_EQ(1, TokenValidator()->CallCount());
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kWrongOrigin, 1);
}

// The feature should not be enabled if token is injected from insecure external
// script even if document origin is secure.
TEST_F(OriginTrialContextTest, ThirdPartyTokenFromInsecureExternalScript) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateThirdPartyTrialName, base::Time(),
                                true);
  bool is_origin_enabled = IsFeatureEnabledForThirdPartyOrigin(
      kFrobulateEnabledOrigin, kFrobulateEnabledOriginUnsecure,
      OriginTrialFeature::kOriginTrialsSampleAPIThirdParty);
  EXPECT_FALSE(is_origin_enabled);
  EXPECT_EQ(1, TokenValidator()->CallCount());
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kInsecure, 1);
}

// The feature should not be enabled if token is injected from insecure external
// script when the document origin is also insecure.
TEST_F(OriginTrialContextTest,
       ThirdPartyTokenFromInsecureExternalScriptOnInsecureDocument) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateThirdPartyTrialName, base::Time(),
                                true);
  bool is_origin_enabled = IsFeatureEnabledForThirdPartyOrigin(
      kFrobulateEnabledOriginUnsecure, kFrobulateEnabledOriginUnsecure,
      OriginTrialFeature::kOriginTrialsSampleAPIThirdParty);
  EXPECT_FALSE(is_origin_enabled);
  EXPECT_EQ(1, TokenValidator()->CallCount());
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kInsecure, 1);
}

// The feature should not be enabled if token is injected from secure external
// script when the document is insecure.
TEST_F(OriginTrialContextTest, ThirdPartyTokenOnInsecureDocument) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateThirdPartyTrialName, base::Time(),
                                true);
  bool is_origin_enabled = IsFeatureEnabledForThirdPartyOrigin(
      kFrobulateEnabledOriginUnsecure, kFrobulateEnabledOrigin,
      OriginTrialFeature::kOriginTrialsSampleAPIThirdParty);
  EXPECT_FALSE(is_origin_enabled);
  EXPECT_EQ(1, TokenValidator()->CallCount());
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kInsecure, 1);
}

// The feature should be enabled if 1) token is valid for third party origin
// 2) token is enabled for third party origin and 3) trial is enabled for
// third party origin.
TEST_F(OriginTrialContextTest, EnabledThirdPartyTrialWithThirdPartyToken) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateThirdPartyTrialName, base::Time(),
                                true);
  bool is_origin_enabled = IsFeatureEnabledForThirdPartyOrigin(
      kFrobulateEnabledOrigin, kFrobulateEnabledOrigin,
      OriginTrialFeature::kOriginTrialsSampleAPIThirdParty);
  EXPECT_TRUE(is_origin_enabled);
  EXPECT_EQ(1, TokenValidator()->CallCount());
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kSuccess, 1);
}

TEST_F(OriginTrialContextTest, ParseHeaderValue) {
  std::unique_ptr<Vector<String>> tokens;
  ASSERT_TRUE(tokens = OriginTrialContext::ParseHeaderValue(" foo\t "));
  ASSERT_EQ(1u, tokens->size());
  EXPECT_EQ("foo", (*tokens)[0]);

  ASSERT_TRUE(tokens = OriginTrialContext::ParseHeaderValue(" \" bar \" "));
  ASSERT_EQ(1u, tokens->size());
  EXPECT_EQ(" bar ", (*tokens)[0]);

  ASSERT_TRUE(tokens = OriginTrialContext::ParseHeaderValue(" foo, bar"));
  ASSERT_EQ(2u, tokens->size());
  EXPECT_EQ("foo", (*tokens)[0]);
  EXPECT_EQ("bar", (*tokens)[1]);

  ASSERT_TRUE(tokens =
                  OriginTrialContext::ParseHeaderValue(",foo, ,bar,,'  ', ''"));
  ASSERT_EQ(3u, tokens->size());
  EXPECT_EQ("foo", (*tokens)[0]);
  EXPECT_EQ("bar", (*tokens)[1]);
  EXPECT_EQ("  ", (*tokens)[2]);

  ASSERT_TRUE(tokens =
                  OriginTrialContext::ParseHeaderValue("  \"abc\"  , 'def',g"));
  ASSERT_EQ(3u, tokens->size());
  EXPECT_EQ("abc", (*tokens)[0]);
  EXPECT_EQ("def", (*tokens)[1]);
  EXPECT_EQ("g", (*tokens)[2]);

  ASSERT_TRUE(tokens = OriginTrialContext::ParseHeaderValue(
                  " \"a\\b\\\"c'd\", 'e\\f\\'g' "));
  ASSERT_EQ(2u, tokens->size());
  EXPECT_EQ("ab\"c'd", (*tokens)[0]);
  EXPECT_EQ("ef'g", (*tokens)[1]);

  ASSERT_TRUE(tokens =
                  OriginTrialContext::ParseHeaderValue("\"ab,c\" , 'd,e'"));
  ASSERT_EQ(2u, tokens->size());
  EXPECT_EQ("ab,c", (*tokens)[0]);
  EXPECT_EQ("d,e", (*tokens)[1]);

  ASSERT_TRUE(tokens = OriginTrialContext::ParseHeaderValue("  "));
  EXPECT_EQ(0u, tokens->size());

  ASSERT_TRUE(tokens = OriginTrialContext::ParseHeaderValue(""));
  EXPECT_EQ(0u, tokens->size());

  ASSERT_TRUE(tokens = OriginTrialContext::ParseHeaderValue(" ,, \"\" "));
  EXPECT_EQ(0u, tokens->size());
}

TEST_F(OriginTrialContextTest, ParseHeaderValue_NotCommaSeparated) {
  EXPECT_FALSE(OriginTrialContext::ParseHeaderValue("foo bar"));
  EXPECT_FALSE(OriginTrialContext::ParseHeaderValue("\"foo\" 'bar'"));
  EXPECT_FALSE(OriginTrialContext::ParseHeaderValue("foo 'bar'"));
  EXPECT_FALSE(OriginTrialContext::ParseHeaderValue("\"foo\" bar"));
}

TEST_F(OriginTrialContextTest, FeaturePolicy) {
  // Create a dummy window/document with an OriginTrialContext.
  auto dummy = std::make_unique<DummyPageHolder>();
  LocalDOMWindow* window = dummy->GetFrame().DomWindow();
  OriginTrialContext* context = window->GetOriginTrialContext();

  // Enable the sample origin trial API ("Frobulate").
  context->AddFeature(OriginTrialFeature::kOriginTrialsSampleAPI);
  EXPECT_TRUE(
      context->IsFeatureEnabled(OriginTrialFeature::kOriginTrialsSampleAPI));

  // Make a mock feature name map with "frobulate".
  FeatureNameMap feature_map;
  feature_map.Set("frobulate", mojom::blink::FeaturePolicyFeature::kFrobulate);

  // Attempt to parse the "frobulate" feature policy. This will only work if the
  // feature policy is successfully enabled via the origin trial.
  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::CreateFromString(kFrobulateEnabledOrigin);

  PolicyParserMessageBuffer logger;
  ParsedFeaturePolicy result;
  result = FeaturePolicyParser::ParseFeaturePolicyForTest(
      "frobulate", security_origin, nullptr, logger, feature_map, window);
  EXPECT_TRUE(logger.GetMessages().IsEmpty());
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(mojom::blink::FeaturePolicyFeature::kFrobulate, result[0].feature);
}

TEST_F(OriginTrialContextTest, GetEnabledNavigationFeatures) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateNavigationTrialName);
  EXPECT_TRUE(
      IsFeatureEnabled(kFrobulateEnabledOrigin,
                       OriginTrialFeature::kOriginTrialsSampleAPINavigation));

  auto enabled_navigation_features = GetEnabledNavigationFeatures();
  ASSERT_NE(nullptr, enabled_navigation_features.get());
  EXPECT_EQ(WTF::Vector<OriginTrialFeature>(
                {OriginTrialFeature::kOriginTrialsSampleAPINavigation}),
            *enabled_navigation_features.get());
}

TEST_F(OriginTrialContextTest, ActivateNavigationFeature) {
  EXPECT_TRUE(ActivateNavigationFeature(
      OriginTrialFeature::kOriginTrialsSampleAPINavigation));
  EXPECT_FALSE(
      ActivateNavigationFeature(OriginTrialFeature::kOriginTrialsSampleAPI));
}

TEST_F(OriginTrialContextTest, GetTokenExpiryTimeIgnoresIrrelevantTokens) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  base::Time nowish = base::Time::Now();
  // A non-success response shouldn't affect Frobulate's expiry time.
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kMalformed,
                                kFrobulateTrialName,
                                nowish + base::TimeDelta::FromDays(2));
  EXPECT_FALSE(IsFeatureEnabled(OriginTrialFeature::kOriginTrialsSampleAPI));
  EXPECT_EQ(base::Time(),
            GetFeatureExpiry(OriginTrialFeature::kOriginTrialsSampleAPI));

  // A different trial shouldn't affect Frobulate's expiry time.
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateDeprecationTrialName,
                                nowish + base::TimeDelta::FromDays(3));
  EXPECT_TRUE(
      IsFeatureEnabled(OriginTrialFeature::kOriginTrialsSampleAPIDeprecation));
  EXPECT_EQ(base::Time(),
            GetFeatureExpiry(OriginTrialFeature::kOriginTrialsSampleAPI));

  // A valid trial should update the expiry time.
  base::Time expected_expiry = nowish + base::TimeDelta::FromDays(1);
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateTrialName, expected_expiry);
  EXPECT_TRUE(IsFeatureEnabled(OriginTrialFeature::kOriginTrialsSampleAPI));
  EXPECT_EQ(expected_expiry,
            GetFeatureExpiry(OriginTrialFeature::kOriginTrialsSampleAPI));
}

TEST_F(OriginTrialContextTest, LastExpiryForFeatureIsUsed) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  base::Time plusone = base::Time::Now() + base::TimeDelta::FromDays(1);
  base::Time plustwo = plusone + base::TimeDelta::FromDays(1);
  base::Time plusthree = plustwo + base::TimeDelta::FromDays(1);

  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateTrialName, plusone);
  EXPECT_TRUE(IsFeatureEnabled(OriginTrialFeature::kOriginTrialsSampleAPI));
  EXPECT_EQ(plusone,
            GetFeatureExpiry(OriginTrialFeature::kOriginTrialsSampleAPI));

  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateTrialName, plusthree);
  EXPECT_TRUE(IsFeatureEnabled(OriginTrialFeature::kOriginTrialsSampleAPI));
  EXPECT_EQ(plusthree,
            GetFeatureExpiry(OriginTrialFeature::kOriginTrialsSampleAPI));

  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateTrialName, plustwo);
  EXPECT_TRUE(IsFeatureEnabled(OriginTrialFeature::kOriginTrialsSampleAPI));
  EXPECT_EQ(plusthree,
            GetFeatureExpiry(OriginTrialFeature::kOriginTrialsSampleAPI));
}

TEST_F(OriginTrialContextTest, ImpliedFeatureExpiryTimesAreUpdated) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  base::Time tomorrow = base::Time::Now() + base::TimeDelta::FromDays(1);
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateTrialName, tomorrow);
  EXPECT_TRUE(IsFeatureEnabled(OriginTrialFeature::kOriginTrialsSampleAPI));
  EXPECT_EQ(tomorrow, GetFeatureExpiry(
                          OriginTrialFeature::kOriginTrialsSampleAPIImplied));
}

}  // namespace blink
