// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"

#include <map>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"

#include "url/gurl.h"
#include "url/origin.h"

// Origin strings used for tests
#define ORIGIN_A "https://example.com/"
#define ORIGIN_B "https://example.net/"
#define ORIGIN_C "https://example.org/"
#define OPAQUE_ORIGIN ""

// identifier used for feature/permissions policy parsing test.
// when there is a testcase for one syntax but not the other.
#define NOT_APPLICABLE nullptr

class GURL;

namespace blink {

namespace {

const char* const kValidPolicies[] = {
    "",      // An empty policy.
    " ",     // An empty policy.
    ";;",    // Empty policies.
    ",,",    // Empty policies.
    " ; ;",  // Empty policies.
    " , ,",  // Empty policies.
    ",;,",   // Empty policies.
    "geolocation 'none'",
    "geolocation 'self'",
    "geolocation 'src'",  // Only valid for iframe allow attribute.
    "geolocation",        // Only valid for iframe allow attribute.
    "geolocation; fullscreen; payment",
    "geolocation *",
    "geolocation " ORIGIN_A "",
    "geolocation " ORIGIN_B "",
    "geolocation  " ORIGIN_A " " ORIGIN_B "",
    "geolocation 'none' " ORIGIN_A " " ORIGIN_B "",
    "geolocation " ORIGIN_A " 'none' " ORIGIN_B "",
    "geolocation 'none' 'none' 'none'",
    "geolocation " ORIGIN_A " *",
    "fullscreen  " ORIGIN_A "; payment 'self'",
    "fullscreen " ORIGIN_A "; payment *, geolocation 'self'"};

const char* const kInvalidPolicies[] = {
    "badfeaturename",
    "badfeaturename 'self'",
    "1.0",
    "geolocation data://badorigin",
    "geolocation https://bad;origin",
    "geolocation https:/bad,origin",
    "geolocation https://example.com, https://a.com",
    "geolocation *, payment data://badorigin",
    "geolocation ws://xn--fd\xbcwsw3taaaaaBaa333aBBBBBBJBBJBBBt",
};

// Names of UMA histograms
const char kAllowlistAttributeHistogram[] =
    "Blink.UseCounter.FeaturePolicy.AttributeAllowlistType";
const char kAllowlistHeaderHistogram[] =
    "Blink.UseCounter.FeaturePolicy.HeaderAllowlistType";

}  // namespace

class FeaturePolicyParserTest : public ::testing::Test {
 protected:
  FeaturePolicyParserTest() = default;

  ~FeaturePolicyParserTest() override = default;

  scoped_refptr<const SecurityOrigin> origin_a_ =
      SecurityOrigin::CreateFromString(ORIGIN_A);
  scoped_refptr<const SecurityOrigin> origin_b_ =
      SecurityOrigin::CreateFromString(ORIGIN_B);
  scoped_refptr<const SecurityOrigin> origin_c_ =
      SecurityOrigin::CreateFromString(ORIGIN_C);

  url::Origin expected_url_origin_a_ = url::Origin::Create(GURL(ORIGIN_A));
  url::Origin expected_url_origin_b_ = url::Origin::Create(GURL(ORIGIN_B));
  url::Origin expected_url_origin_c_ = url::Origin::Create(GURL(ORIGIN_C));

  const FeatureNameMap test_feature_name_map = {
      {"fullscreen", blink::mojom::blink::FeaturePolicyFeature::kFullscreen},
      {"payment", blink::mojom::blink::FeaturePolicyFeature::kPayment},
      {"geolocation", blink::mojom::blink::FeaturePolicyFeature::kGeolocation}};

  ParsedFeaturePolicy ParseFeaturePolicyHeader(
      const String& feature_policy_header,
      scoped_refptr<const SecurityOrigin> origin,
      PolicyParserMessageBuffer& logger,
      ExecutionContext* context = nullptr) {
    return FeaturePolicyParser::ParseHeader(
        feature_policy_header, g_empty_string, origin, logger, logger, context);
  }
};

struct ParsedPolicyDeclarationForTest {
  mojom::blink::FeaturePolicyFeature feature;
  bool matches_all_origins;
  bool matches_opaque_src;
  std::vector<const char*> origins;
};

using ParsedPolicyForTest = std::vector<ParsedPolicyDeclarationForTest>;

struct FeaturePolicyParserTestCase {
  const char* test_name;

  // Test inputs.
  const char* feature_policy_string;
  const char* permissions_policy_string;
  const char* self_origin;
  const char* src_origin;

  // Test expectation.
  ParsedPolicyForTest expected_parse_result;
};

class FeaturePolicyParserParsingTest
    : public FeaturePolicyParserTest,
      public ::testing::WithParamInterface<FeaturePolicyParserTestCase> {
 private:
  scoped_refptr<const SecurityOrigin> GetSrcOrigin(const char* origin_str) {
    scoped_refptr<const SecurityOrigin> src_origin;
    if (String(origin_str) == OPAQUE_ORIGIN) {
      src_origin = SecurityOrigin::CreateUniqueOpaque();
    } else {
      src_origin =
          origin_str ? SecurityOrigin::CreateFromString(origin_str) : nullptr;
    }
    return src_origin;
  }

 protected:
  ParsedFeaturePolicy ParseFeaturePolicy(const char* policy_string,
                                         const char* self_origin_string,
                                         const char* src_origin_string,
                                         PolicyParserMessageBuffer& logger,
                                         const FeatureNameMap& feature_names,
                                         ExecutionContext* context = nullptr) {
    return FeaturePolicyParser::ParseFeaturePolicyForTest(
        policy_string, SecurityOrigin::CreateFromString(self_origin_string),
        GetSrcOrigin(src_origin_string), logger, feature_names, context);
  }

  ParsedFeaturePolicy ParsePermissionsPolicy(
      const char* policy_string,
      const char* self_origin_string,
      const char* src_origin_string,
      PolicyParserMessageBuffer& logger,
      const FeatureNameMap& feature_names,
      ExecutionContext* context = nullptr) {
    return FeaturePolicyParser::ParsePermissionsPolicyForTest(
        policy_string, SecurityOrigin::CreateFromString(self_origin_string),
        GetSrcOrigin(src_origin_string), logger, feature_names, context);
  }

  void CheckParsedPolicy(const ParsedFeaturePolicy& actual,
                         const ParsedPolicyForTest& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < actual.size(); ++i) {
      const auto& actual_declaration = actual[i];
      const auto& expected_declaration = expected[i];

      EXPECT_EQ(actual_declaration.feature, expected_declaration.feature);
      EXPECT_EQ(actual_declaration.matches_all_origins,
                expected_declaration.matches_all_origins);
      EXPECT_EQ(actual_declaration.matches_opaque_src,
                expected_declaration.matches_opaque_src);

      ASSERT_EQ(actual_declaration.allowed_origins.size(),
                expected_declaration.origins.size());
      for (size_t j = 0; j < actual_declaration.allowed_origins.size(); ++j) {
        EXPECT_TRUE(actual_declaration.allowed_origins[j].IsSameOriginWith(
            url::Origin::Create(GURL(expected_declaration.origins[j]))));
      }
    }
  }

