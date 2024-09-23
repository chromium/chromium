// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"

#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/origin_trials.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_result.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace {

const char kUnknownTrialName[] = "UnknownTrial";
const char kFrobulateTrialName[] = "Frobulate";
const char kFrobulateThirdPartyTrialName[] = "FrobulateThirdParty";
const char kFrobulateNavigationTrialName[] = "FrobulateNavigation";
const char kFrobulateDeprecationTrialName[] = "FrobulateDeprecation";
const char kFrobulateBrowserReadWriteTrialName[] = "FrobulateBrowserReadWrite";
const char kFrobulateEnabledOrigin[] = "https://www.example.com";
const char kFrobulateEnabledOriginInsecure[] = "http://www.example.com";
const char kUnrelatedSecureOrigin[] = "https://other.example.com";

// The tokens expire in 2033.
const base::Time kBaseTokenExpiryTime = base::Time::FromTimeT(2000000000);

// Trial token placeholder for mocked calls to validator
const char kTokenPlaceholder[] = "The token contents are not used";

// Since all of trial token validation is tested elsewhere,
// this mock lets us test the context in isolation and assert
// that correct parameters are passed to the validator
// without having to generate a large number of valid tokens
class MockTokenValidator : public TrialTokenValidator {
 public:
  struct MockResponse {
    OriginTrialTokenStatus status = OriginTrialTokenStatus::kNotSupported;
    std::string feature;
    url::Origin origin;
    base::Time expiry = kBaseTokenExpiryTime;
  };

  struct ValidationParams {
    const std::string token;
    const OriginInfo origin;
    Vector<OriginInfo> third_party_origin_info;
    const base::Time current_time;
    ValidationParams(std::string_view token_param,
                     const OriginInfo& origin_info,
                     base::span<const OriginInfo> scripts,
                     base::Time time)
        : token(token_param), origin(origin_info), current_time(time) {
      third_party_origin_info.AppendRange(scripts.begin(), scripts.end());
    }
  };

  MockTokenValidator() = default;
  MockTokenValidator(const MockTokenValidator&) = delete;
  MockTokenValidator& operator=(const MockTokenValidator&) = delete;
  ~MockTokenValidator() override = default;

  TrialTokenResult ValidateTokenAndTrialWithOriginInfo(
      std::string_view token,
      const OriginInfo& origin,
      base::span<const OriginInfo> third_party_origin_info,
      base::Time current_time) const override {
    validation_params_.emplace_back(token, origin, third_party_origin_info,
                                    current_time);
    if (response_.status == OriginTrialTokenStatus::kMalformed) {
      return TrialTokenResult(response_.status);
    } else {
      return TrialTokenResult(
          response_.status,
          TrialToken::CreateTrialTokenForTesting(
              origin.origin, false, response_.feature, response_.expiry, false,
              TrialToken::UsageRestriction::kNone, ""));
    }
  }

  void SetResponse(MockResponse response) { response_ = response; }

  Vector<ValidationParams> GetValidationParams() const {
    return validation_params_;
  }

 private:
  MockResponse response_;
  mutable Vector<ValidationParams> validation_params_;
};

}  // namespace

class OriginTrialContextTest : public testing::Test {
 protected:
  OriginTrialContextTest()
      : token_validator_(new MockTokenValidator()),
        execution_context_(MakeGarbageCollected<NullExecutionContext>()) {
    execution_context_->GetOriginTrialContext()
        ->SetTrialTokenValidatorForTesting(
            std::unique_ptr<MockTokenValidator>(token_validator_));
  }
  ~OriginTrialContextTest() override {
    execution_context_->NotifyContextDestroyed();
    // token_validator_ is deleted by the unique_ptr handed to the
    // OriginTrialContext
  }

  void UpdateSecurityOrigin(const String& origin) {
    KURL page_url(origin);
    scoped_refptr<SecurityOrigin> page_origin =
        SecurityOrigin::Create(page_url);
    execution_context_->GetSecurityContext().SetSecurityOrigin(page_origin);
  }