  void CheckConsoleMessage(
      const Vector<PolicyParserMessageBuffer::Message>& actual,
      const std::vector<String> expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < actual.size(); ++i) {
      EXPECT_EQ(actual[i].content, expected[i]);
    }
  }

 public:
  static const FeaturePolicyParserTestCase kCases[];
};

const FeaturePolicyParserTestCase FeaturePolicyParserParsingTest::kCases[] = {
    {
        /* test_name */ "EmptyPolicy",
        /* feature_policy_string */ "",
        /* permissions_policy_string */ "",
        /* self_origin */ ORIGIN_A,
        /* src_origin */ ORIGIN_B,
        /* expected_parse_result */ {},
    },
    {
        /* test_name */ "SimplePolicyWithSelf",
        /* feature_policy_string */ "geolocation 'self'",
        /* permissions_policy_string */ "geolocation=self",
        /* self_origin */ ORIGIN_A,
        /* src_origin */ ORIGIN_B,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ false,
                {ORIGIN_A},
            },
        },
    },
    {
        /* test_name */ "SimplePolicyWithSelfExplicitListSyntax",
        /* feature_policy_string */ NOT_APPLICABLE,
        /* permissions_policy_string */ "geolocation=(self)",
        /* self_origin */ ORIGIN_A,
        /* src_origin */ ORIGIN_B,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ false,
                {ORIGIN_A},
            },
        },
    },
    {
        /* test_name */ "SimplePolicyWithStar",
        /* feature_policy_string */ "geolocation *",
        /* permissions_policy_string */ "geolocation=*",
        /* self_origin */ ORIGIN_A,
        /* src_origin */ ORIGIN_B,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ true,
                /* matches_opaque_src */ true,
                {},
            },
        },
    },
    {
        /* test_name */ "ComplicatedPolicy",
        /* feature_policy_string */
        "geolocation *; "
        "fullscreen " ORIGIN_B " " ORIGIN_C "; "
        "payment 'self'",
        /* permissions_policy_string */
        "geolocation=*, "
        "fullscreen=(\"" ORIGIN_B "\" \"" ORIGIN_C "\"),"
        "payment=self",
        /* self_origin */ ORIGIN_A,
        /* src_origin */ ORIGIN_B,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ true,
                /* matches_opaque_src */ true,
                {},
            },
            {
                mojom::blink::FeaturePolicyFeature::kFullscreen,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ false,
                {ORIGIN_B, ORIGIN_C},
            },
            {
                mojom::blink::FeaturePolicyFeature::kPayment,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ false,
                {ORIGIN_A},
            },
        },
    },
    {
        /* test_name */ "MultiplePoliciesIncludingBadFeatureName",
        /* feature_policy_string */
        "geolocation * " ORIGIN_B "; "
        "fullscreen " ORIGIN_B " bad_feature_name " ORIGIN_C ","
        "payment 'self' badorigin",
        /* permissions_policy_string */
        "geolocation=(* \"" ORIGIN_B "\"),"
        "fullscreen=(\"" ORIGIN_B "\" bad_feature_name \"" ORIGIN_C "\"),"
        "payment=(self \"badorigin\")",
        /* self_origin */ ORIGIN_A,
        /* src_origin */ ORIGIN_B,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ true,
                /* matches_opaque_src */ true,
                {},
            },
            {
                mojom::blink::FeaturePolicyFeature::kFullscreen,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ false,
                {ORIGIN_B, ORIGIN_C},
            },
            {
                mojom::blink::FeaturePolicyFeature::kPayment,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ false,
                {ORIGIN_A},
            },
        },
    },
    {
        /* test_name */ "HeaderPoliciesWithNoOptionalOriginLists",
        /* feature_policy_string */ "geolocation;fullscreen;payment",
        // Note: In structured header, if no value is associated with a key
        // in dictionary, default value would be boolean true, which is
        // not allowed as allowlist value in permission policy syntax.
        /* permissions_policy_string */
        "geolocation=self,fullscreen=self,payment=self",
        /* self_origin */ ORIGIN_A,
        /* src_origin */ nullptr,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ false,
                {ORIGIN_A},
            },
            {
                mojom::blink::FeaturePolicyFeature::kFullscreen,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ false,
                {ORIGIN_A},
            },
            {
                mojom::blink::FeaturePolicyFeature::kPayment,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ false,
                {ORIGIN_A},
            },
        },
    },
    {
        /* test_name */ "EmptyPolicyOpaqueSrcOrigin",
        /* feature_policy_string */ "",
        /* permissions_policy_string */ "",
        /* self_origin */ ORIGIN_A,
        /* src_origin */ OPAQUE_ORIGIN,
        /* expected_parse_result */ {},
    },
    {
        /* test_name */ "SimplePolicyOpaqueSrcOrigin",
        /* feature_policy_string */ "geolocation",
        /* permissions_policy_string */ NOT_APPLICABLE,
        /* self_origin */ ORIGIN_A,
        /* src_origin */ OPAQUE_ORIGIN,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ true,
                {},
            },
        },
    },
    {
        /* test_name */ "SimplePolicyWithSrcOpaqueSrcOrigin",
        /* feature_policy_string */ "geolocation 'src'",
        /* permissions_policy_string */ NOT_APPLICABLE,
        /* self_origin */ ORIGIN_A,
        /* src_origin */ OPAQUE_ORIGIN,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ true,
                {},
            },
        },
    },
    {
        /* test_name */ "SimplePolicyWithStarOpaqueSrcOrigin",
        /* feature_policy_string */ "geolocation *",
        /* permissions_policy_string */ "geolocation=*",
        /* self_origin */ ORIGIN_A,
        /* src_origin */ OPAQUE_ORIGIN,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ true,
                /* matches_opaque_src */ true,
                {},
            },
        },
    },
    {
        /* test_name */ "PolicyWithExplicitOriginsOpaqueSrcOrigin",
        /* feature_policy_string */ "geolocation " ORIGIN_B " " ORIGIN_C,
        /* permissions_policy_string */
        "geolocation=(\"" ORIGIN_B "\" \"" ORIGIN_C "\")",
        /* self_origin */ ORIGIN_A,
        /* src_origin */ OPAQUE_ORIGIN,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ false,
                {ORIGIN_B, ORIGIN_C},
            },
        },
    },
    {
        /* test_name */ "PolicyWithMultipleOriginsIncludingSrc"
                        "OpaqueSrcOrigin",
        /* feature_policy_string */ "geolocation " ORIGIN_B " 'src'",
        /* permissions_policy_string */ NOT_APPLICABLE,
        /* self_origin */ ORIGIN_A,
        /* src_origin */ OPAQUE_ORIGIN,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ true,
                {ORIGIN_B},
            },
        },
    },
    {
        /* test_name */ "PolicyWithInvalidDataTypeInt",
        /* feature_policy_string */ NOT_APPLICABLE,
        // int value should be rejected as allowlist items.
        /* permissions_policy_string */ "geolocation=9",
        /* self_origin */ ORIGIN_A,
        /* src_origin */ nullptr,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ false,
                {},
            },
        },
    },
    {
        /* test_name */ "PolicyWithInvalidDataTypeFloat",
        /* feature_policy_string */ NOT_APPLICABLE,
        // decimal value should be rejected as allowlist items.
        /* permissions_policy_string */ "geolocation=1.1",
        /* self_origin */ ORIGIN_A,
        /* src_origin */ nullptr,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ false,
                {},
            },
        },
    },
    {
        /* test_name */ "PolicyWithInvalidDataTypeBoolean",
        /* feature_policy_string */ NOT_APPLICABLE,
        // boolean value should be rejected as allowlist items.
        /* permissions_policy_string */ "geolocation=?0",
        /* self_origin */ ORIGIN_A,
        /* src_origin */ nullptr,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ false,
                {},
            },
        },
    },
    {
        /* test_name */ "PolicyWithEmptyOriginString",
        /* feature_policy_string */ NOT_APPLICABLE,
        /* permissions_policy_string */ "geolocation=\"\"",
        /* self_origin */ ORIGIN_A,
        /* src_origin */ nullptr,
        /* expected_parse_result */
        {
            {
                mojom::blink::FeaturePolicyFeature::kGeolocation,
                /* matches_all_origins */ false,
                /* matches_opaque_src */ false,
                {},
            },
        },
    },
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FeaturePolicyParserParsingTest,
    ::testing::ValuesIn(FeaturePolicyParserParsingTest::kCases),
    [](const testing::TestParamInfo<FeaturePolicyParserTestCase>& param_info) {
      return param_info.param.test_name;
    });

TEST_P(FeaturePolicyParserParsingTest, FeaturePolicyParsedCorrectly) {
  PolicyParserMessageBuffer logger;
  const FeaturePolicyParserTestCase& test_case = GetParam();
  if (test_case.feature_policy_string == NOT_APPLICABLE)
    return;

  ASSERT_NE(test_case.self_origin, nullptr);

  CheckParsedPolicy(
      ParseFeaturePolicy(test_case.feature_policy_string, test_case.self_origin,
                         test_case.src_origin, logger, test_feature_name_map),
      test_case.expected_parse_result);
}

TEST_P(FeaturePolicyParserParsingTest, PermissionsPolicyParsedCorrectly) {
  PolicyParserMessageBuffer logger;
  const FeaturePolicyParserTestCase& test_case = GetParam();
  if (test_case.permissions_policy_string == NOT_APPLICABLE)
    return;

  ASSERT_NE(test_case.self_origin, nullptr);
  CheckParsedPolicy(
      ParsePermissionsPolicy(test_case.permissions_policy_string,
                             test_case.self_origin, test_case.src_origin,
                             logger, test_feature_name_map),
      test_case.expected_parse_result);
}

TEST_F(FeaturePolicyParserParsingTest,
       FeaturePolicyDuplicatedFeatureDeclaration) {
  PolicyParserMessageBuffer logger;

  // For Feature-Policy header, if there are multiple declaration for same
  // feature, the allowlist value from *FIRST* declaration will be taken.
  CheckParsedPolicy(FeaturePolicyParser::ParseHeader(
                        "geolocation 'none', geolocation 'self'", "",
                        origin_a_.get(), logger, logger, nullptr /* context */),
                    {
                        {
                            // allowlist value 'none' is expected.
                            mojom::blink::FeaturePolicyFeature::kGeolocation,
                            /* matches_all_origins */ false,
                            /* matches_opaque_src */ false,
                            {},
                        },
                    });

  EXPECT_TRUE(logger.GetMessages().IsEmpty());
}

TEST_F(FeaturePolicyParserParsingTest,
       PermissionsPolicyDuplicatedFeatureDeclaration) {
  PolicyParserMessageBuffer logger;

  // For Permissions-Policy header, if there are multiple declaration for same
  // feature, the allowlist value from *LAST* declaration will be taken.
  CheckParsedPolicy(FeaturePolicyParser::ParseHeader(
                        "", "geolocation=(), geolocation=self", origin_a_.get(),
                        logger, logger, nullptr /* context */),
                    {
                        {
                            // allowlist value 'self' is expected.
                            mojom::blink::FeaturePolicyFeature::kGeolocation,
                            /* matches_all_origins */ false,
                            /* matches_opaque_src */ false,
                            {ORIGIN_A},
                        },
                    });

  EXPECT_TRUE(logger.GetMessages().IsEmpty());
}