  void AddTokenWithResponse(MockTokenValidator::MockResponse response) {
    token_validator_->SetResponse(std::move(response));
    execution_context_->GetOriginTrialContext()->AddToken(kTokenPlaceholder);
  }

  void AddTokenWithResponse(const std::string& trial_name,
                            OriginTrialTokenStatus validation_status) {
    AddTokenWithResponse({.status = validation_status, .feature = trial_name});
  }

  void AddTokenForThirdPartyOriginsWithResponse(
      const std::string& trial_name,
      OriginTrialTokenStatus validation_status,
      const Vector<String>& script_origins) {
    token_validator_->SetResponse(
        {.status = validation_status, .feature = trial_name});
    Vector<scoped_refptr<SecurityOrigin>> external_origins;
    for (const auto& script_origin : script_origins) {
      KURL script_url(script_origin);
      external_origins.emplace_back(SecurityOrigin::Create(script_url));
    };
    execution_context_->GetOriginTrialContext()->AddTokenFromExternalScript(
        kTokenPlaceholder, external_origins);
  }

  bool IsFeatureEnabled(mojom::blink::OriginTrialFeature feature) {
    return execution_context_->GetOriginTrialContext()->IsFeatureEnabled(
        feature);
  }

  base::Time GetFeatureExpiry(mojom::blink::OriginTrialFeature feature) {
    return execution_context_->GetOriginTrialContext()->GetFeatureExpiry(
        feature);
  }

  std::unique_ptr<Vector<mojom::blink::OriginTrialFeature>>
  GetEnabledNavigationFeatures() {
    return execution_context_->GetOriginTrialContext()
        ->GetEnabledNavigationFeatures();
  }

  HashMap<mojom::blink::OriginTrialFeature, Vector<String>>
  GetFeatureToTokens() {
    return execution_context_->GetOriginTrialContext()
        ->GetFeatureToTokensForTesting();
  }

  bool ActivateNavigationFeature(mojom::blink::OriginTrialFeature feature) {
    execution_context_->GetOriginTrialContext()
        ->ActivateNavigationFeaturesFromInitiator({feature});
    return execution_context_->GetOriginTrialContext()
        ->IsNavigationFeatureActivated(feature);
  }

 protected:
  test::TaskEnvironment task_environment_;
  MockTokenValidator* token_validator_;
  Persistent<NullExecutionContext> execution_context_;
};

// Test that we're passing correct information to the validator
TEST_F(OriginTrialContextTest, ValidatorGetsCorrectInfo) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  AddTokenWithResponse(kFrobulateTrialName, OriginTrialTokenStatus::kSuccess);

  Vector<MockTokenValidator::ValidationParams> validation_params =
      token_validator_->GetValidationParams();
  ASSERT_EQ(1ul, validation_params.size());
  EXPECT_EQ(url::Origin::Create(GURL(kFrobulateEnabledOrigin)),
            validation_params[0].origin.origin);
  EXPECT_TRUE(validation_params[0].origin.is_secure);
  EXPECT_TRUE(validation_params[0].third_party_origin_info.empty());

  // Check that the "expected" token is passed to the validator
  EXPECT_EQ(kTokenPlaceholder, validation_params[0].token);

  // Check that the passed current_time to the validator was within a reasonable
  // bound (+-5 minutes) of the current time, since the context is passing
  // base::Time::Now() when it calls the function.
  ASSERT_LT(base::Time::Now() - validation_params[0].current_time,
            base::Minutes(5));
  ASSERT_LT(validation_params[0].current_time - base::Time::Now(),
            base::Minutes(5));
}