TEST_F(FeaturePolicyParserParsingTest,
       FeaturePolicyHeaderPermissionsPolicyHeaderCoExistConflictEntry) {
  PolicyParserMessageBuffer logger;

  // When there is conflict take the value from permission policy,
  // non-conflicting entries will be merged.
  CheckParsedPolicy(FeaturePolicyParser::ParseHeader(
                        "geolocation 'none', fullscreen 'self'",
                        "geolocation=self, payment=*", origin_a_.get(), logger,
                        logger, nullptr /* context */),
                    {
                        {
                            // With geolocation appearing in both headers,
                            // the value should be taken from permissions policy
                            // header, which is 'self' here.
                            mojom::blink::FeaturePolicyFeature::kGeolocation,
                            /* matches_all_origins */ false,
                            /* matches_opaque_src */ false,
                            {ORIGIN_A},
                        },
                        {
                            mojom::blink::FeaturePolicyFeature::kPayment,
                            /* matches_all_origins */ true,
                            /* matches_opaque_src */ true,
                            {},
                        },
                        {
                            mojom::blink::FeaturePolicyFeature::kFullscreen,
                            /* matches_all_origins */ false,
                            /* matches_opaque_src */ false,
                            {ORIGIN_A},
                        },
                    });
}

TEST_F(FeaturePolicyParserParsingTest,
       FeaturePolicyHeaderPermissionsPolicyHeaderCoExistSeparateLogger) {
  PolicyParserMessageBuffer feature_policy_logger("Feature Policy: ");
  PolicyParserMessageBuffer permissions_policy_logger("Permissions Policy: ");

  // 'geolocation' in permissions policy has a invalid allowlist item, which
  // results in an empty allowlist, which is equivalent to 'none' in feature
  // policy syntax.
  CheckParsedPolicy(
      FeaturePolicyParser::ParseHeader(
          "worse-feature 'none', geolocation 'self'" /* feature_policy_header */
          ,
          "bad-feature=*, geolocation=\"data://bad-origin\"" /* permissions_policy_header
                                                              */
          ,
          origin_a_.get(), feature_policy_logger, permissions_policy_logger,
          nullptr /* context */
          ),
      {
          {
              mojom::blink::FeaturePolicyFeature::kGeolocation,
              /* matches_all_origins */ false,
              /* matches_opaque_src */ false,
              {},
          },
      });

  CheckConsoleMessage(
      feature_policy_logger.GetMessages(),
      {
          "Feature Policy: Unrecognized feature: 'worse-feature'.",
          "Feature Policy: Feature geolocation has been specified in both "
          "Feature-Policy and Permissions-Policy header. Value defined in "
          "Permissions-Policy header will be used.",
      });
  CheckConsoleMessage(
      permissions_policy_logger.GetMessages(),
      {
          "Permissions Policy: Unrecognized feature: 'bad-feature'.",
          "Permissions Policy: Unrecognized origin: 'data://bad-origin'.",
      });
}

TEST_F(FeaturePolicyParserTest, ParseValidPolicy) {
  for (const char* policy_string : kValidPolicies) {
    PolicyParserMessageBuffer logger;
    FeaturePolicyParser::ParseFeaturePolicyForTest(
        policy_string, origin_a_.get(), origin_b_.get(), logger,
        test_feature_name_map);
    EXPECT_EQ(0UL, logger.GetMessages().size())
        << "Should parse " << policy_string;
  }
}

TEST_F(FeaturePolicyParserTest, ParseInvalidPolicy) {
  for (const char* policy_string : kInvalidPolicies) {
    PolicyParserMessageBuffer logger;
    FeaturePolicyParser::ParseFeaturePolicyForTest(
        policy_string, origin_a_.get(), origin_b_.get(), logger,
        test_feature_name_map);
    EXPECT_LT(0UL, logger.GetMessages().size())
        << "Should fail to parse " << policy_string;
  }
}

TEST_F(FeaturePolicyParserTest, ParseTooLongPolicy) {
  PolicyParserMessageBuffer logger;
  auto policy_string = "geolocation http://" + std::string(1 << 17, 'a');
  FeaturePolicyParser::ParseFeaturePolicyForTest(
      policy_string.c_str(), origin_a_.get(), origin_b_.get(), logger,
      test_feature_name_map);
  EXPECT_EQ(1UL, logger.GetMessages().size())
      << "Should fail to parse feature policy string with size "
      << policy_string.size();
  FeaturePolicyParser::ParsePermissionsPolicyForTest(
      policy_string.c_str(), origin_a_.get(), origin_b_.get(), logger,
      test_feature_name_map);
  EXPECT_EQ(2UL, logger.GetMessages().size())
      << "Should fail to parse permissions policy string with size "
      << policy_string.size();
}

// Test histogram counting the use of feature policies in header.
TEST_F(FeaturePolicyParserTest, HeaderHistogram) {
  const char* histogram_name = "Blink.UseCounter.FeaturePolicy.Header";
  HistogramTester tester;
  PolicyParserMessageBuffer logger;

  FeaturePolicyParser::ParseFeaturePolicyForTest("payment; fullscreen",
                                                 origin_a_.get(), nullptr,
                                                 logger, test_feature_name_map);
  tester.ExpectTotalCount(histogram_name, 2);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::blink::FeaturePolicyFeature::kPayment), 1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::blink::FeaturePolicyFeature::kFullscreen),
      1);
}

// Test counting the use of each feature policy only once per header.
TEST_F(FeaturePolicyParserTest, HistogramMultiple) {
  const char* histogram_name = "Blink.UseCounter.FeaturePolicy.Header";
  HistogramTester tester;
  PolicyParserMessageBuffer logger;

  // If the same feature is listed multiple times, it should only be counted
  // once.
  FeaturePolicyParser::ParseFeaturePolicyForTest(
      "geolocation 'self'; payment; geolocation *", origin_a_.get(), nullptr,
      logger, test_feature_name_map);
  FeaturePolicyParser::ParseFeaturePolicyForTest(
      "fullscreen 'self', fullscreen *", origin_a_.get(), nullptr, logger,
      test_feature_name_map);
  tester.ExpectTotalCount(histogram_name, 3);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::blink::FeaturePolicyFeature::kGeolocation),
      1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::blink::FeaturePolicyFeature::kFullscreen),
      1);
}

// Test histogram counting the use of feature policies via "allow"
// attribute. This test parses two policies on the same document.
TEST_F(FeaturePolicyParserTest, AllowHistogramSameDocument) {
  const char* histogram_name = "Blink.UseCounter.FeaturePolicy.Allow";
  HistogramTester tester;
  PolicyParserMessageBuffer logger;
  auto dummy = std::make_unique<DummyPageHolder>();

  FeaturePolicyParser::ParseFeaturePolicyForTest(
      "payment; fullscreen", origin_a_.get(), origin_b_.get(), logger,
      test_feature_name_map, dummy->GetFrame().DomWindow());
  FeaturePolicyParser::ParseFeaturePolicyForTest(
      "fullscreen; geolocation", origin_a_.get(), origin_b_.get(), logger,
      test_feature_name_map, dummy->GetFrame().DomWindow());

  tester.ExpectTotalCount(histogram_name, 3);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::blink::FeaturePolicyFeature::kPayment), 1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::blink::FeaturePolicyFeature::kFullscreen),
      1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::blink::FeaturePolicyFeature::kGeolocation),
      1);
}

// Test histogram counting the use of feature policies via "allow"
// attribute. This test parses two policies on different documents.
TEST_F(FeaturePolicyParserTest, AllowHistogramDifferentDocument) {
  const char* histogram_name = "Blink.UseCounter.FeaturePolicy.Allow";
  HistogramTester tester;
  PolicyParserMessageBuffer logger;
  auto dummy = std::make_unique<DummyPageHolder>();
  auto dummy2 = std::make_unique<DummyPageHolder>();

  FeaturePolicyParser::ParseFeaturePolicyForTest(
      "payment; fullscreen", origin_a_.get(), origin_b_.get(), logger,
      test_feature_name_map, dummy->GetFrame().DomWindow());
  FeaturePolicyParser::ParseFeaturePolicyForTest(
      "fullscreen; geolocation", origin_a_.get(), origin_b_.get(), logger,
      test_feature_name_map, dummy2->GetFrame().DomWindow());

  tester.ExpectTotalCount(histogram_name, 4);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::blink::FeaturePolicyFeature::kPayment), 1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::blink::FeaturePolicyFeature::kFullscreen),
      2);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::blink::FeaturePolicyFeature::kGeolocation),
      1);
}

// Tests the use counter for comma separator in declarations.
TEST_F(FeaturePolicyParserTest, CommaSeparatedUseCounter) {
  PolicyParserMessageBuffer logger;

  // Declarations without a semicolon should not trigger the use counter.
  {
    auto dummy = std::make_unique<DummyPageHolder>();
    ParseFeaturePolicyHeader("payment", origin_a_.get(), logger,
                             dummy->GetFrame().DomWindow());
    EXPECT_FALSE(dummy->GetDocument().IsUseCounted(
        WebFeature::kFeaturePolicyCommaSeparatedDeclarations));
  }

  // Validate that declarations which should trigger the use counter do.
  {
    auto dummy = std::make_unique<DummyPageHolder>();
    ParseFeaturePolicyHeader("payment, fullscreen", origin_a_.get(), logger,
                             dummy->GetFrame().DomWindow());
    EXPECT_TRUE(dummy->GetDocument().IsUseCounted(
        WebFeature::kFeaturePolicyCommaSeparatedDeclarations))
        << "'payment, fullscreen' should trigger the comma separated use "
           "counter.";
  }
}

// Tests the use counter for semicolon separator in declarations.
TEST_F(FeaturePolicyParserTest, SemicolonSeparatedUseCounter) {
  PolicyParserMessageBuffer logger;

  // Declarations without a semicolon should not trigger the use counter.
  {
    auto dummy = std::make_unique<DummyPageHolder>();
    ParseFeaturePolicyHeader("payment", origin_a_.get(), logger,
                             dummy->GetFrame().DomWindow());
    EXPECT_FALSE(dummy->GetDocument().IsUseCounted(
        WebFeature::kFeaturePolicySemicolonSeparatedDeclarations));
  }

  // Validate that declarations which should trigger the use counter do.
  {
    auto dummy = std::make_unique<DummyPageHolder>();
    ParseFeaturePolicyHeader("payment; fullscreen", origin_a_.get(), logger,
                             dummy->GetFrame().DomWindow());
    EXPECT_TRUE(dummy->GetDocument().IsUseCounted(
        WebFeature::kFeaturePolicySemicolonSeparatedDeclarations))
        << "'payment; fullscreen' should trigger the semicolon separated use "
           "counter.";
  }
}

// Tests that the histograms for usage of various directive options are
// recorded correctly.
struct AllowlistHistogramData {
  // Name of the test
  const char* name;
  const char* policy_declaration;
  int expected_total;
  std::vector<FeaturePolicyAllowlistType> expected_buckets;
};

class FeaturePolicyAllowlistHistogramTest
    : public FeaturePolicyParserTest,
      public testing::WithParamInterface<AllowlistHistogramData> {
 public:
  static const AllowlistHistogramData kCases[];
};