// Test that we're passing correct security information to the validator
TEST_F(OriginTrialContextTest,
       ValidatorGetsCorrectSecurityInfoForInsecureOrigins) {
  UpdateSecurityOrigin(kFrobulateEnabledOriginInsecure);

  AddTokenWithResponse(kFrobulateTrialName, OriginTrialTokenStatus::kInsecure);

  Vector<MockTokenValidator::ValidationParams> validation_params =
      token_validator_->GetValidationParams();
  ASSERT_EQ(1ul, validation_params.size());
  EXPECT_EQ(url::Origin::Create(GURL(kFrobulateEnabledOriginInsecure)),
            validation_params[0].origin.origin);
  EXPECT_FALSE(validation_params[0].origin.is_secure);
  EXPECT_TRUE(validation_params[0].third_party_origin_info.empty());
}

// Test that we're passing correct security information to the validator
TEST_F(OriginTrialContextTest, ValidatorGetsCorrectSecurityInfoThirdParty) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  AddTokenForThirdPartyOriginsWithResponse(
      kFrobulateThirdPartyTrialName, OriginTrialTokenStatus::kInsecure,
      {kUnrelatedSecureOrigin, kFrobulateEnabledOriginInsecure});

  Vector<MockTokenValidator::ValidationParams> validation_params =
      token_validator_->GetValidationParams();
  ASSERT_EQ(1ul, validation_params.size());
  EXPECT_EQ(url::Origin::Create(GURL(kFrobulateEnabledOrigin)),
            validation_params[0].origin.origin);
  EXPECT_TRUE(validation_params[0].origin.is_secure);

  EXPECT_EQ(2ul, validation_params[0].third_party_origin_info.size());
  auto unrelated_info = base::ranges::find_if(
      validation_params[0].third_party_origin_info,
      [](const TrialTokenValidator::OriginInfo& item) {
        return item.origin.IsSameOriginWith(GURL(kUnrelatedSecureOrigin));
      });
  ASSERT_NE(validation_params[0].third_party_origin_info.end(), unrelated_info);
  EXPECT_TRUE(unrelated_info->is_secure);

  auto insecure_origin_info =
      base::ranges::find_if(validation_params[0].third_party_origin_info,
                            [](const TrialTokenValidator::OriginInfo& item) {
                              return item.origin.IsSameOriginWith(
                                  GURL(kFrobulateEnabledOriginInsecure));
                            });
  ASSERT_NE(validation_params[0].third_party_origin_info.end(),
            insecure_origin_info);
  EXPECT_FALSE(insecure_origin_info->is_secure);
}

// Test that unrelated features are not enabled
TEST_F(OriginTrialContextTest, EnabledNonExistingTrial) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  AddTokenWithResponse(kFrobulateTrialName, OriginTrialTokenStatus::kSuccess);

  bool is_non_existing_feature_enabled =
      IsFeatureEnabled(mojom::blink::OriginTrialFeature::kNonExisting);
  EXPECT_FALSE(is_non_existing_feature_enabled);
}

// The feature should be enabled if a valid token for the origin is provided
TEST_F(OriginTrialContextTest, EnabledSecureRegisteredOrigin) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  AddTokenWithResponse(kFrobulateTrialName, OriginTrialTokenStatus::kSuccess);
  bool is_origin_enabled = IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI);
  EXPECT_TRUE(is_origin_enabled);

  // kOriginTrialsSampleAPI is not a navigation feature, so shouldn't be
  // included in GetEnabledNavigationFeatures().
  EXPECT_EQ(nullptr, GetEnabledNavigationFeatures());
}

// The feature should be enabled when all of:
// 1) token is valid for third party origin
// 2) token is enabled for secure, third party origin
// 3) trial allows third party origins
TEST_F(OriginTrialContextTest, ThirdPartyTrialWithThirdPartyTokenEnabled) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);
  AddTokenForThirdPartyOriginsWithResponse(kFrobulateThirdPartyTrialName,
                                           OriginTrialTokenStatus::kSuccess,
                                           {kFrobulateEnabledOrigin});
  bool is_origin_enabled = IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPIThirdParty);
  EXPECT_TRUE(is_origin_enabled);
}

// If the browser says it's invalid for any reason, that's enough to reject.
TEST_F(OriginTrialContextTest, InvalidTokenResponseFromPlatform) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);
  AddTokenWithResponse(kFrobulateTrialName,
                       OriginTrialTokenStatus::kInvalidSignature);

  bool is_origin_enabled = IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI);
  EXPECT_FALSE(is_origin_enabled);
}

// Features should not be enabled on insecure origins
TEST_F(OriginTrialContextTest, FeatureNotEnableOnInsecureOrigin) {
  UpdateSecurityOrigin(kFrobulateEnabledOriginInsecure);
  AddTokenWithResponse(kFrobulateTrialName, OriginTrialTokenStatus::kInsecure);
  EXPECT_FALSE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));
}

// Features should not be enabled on insecure third-party origins
TEST_F(OriginTrialContextTest, FeatureNotEnableOnInsecureThirdPartyOrigin) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);
  AddTokenForThirdPartyOriginsWithResponse(kFrobulateThirdPartyTrialName,
                                           OriginTrialTokenStatus::kInsecure,
                                           {kFrobulateEnabledOriginInsecure});
  EXPECT_FALSE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPIThirdParty));
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

TEST_F(OriginTrialContextTest, PermissionsPolicy) {
  // Create a page holder window/document with an OriginTrialContext.
  auto page_holder = std::make_unique<DummyPageHolder>();
  LocalDOMWindow* window = page_holder->GetFrame().DomWindow();
  OriginTrialContext* context = window->GetOriginTrialContext();

  // Enable the sample origin trial API ("Frobulate").
  context->AddFeature(mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI);
  EXPECT_TRUE(context->IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));

  // Make a mock feature name map with "frobulate".
  FeatureNameMap feature_map;
  feature_map.Set("frobulate",
                  mojom::blink::PermissionsPolicyFeature::kFrobulate);

  // Attempt to parse the "frobulate" permissions policy. This will only work if
  // the permissions policy is successfully enabled via the origin trial.
  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::CreateFromString(kFrobulateEnabledOrigin);

  PolicyParserMessageBuffer logger;
  ParsedPermissionsPolicy result;
  result = PermissionsPolicyParser::ParsePermissionsPolicyForTest(
      "frobulate=*", security_origin, nullptr, logger, feature_map, window);
  EXPECT_TRUE(logger.GetMessages().empty());
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(mojom::blink::PermissionsPolicyFeature::kFrobulate,
            result[0].feature);
}

TEST_F(OriginTrialContextTest, GetEnabledNavigationFeatures) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);
  AddTokenWithResponse(kFrobulateNavigationTrialName,
                       OriginTrialTokenStatus::kSuccess);
  EXPECT_TRUE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPINavigation));

  auto enabled_navigation_features = GetEnabledNavigationFeatures();
  ASSERT_NE(nullptr, enabled_navigation_features.get());
  EXPECT_EQ(
      WTF::Vector<mojom::blink::OriginTrialFeature>(
          {mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPINavigation}),
      *enabled_navigation_features.get());
}

TEST_F(OriginTrialContextTest, ActivateNavigationFeature) {
  EXPECT_TRUE(ActivateNavigationFeature(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPINavigation));
  EXPECT_FALSE(ActivateNavigationFeature(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));
}

TEST_F(OriginTrialContextTest, GetTokenExpiryTimeIgnoresIrrelevantTokens) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  // A non-success response shouldn't affect Frobulate's expiry time.
  AddTokenWithResponse(kUnknownTrialName, OriginTrialTokenStatus::kMalformed);
  EXPECT_FALSE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));
  EXPECT_EQ(base::Time(),
            GetFeatureExpiry(
                mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));

  // A different trial shouldn't affect Frobulate's expiry time.
  AddTokenWithResponse(kFrobulateDeprecationTrialName,
                       OriginTrialTokenStatus::kSuccess);
  EXPECT_TRUE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPIDeprecation));
  EXPECT_EQ(base::Time(),
            GetFeatureExpiry(
                mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));

  // A valid trial should update the expiry time.
  AddTokenWithResponse(kFrobulateTrialName, OriginTrialTokenStatus::kSuccess);
  EXPECT_TRUE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));
  EXPECT_EQ(kBaseTokenExpiryTime,
            GetFeatureExpiry(
                mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));
}