const AllowlistHistogramData FeaturePolicyAllowlistHistogramTest::kCases[] = {
    {"Empty", "fullscreen", 1, {FeaturePolicyAllowlistType::kEmpty}},
    {"Empty_MultipleDirectivesComma",
     "fullscreen, geolocation, payment",
     1,
     {FeaturePolicyAllowlistType::kEmpty}},
    {"Empty_MultipleDirectivesSemicolon",
     "fullscreen; payment",
     1,
     {FeaturePolicyAllowlistType::kEmpty}},
    {"Star", "fullscreen *", 1, {FeaturePolicyAllowlistType::kStar}},
    {"Star_MultipleDirectivesComma",
     "fullscreen *, payment *",
     1,
     {FeaturePolicyAllowlistType::kStar}},
    {"Star_MultipleDirectivesSemicolon",
     "fullscreen *; payment *",
     1,
     {FeaturePolicyAllowlistType::kStar}},
    {"Self", "fullscreen 'self'", 1, {FeaturePolicyAllowlistType::kSelf}},
    {"Self_MultipleDirectives",
     "fullscreen 'self', geolocation 'self', payment 'self'",
     1,
     {FeaturePolicyAllowlistType::kSelf}},
    {"None", "fullscreen 'none'", 1, {FeaturePolicyAllowlistType::kNone}},
    {"None_MultipleDirectives",
     "fullscreen 'none'; payment 'none'",
     1,
     {FeaturePolicyAllowlistType::kNone}},
    {"Origins",
     "fullscreen " ORIGIN_A,
     1,
     {FeaturePolicyAllowlistType::kOrigins}},
    {"Origins_MultipleDirectivesComma",
     "fullscreen " ORIGIN_A ", payment " ORIGIN_A,
     1,
     {FeaturePolicyAllowlistType::kOrigins}},
    {"Origins_MultipleDirectivesSemicolon",
     "fullscreen " ORIGIN_A "; payment " ORIGIN_A " " ORIGIN_B,
     1,
     {FeaturePolicyAllowlistType::kOrigins}},
    {"Mixed",
     "fullscreen 'self' " ORIGIN_A,
     1,
     {FeaturePolicyAllowlistType::kMixed}},
    {"Mixed_MultipleDirectives",
     "fullscreen 'self' " ORIGIN_A ", payment 'none' " ORIGIN_A " " ORIGIN_B,
     1,
     {FeaturePolicyAllowlistType::kMixed}},
    {"KeywordsOnly",
     "fullscreen 'self' 'src'",
     1,
     {FeaturePolicyAllowlistType::kKeywordsOnly}},
    {"KeywordsOnly_MultipleDirectives",
     "fullscreen 'self' 'src'; payment 'self' 'none'",
     1,
     {FeaturePolicyAllowlistType::kKeywordsOnly}},
    {"MultipleDirectives_SeparateTypes_Semicolon",
     "fullscreen; geolocation 'self'",
     2,
     {FeaturePolicyAllowlistType::kEmpty, FeaturePolicyAllowlistType::kSelf}},
    {"MultipleDirectives_SeparateTypes_Mixed",
     "fullscreen *; geolocation 'none' " ORIGIN_A,
     2,
     {FeaturePolicyAllowlistType::kStar, FeaturePolicyAllowlistType::kMixed}},
    {"MultipleDirectives_SeparateTypes_Comma",
     "fullscreen *, geolocation 'none', payment " ORIGIN_A " " ORIGIN_B,
     3,
     {FeaturePolicyAllowlistType::kStar, FeaturePolicyAllowlistType::kNone,
      FeaturePolicyAllowlistType::kOrigins}}};

INSTANTIATE_TEST_SUITE_P(
    All,
    FeaturePolicyAllowlistHistogramTest,
    ::testing::ValuesIn(FeaturePolicyAllowlistHistogramTest::kCases),
    [](const testing::TestParamInfo<AllowlistHistogramData>& param_info) {
      return param_info.param.name;
    });

TEST_P(FeaturePolicyAllowlistHistogramTest, HeaderHistogram) {
  PolicyParserMessageBuffer logger;
  HistogramTester tester;

  AllowlistHistogramData data = GetParam();

  auto dummy = std::make_unique<DummyPageHolder>();
  ParseFeaturePolicyHeader(data.policy_declaration, origin_a_.get(), logger,
                           dummy->GetFrame().DomWindow());
  for (FeaturePolicyAllowlistType expected_bucket : data.expected_buckets) {
    tester.ExpectBucketCount(kAllowlistHeaderHistogram,
                             static_cast<int>(expected_bucket), 1);
  }

  tester.ExpectTotalCount(kAllowlistHeaderHistogram, data.expected_total);
}

TEST_F(FeaturePolicyAllowlistHistogramTest, MixedInHeaderHistogram) {
  PolicyParserMessageBuffer logger;
  HistogramTester tester;

  auto dummy = std::make_unique<DummyPageHolder>();
  const char* declaration = "fullscreen *; geolocation 'self' " ORIGIN_A;
  ParseFeaturePolicyHeader(declaration, origin_a_.get(), logger,
                           dummy->GetFrame().DomWindow());

  tester.ExpectBucketCount(kAllowlistHeaderHistogram,
                           static_cast<int>(FeaturePolicyAllowlistType::kStar),
                           1);
  tester.ExpectBucketCount(kAllowlistHeaderHistogram,
                           static_cast<int>(FeaturePolicyAllowlistType::kMixed),
                           1);

  tester.ExpectTotalCount(kAllowlistHeaderHistogram, 2);
}

TEST_P(FeaturePolicyAllowlistHistogramTest, AttributeHistogram) {
  PolicyParserMessageBuffer logger;
  HistogramTester tester;

  AllowlistHistogramData data = GetParam();

  auto dummy = std::make_unique<DummyPageHolder>();
  FeaturePolicyParser::ParseAttribute(data.policy_declaration, origin_a_.get(),
                                      origin_b_.get(), logger,
                                      dummy->GetFrame().DomWindow());
  for (FeaturePolicyAllowlistType expected_bucket : data.expected_buckets) {
    tester.ExpectBucketCount(kAllowlistAttributeHistogram,
                             static_cast<int>(expected_bucket), 1);
  }

  tester.ExpectTotalCount(kAllowlistAttributeHistogram, data.expected_total);
}