TEST_F(OriginTrialContextTest, LastExpiryForFeatureIsUsed) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  base::Time plusone = kBaseTokenExpiryTime + base::Seconds(1);
  base::Time plustwo = plusone + base::Seconds(1);
  base::Time plusthree = plustwo + base::Seconds(1);

  AddTokenWithResponse({
      .status = OriginTrialTokenStatus::kSuccess,
      .feature = kFrobulateTrialName,
      .expiry = plusone,
  });
  EXPECT_TRUE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));
  EXPECT_EQ(plusone,
            GetFeatureExpiry(
                mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));

  AddTokenWithResponse({
      .status = OriginTrialTokenStatus::kSuccess,
      .feature = kFrobulateTrialName,
      .expiry = plusthree,
  });
  EXPECT_TRUE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));
  EXPECT_EQ(plusthree,
            GetFeatureExpiry(
                mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));

  AddTokenWithResponse({
      .status = OriginTrialTokenStatus::kSuccess,
      .feature = kFrobulateTrialName,
      .expiry = plustwo,
  });
  EXPECT_TRUE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));
  EXPECT_EQ(plusthree,
            GetFeatureExpiry(
                mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));
}

TEST_F(OriginTrialContextTest, ImpliedFeatureExpiryTimesAreUpdated) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  base::Time plusone = kBaseTokenExpiryTime + base::Seconds(1);
  AddTokenWithResponse({
      .status = OriginTrialTokenStatus::kSuccess,
      .feature = kFrobulateTrialName,
      .expiry = plusone,
  });
  EXPECT_TRUE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));
  EXPECT_EQ(
      plusone,
      GetFeatureExpiry(
          mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPIImplied));
}

TEST_F(OriginTrialContextTest, SettingFeatureUpdatesDocumentSettings) {
  // Create a page holder window/document with an OriginTrialContext.
  auto page_holder = std::make_unique<DummyPageHolder>();
  LocalDOMWindow* window = page_holder->GetFrame().DomWindow();
  OriginTrialContext* context = window->GetOriginTrialContext();

  // Force-disabled the AutoDarkMode feature in the page holder's settings.
  ASSERT_TRUE(page_holder->GetDocument().GetSettings());
  page_holder->GetDocument().GetSettings()->SetForceDarkModeEnabled(false);

  // Enable a settings-based origin trial API ("AutoDarkMode").
  context->AddFeature(mojom::blink::OriginTrialFeature::kAutoDarkMode);
  EXPECT_TRUE(context->IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kAutoDarkMode));

  // Expect the AutoDarkMode setting to have been enabled.
  EXPECT_TRUE(
      page_holder->GetDocument().GetSettings()->GetForceDarkModeEnabled());

  // TODO(crbug.com/1260410): Switch this test away from using the AutoDarkMode
  // feature towards an OriginTrialsSampleAPI* feature.
}

// This test ensures that the feature and token data are correctly mapped. The
// assertions mirror the code that is used to send origin trial overrides to the
// browser process via RuntimeFeatureStateOverrideContext's IPC.
TEST_F(OriginTrialContextTest, AddedFeaturesAreMappedToTokens) {
  // Add a new feature via token.
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);
  AddTokenWithResponse(kFrobulateBrowserReadWriteTrialName,
                       OriginTrialTokenStatus::kSuccess);
  // Ensure that FrobulateBrowserReadWrite is enabled.
  EXPECT_TRUE(IsFeatureEnabled(mojom::blink::OriginTrialFeature::
                                   kOriginTrialsSampleAPIBrowserReadWrite));
  EXPECT_TRUE(GetFeatureToTokens().Contains(
      mojom::blink::OriginTrialFeature::
          kOriginTrialsSampleAPIBrowserReadWrite));
  // Ensure that the corresponding token is stored.
  Vector<String> expected_tokens({kTokenPlaceholder});
  EXPECT_EQ(GetFeatureToTokens().at(mojom::blink::OriginTrialFeature::
                                        kOriginTrialsSampleAPIBrowserReadWrite),
            expected_tokens);
}