TEST_F(FeaturePolicyAllowlistHistogramTest, MixedInAttributeHistogram) {
  PolicyParserMessageBuffer logger;
  HistogramTester tester;

  auto dummy = std::make_unique<DummyPageHolder>();
  const char* declaration = "fullscreen *; geolocation 'src' " ORIGIN_A;
  FeaturePolicyParser::ParseAttribute(declaration, origin_a_.get(),
                                      origin_b_.get(), logger,
                                      dummy->GetFrame().DomWindow());

  tester.ExpectBucketCount(kAllowlistAttributeHistogram,
                           static_cast<int>(FeaturePolicyAllowlistType::kStar),
                           1);
  tester.ExpectBucketCount(kAllowlistAttributeHistogram,
                           static_cast<int>(FeaturePolicyAllowlistType::kMixed),
                           1);

  tester.ExpectTotalCount(kAllowlistAttributeHistogram, 2);
}

TEST_F(FeaturePolicyAllowlistHistogramTest, SrcInAttributeHistogram) {
  PolicyParserMessageBuffer logger;
  HistogramTester tester;

  auto dummy = std::make_unique<DummyPageHolder>();
  const char* declaration = "fullscreen 'src'";
  FeaturePolicyParser::ParseAttribute(declaration, origin_a_.get(),
                                      origin_b_.get(), logger,
                                      dummy->GetFrame().DomWindow());

  tester.ExpectBucketCount(kAllowlistAttributeHistogram,
                           static_cast<int>(FeaturePolicyAllowlistType::kSrc),
                           1);

  tester.ExpectTotalCount(kAllowlistAttributeHistogram, 1);
}

// Test policy mutation methods
class FeaturePolicyMutationTest : public testing::Test {
 protected:
  FeaturePolicyMutationTest() = default;

  ~FeaturePolicyMutationTest() override = default;

  url::Origin url_origin_a_ = url::Origin::Create(GURL(ORIGIN_A));
  url::Origin url_origin_b_ = url::Origin::Create(GURL(ORIGIN_B));
  url::Origin url_origin_c_ = url::Origin::Create(GURL(ORIGIN_C));

  // Returns true if the policy contains a declaration for the feature which
  // allows it in all origins.
  bool IsFeatureAllowedEverywhere(mojom::blink::FeaturePolicyFeature feature,
                                  const ParsedFeaturePolicy& policy) {
    const auto& result = std::find_if(policy.begin(), policy.end(),
                                      [feature](const auto& declaration) {
                                        return declaration.feature == feature;
                                      });
    if (result == policy.end())
      return false;

    return result->feature == feature && result->matches_all_origins &&
           result->matches_opaque_src && result->allowed_origins.empty();
  }

  // Returns true if the policy contains a declaration for the feature which
  // disallows it in all origins.
  bool IsFeatureDisallowedEverywhere(mojom::blink::FeaturePolicyFeature feature,
                                     const ParsedFeaturePolicy& policy) {
    const auto& result = std::find_if(policy.begin(), policy.end(),
                                      [feature](const auto& declaration) {
                                        return declaration.feature == feature;
                                      });
    if (result == policy.end())
      return false;

    return result->feature == feature && !result->matches_all_origins &&
           !result->matches_opaque_src && result->allowed_origins.empty();
  }

  ParsedFeaturePolicy test_policy = {
      {mojom::blink::FeaturePolicyFeature::kFullscreen,
       /* allowed_origins */ {url_origin_a_, url_origin_b_}, false, false},
      {mojom::blink::FeaturePolicyFeature::kGeolocation,
       /* allowed_origins */ {url_origin_a_}, false, false}};

  ParsedFeaturePolicy empty_policy = {};
};

TEST_F(FeaturePolicyMutationTest, TestIsFeatureDeclared) {
  EXPECT_TRUE(IsFeatureDeclared(mojom::blink::FeaturePolicyFeature::kFullscreen,
                                test_policy));
  EXPECT_TRUE(IsFeatureDeclared(
      mojom::blink::FeaturePolicyFeature::kGeolocation, test_policy));
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::blink::FeaturePolicyFeature::kUsb, test_policy));
  EXPECT_FALSE(IsFeatureDeclared(mojom::blink::FeaturePolicyFeature::kNotFound,
                                 test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestIsFeatureDeclaredWithEmptyPolicy) {
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::FeaturePolicyFeature::kFullscreen, empty_policy));
  EXPECT_FALSE(IsFeatureDeclared(mojom::blink::FeaturePolicyFeature::kNotFound,
                                 empty_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveAbsentFeature) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(mojom::blink::FeaturePolicyFeature::kPayment,
                                 test_policy));
  EXPECT_FALSE(RemoveFeatureIfPresent(
      mojom::blink::FeaturePolicyFeature::kPayment, test_policy));
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(mojom::blink::FeaturePolicyFeature::kPayment,
                                 test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveFromEmptyPolicy) {
  ASSERT_EQ(0UL, empty_policy.size());
  EXPECT_FALSE(RemoveFeatureIfPresent(
      mojom::blink::FeaturePolicyFeature::kPayment, test_policy));
  ASSERT_EQ(0UL, empty_policy.size());
}