class OriginTrialContextDevtoolsTest : public OriginTrialContextTest {
 public:
  OriginTrialContextDevtoolsTest() = default;

  const HashMap<String, OriginTrialResult> GetOriginTrialResultsForDevtools()
      const {
    return execution_context_->GetOriginTrialContext()
        ->GetOriginTrialResultsForDevtools();
  }

  struct ExpectedOriginTrialTokenResult {
    OriginTrialTokenStatus status;
    bool token_parsable;
  };

  void ExpectTrialResultContains(
      const HashMap<String, OriginTrialResult>& trial_results,
      const String& trial_name,
      OriginTrialStatus trial_status,
      const Vector<ExpectedOriginTrialTokenResult>& expected_token_results)
      const {
    auto trial_result = trial_results.find(trial_name);
    ASSERT_TRUE(trial_result != trial_results.end());
    EXPECT_EQ(trial_result->value.trial_name, trial_name);
    EXPECT_EQ(trial_result->value.status, trial_status);
    EXPECT_EQ(trial_result->value.token_results.size(),
              expected_token_results.size());

    for (wtf_size_t i = 0; i < expected_token_results.size(); i++) {
      const auto& expected_token_result = expected_token_results[i];
      const auto& actual_token_result = trial_result->value.token_results[i];

      // Note: `OriginTrialTokenResult::raw_token` is not checked
      // as the mocking class uses `kTokenPlaceholder` as raw token string.
      // Further content of `OriginTrialTokenResult::raw_token` is
      // also not checked, as it is generated by the mocking class.
      EXPECT_EQ(actual_token_result.status, expected_token_result.status);
      EXPECT_EQ(actual_token_result.parsed_token.has_value(),
                expected_token_result.token_parsable);
      EXPECT_NE(actual_token_result.raw_token, g_empty_string);
    }
  }
};

TEST_F(OriginTrialContextDevtoolsTest, DependentFeatureNotEnabled) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitAndDisableFeature(
      blink::features::kSpeculationRulesPrefetchFuture);

  AddTokenWithResponse("SpeculationRulesPrefetchFuture",
                       OriginTrialTokenStatus::kSuccess);

  EXPECT_FALSE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kSpeculationRulesPrefetchFuture));
  HashMap<String, OriginTrialResult> origin_trial_results =
      GetOriginTrialResultsForDevtools();
  EXPECT_EQ(origin_trial_results.size(), 1u);
  ExpectTrialResultContains(
      origin_trial_results, "SpeculationRulesPrefetchFuture",
      OriginTrialStatus::kTrialNotAllowed,
      {{OriginTrialTokenStatus::kSuccess, /* token_parsable */ true}});
}

TEST_F(OriginTrialContextDevtoolsTest, TrialNameNotRecognized) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  AddTokenWithResponse(kUnknownTrialName,
                       OriginTrialTokenStatus::kUnknownTrial);

  EXPECT_FALSE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));

  HashMap<String, OriginTrialResult> origin_trial_results =
      GetOriginTrialResultsForDevtools();

  EXPECT_EQ(origin_trial_results.size(), 1u);
  ExpectTrialResultContains(
      origin_trial_results,
      /* trial_name */ "UNKNOWN", OriginTrialStatus::kValidTokenNotProvided,
      {{OriginTrialTokenStatus::kUnknownTrial, /* token_parsable */ true}});
}