TEST_F(FeaturePolicyMutationTest, TestRemoveFeatureIfPresent) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_TRUE(IsFeatureDeclared(mojom::blink::FeaturePolicyFeature::kFullscreen,
                                test_policy));
  EXPECT_TRUE(RemoveFeatureIfPresent(
      mojom::blink::FeaturePolicyFeature::kFullscreen, test_policy));
  EXPECT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::FeaturePolicyFeature::kFullscreen, test_policy));

  // Attempt to remove the feature again
  EXPECT_FALSE(RemoveFeatureIfPresent(
      mojom::blink::FeaturePolicyFeature::kFullscreen, test_policy));
  EXPECT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::FeaturePolicyFeature::kFullscreen, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveFeatureIfPresentOnSecondFeature) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_TRUE(IsFeatureDeclared(
      mojom::blink::FeaturePolicyFeature::kGeolocation, test_policy));
  EXPECT_TRUE(RemoveFeatureIfPresent(
      mojom::blink::FeaturePolicyFeature::kGeolocation, test_policy));
  ASSERT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::FeaturePolicyFeature::kGeolocation, test_policy));

  // Attempt to remove the feature again
  EXPECT_FALSE(RemoveFeatureIfPresent(
      mojom::blink::FeaturePolicyFeature::kGeolocation, test_policy));
  EXPECT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::FeaturePolicyFeature::kGeolocation, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveAllFeatures) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_TRUE(RemoveFeatureIfPresent(
      mojom::blink::FeaturePolicyFeature::kFullscreen, test_policy));
  EXPECT_TRUE(RemoveFeatureIfPresent(
      mojom::blink::FeaturePolicyFeature::kGeolocation, test_policy));
  EXPECT_EQ(0UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::FeaturePolicyFeature::kFullscreen, test_policy));
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::FeaturePolicyFeature::kGeolocation, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestDisallowIfNotPresent) {
  ParsedFeaturePolicy copy = test_policy;
  // Try to disallow a feature which already exists
  EXPECT_FALSE(DisallowFeatureIfNotPresent(
      mojom::blink::FeaturePolicyFeature::kFullscreen, copy));
  ASSERT_EQ(copy, test_policy);

  // Disallow a new feature
  EXPECT_TRUE(DisallowFeatureIfNotPresent(
      mojom::blink::FeaturePolicyFeature::kPayment, copy));
  EXPECT_EQ(3UL, copy.size());
  // Verify that the feature is, in fact, now disallowed everywhere
  EXPECT_TRUE(IsFeatureDisallowedEverywhere(
      mojom::blink::FeaturePolicyFeature::kPayment, copy));
}

TEST_F(FeaturePolicyMutationTest, TestAllowEverywhereIfNotPresent) {
  ParsedFeaturePolicy copy = test_policy;
  // Try to allow a feature which already exists
  EXPECT_FALSE(AllowFeatureEverywhereIfNotPresent(
      mojom::blink::FeaturePolicyFeature::kFullscreen, copy));
  ASSERT_EQ(copy, test_policy);

  // Allow a new feature
  EXPECT_TRUE(AllowFeatureEverywhereIfNotPresent(
      mojom::blink::FeaturePolicyFeature::kPayment, copy));
  EXPECT_EQ(3UL, copy.size());
  // Verify that the feature is, in fact, allowed everywhere
  EXPECT_TRUE(IsFeatureAllowedEverywhere(
      mojom::blink::FeaturePolicyFeature::kPayment, copy));
}

TEST_F(FeaturePolicyMutationTest, TestDisallowUnconditionally) {
  // Try to disallow a feature which already exists
  DisallowFeature(mojom::blink::FeaturePolicyFeature::kFullscreen, test_policy);
  // Should not have changed the number of declarations
  EXPECT_EQ(2UL, test_policy.size());
  // Verify that the feature is, in fact, now disallowed everywhere
  EXPECT_TRUE(IsFeatureDisallowedEverywhere(
      mojom::blink::FeaturePolicyFeature::kFullscreen, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestDisallowNewFeatureUnconditionally) {
  // Try to disallow a feature which does not yet exist
  DisallowFeature(mojom::blink::FeaturePolicyFeature::kPayment, test_policy);
  // Should have added a new declaration
  EXPECT_EQ(3UL, test_policy.size());
  // Verify that the feature is, in fact, now disallowed everywhere
  EXPECT_TRUE(IsFeatureDisallowedEverywhere(
      mojom::blink::FeaturePolicyFeature::kPayment, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestAllowUnconditionally) {
  // Try to allow a feature which already exists
  AllowFeatureEverywhere(mojom::blink::FeaturePolicyFeature::kFullscreen,
                         test_policy);
  // Should not have changed the number of declarations
  EXPECT_EQ(2UL, test_policy.size());
  // Verify that the feature is, in fact, now allowed everywhere
  EXPECT_TRUE(IsFeatureAllowedEverywhere(
      mojom::blink::FeaturePolicyFeature::kFullscreen, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestAllowNewFeatureUnconditionally) {
  // Try to allow a feature which does not yet exist
  AllowFeatureEverywhere(mojom::blink::FeaturePolicyFeature::kPayment,
                         test_policy);
  // Should have added a new declaration
  EXPECT_EQ(3UL, test_policy.size());
  // Verify that the feature is, in fact, now allowed everywhere
  EXPECT_TRUE(IsFeatureAllowedEverywhere(
      mojom::blink::FeaturePolicyFeature::kPayment, test_policy));
}

class FeaturePolicyViolationHistogramTest : public testing::Test {
 protected:
  FeaturePolicyViolationHistogramTest() = default;

  ~FeaturePolicyViolationHistogramTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FeaturePolicyViolationHistogramTest);
};

TEST_F(FeaturePolicyViolationHistogramTest, PotentialViolation) {
  HistogramTester tester;
  const char* histogram_name =
      "Blink.UseCounter.FeaturePolicy.PotentialViolation";
  auto dummy_page_holder_ = std::make_unique<DummyPageHolder>();
  // Probing feature state should not count.
  dummy_page_holder_->GetFrame().DomWindow()->IsFeatureEnabled(
      mojom::blink::FeaturePolicyFeature::kPayment);
  tester.ExpectTotalCount(histogram_name, 0);
  // Checking the feature state with reporting intent should record a potential
  // violation.
  dummy_page_holder_->GetFrame().DomWindow()->IsFeatureEnabled(
      mojom::blink::FeaturePolicyFeature::kPayment,
      ReportOptions::kReportOnFailure);
  tester.ExpectTotalCount(histogram_name, 1);
  // The potential violation for an already recorded violation does not count
  // again.
  dummy_page_holder_->GetFrame().DomWindow()->IsFeatureEnabled(
      mojom::blink::FeaturePolicyFeature::kPayment,
      ReportOptions::kReportOnFailure);
  tester.ExpectTotalCount(histogram_name, 1);
  // Sanity check: check some other feature to increase the count.
  dummy_page_holder_->GetFrame().DomWindow()->IsFeatureEnabled(
      mojom::blink::FeaturePolicyFeature::kFullscreen,
      ReportOptions::kReportOnFailure);
  tester.ExpectTotalCount(histogram_name, 2);
}

}  // namespace blink