TEST_F(OriginTrialContextDevtoolsTest, NoValidToken) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  AddTokenWithResponse(kFrobulateTrialName, OriginTrialTokenStatus::kExpired);

  EXPECT_FALSE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));

  HashMap<String, OriginTrialResult> origin_trial_results =
      GetOriginTrialResultsForDevtools();

  // Receiving invalid token should set feature status to
  // kValidTokenNotProvided.
  EXPECT_EQ(origin_trial_results.size(), 1u);
  ExpectTrialResultContains(
      origin_trial_results,
      /* trial_name */ kFrobulateTrialName,
      OriginTrialStatus::kValidTokenNotProvided,
      {{OriginTrialTokenStatus::kExpired, /* token_parsable */ true}});

  // Add a non-expired token
  AddTokenWithResponse(kFrobulateTrialName, OriginTrialTokenStatus::kSuccess);

  // Receiving valid token should change feature status to kEnabled.
  EXPECT_TRUE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));
  origin_trial_results = GetOriginTrialResultsForDevtools();
  EXPECT_EQ(origin_trial_results.size(), 1u);
  ExpectTrialResultContains(
      origin_trial_results,
      /* trial_name */ kFrobulateTrialName, OriginTrialStatus::kEnabled,
      {
          {OriginTrialTokenStatus::kExpired, /* token_parsable */ true},
          {OriginTrialTokenStatus::kSuccess, /* token_parsable */ true},
      });
}

TEST_F(OriginTrialContextDevtoolsTest, Enabled) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  AddTokenWithResponse(kFrobulateTrialName, OriginTrialTokenStatus::kSuccess);

  // Receiving valid token when feature is enabled should set feature status
  // to kEnabled.
  EXPECT_TRUE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));
  HashMap<String, OriginTrialResult> origin_trial_results =
      GetOriginTrialResultsForDevtools();
  EXPECT_EQ(origin_trial_results.size(), 1u);
  ExpectTrialResultContains(
      origin_trial_results,
      /* trial_name */ kFrobulateTrialName, OriginTrialStatus::kEnabled,
      {{OriginTrialTokenStatus::kSuccess, /* token_parsable */ true}});

  AddTokenWithResponse(kFrobulateTrialName, OriginTrialTokenStatus::kExpired);

  // Receiving invalid token when a valid token already exists should
  // not change feature status.
  EXPECT_TRUE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));
  origin_trial_results = GetOriginTrialResultsForDevtools();
  EXPECT_EQ(origin_trial_results.size(), 1u);
  ExpectTrialResultContains(
      origin_trial_results,
      /* trial_name */ kFrobulateTrialName, OriginTrialStatus::kEnabled,
      {
          {OriginTrialTokenStatus::kSuccess, /* token_parsable */ true},
          {OriginTrialTokenStatus::kExpired, /* token_parsable */ true},
      });
}

TEST_F(OriginTrialContextDevtoolsTest, UnparsableToken) {
  UpdateSecurityOrigin(kFrobulateEnabledOrigin);

  AddTokenWithResponse(kFrobulateTrialName, OriginTrialTokenStatus::kMalformed);

  EXPECT_FALSE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));
  HashMap<String, OriginTrialResult> origin_trial_results =
      GetOriginTrialResultsForDevtools();
  EXPECT_EQ(origin_trial_results.size(), 1u);
  ExpectTrialResultContains(
      origin_trial_results,
      /* trial_name */ "UNKNOWN", OriginTrialStatus::kValidTokenNotProvided,
      {{OriginTrialTokenStatus::kMalformed, /* token_parsable */ false}});
}

TEST_F(OriginTrialContextDevtoolsTest, InsecureOrigin) {
  UpdateSecurityOrigin(kFrobulateEnabledOriginInsecure);
  AddTokenWithResponse(kFrobulateTrialName, OriginTrialTokenStatus::kInsecure);

  EXPECT_FALSE(IsFeatureEnabled(
      mojom::blink::OriginTrialFeature::kOriginTrialsSampleAPI));

  HashMap<String, OriginTrialResult> origin_trial_results =
      GetOriginTrialResultsForDevtools();

  EXPECT_EQ(origin_trial_results.size(), 1u);
  ExpectTrialResultContains(
      origin_trial_results,
      /* trial_name */ kFrobulateTrialName,
      OriginTrialStatus::kValidTokenNotProvided,
      {{OriginTrialTokenStatus::kInsecure, /* token_parsable */ true}});
}

}  // namespace blink
