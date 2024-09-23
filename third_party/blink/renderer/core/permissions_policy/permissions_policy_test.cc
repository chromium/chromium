// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/core/permissions_policy/policy_helper.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "url/gurl.h"
#include "url/origin.h"

// Origin strings used for tests
#define ORIGIN_A "https://example.com/"
#define ORIGIN_A_SUBDOMAIN_WILDCARD "https://*.example.com/"
#define ORIGIN_A_SUBDOMAIN_ESCAPED "https://%2A.example.com/"
#define ORIGIN_B "https://example.net/"
#define ORIGIN_C "https://example.org/"
#define OPAQUE_ORIGIN ""

// identifier used for feature/permissions policy parsing test.
// when there is a testcase for one syntax but not the other.
#define NOT_APPLICABLE nullptr

class GURL;

namespace blink {

namespace {

const char* const kValidHeaderPolicies[] = {
    "",      // An empty policy.
    " ",     // An empty policy.
    ";;",    // Empty policies.
    ",,",    // Empty policies.
    " ; ;",  // Empty policies.
    " , ,",  // Empty policies.
    ",;,",   // Empty policies.
    "geolocation 'none'",
    "geolocation 'self'",
    "geolocation",
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

const char* const kInvalidHeaderPolicies[] = {
    "badfeaturename",
    "badfeaturename 'self'",
    "1.0",
    "geolocation 'src'",  // Only valid for iframe allow attribute.
    "geolocation data:///badorigin",
    "geolocation https://bad;origin",
    "geolocation https:/bad,origin",
    "geolocation https://example.com, https://a.com",
    "geolocation *, payment data:///badorigin",
    "geolocation ws://xn--fd\xbcwsw3taaaaaBaa333aBBBBBBJBBJBBBt",
};
}  // namespace

class PermissionsPolicyParserTest : public ::testing::Test {
 protected:
  PermissionsPolicyParserTest() = default;

  ~PermissionsPolicyParserTest() override = default;

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
      {"fullscreen",
       blink::mojom::blink::PermissionsPolicyFeature::kFullscreen},
      {"payment", blink::mojom::blink::PermissionsPolicyFeature::kPayment},
      {"geolocation",
       blink::mojom::blink::PermissionsPolicyFeature::kGeolocation}};

  ParsedPermissionsPolicy ParseFeaturePolicyHeader(
      const String& feature_policy_header,
      scoped_refptr<const SecurityOrigin> origin,
      PolicyParserMessageBuffer& logger,
      ExecutionContext* context = nullptr) {
    return PermissionsPolicyParser::ParseHeader(
        feature_policy_header, g_empty_string, origin, logger, logger, context);
  }
  test::TaskEnvironment task_environment_;
};

struct OriginWithPossibleWildcardsForTest {
  const char* origin;
  bool has_subdomain_wildcard;
};

struct ParsedPolicyDeclarationForTest {
  mojom::blink::PermissionsPolicyFeature feature;
  std::optional<const char*> self_if_matches;
  bool matches_all_origins;
  bool matches_opaque_src;
  std::vector<OriginWithPossibleWildcardsForTest> allowed_origins;
  std::optional<std::string> reporting_endpoint;
};

using ParsedPolicyForTest = std::vector<ParsedPolicyDeclarationForTest>;

struct PermissionsPolicyParserTestCase {
  const char* test_name;

  // Test inputs.
  const char* feature_policy_string;
  const char* permissions_policy_string;
  const char* self_origin;
  const char* src_origin;

  // Test expectation.
  ParsedPolicyForTest expected_parse_result;
};

class PermissionsPolicyParserParsingTest
    : public PermissionsPolicyParserTest,
      public ::testing::WithParamInterface<PermissionsPolicyParserTestCase> {
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
  ParsedPermissionsPolicy ParseFeaturePolicy(
      const char* policy_string,
      const char* self_origin_string,
      const char* src_origin_string,
      PolicyParserMessageBuffer& logger,
      const FeatureNameMap& feature_names,
      ExecutionContext* context = nullptr) {
    return PermissionsPolicyParser::ParseFeaturePolicyForTest(
        policy_string, SecurityOrigin::CreateFromString(self_origin_string),
        GetSrcOrigin(src_origin_string), logger, feature_names, context);
  }

  ParsedPermissionsPolicy ParsePermissionsPolicy(
      const char* policy_string,
      const char* self_origin_string,
      const char* src_origin_string,
      PolicyParserMessageBuffer& logger,
      const FeatureNameMap& feature_names,
      ExecutionContext* context = nullptr) {
    return PermissionsPolicyParser::ParsePermissionsPolicyForTest(
        policy_string, SecurityOrigin::CreateFromString(self_origin_string),
        GetSrcOrigin(src_origin_string), logger, feature_names, context);
  }

  void CheckParsedPolicy(const ParsedPermissionsPolicy& actual,
                         const ParsedPolicyForTest& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (size_t i = 0; i < actual.size(); ++i) {
      const auto& actual_declaration = actual[i];
      const auto& expected_declaration = expected[i];

      EXPECT_EQ(actual_declaration.feature, expected_declaration.feature);
      if (expected_declaration.self_if_matches) {
        EXPECT_TRUE(actual_declaration.self_if_matches->IsSameOriginWith(
            url::Origin::Create(GURL(*expected_declaration.self_if_matches))));
      } else {
        EXPECT_FALSE(actual_declaration.self_if_matches);
      }
      EXPECT_EQ(actual_declaration.matches_all_origins,
                expected_declaration.matches_all_origins);
      EXPECT_EQ(actual_declaration.matches_opaque_src,
                expected_declaration.matches_opaque_src);
      EXPECT_EQ(actual_declaration.reporting_endpoint,
                expected_declaration.reporting_endpoint);

      ASSERT_EQ(actual_declaration.allowed_origins.size(),
                expected_declaration.allowed_origins.size());
      for (size_t j = 0; j < actual_declaration.allowed_origins.size(); ++j) {
        const url::Origin origin = url::Origin::Create(
            GURL(expected_declaration.allowed_origins[j].origin));
        EXPECT_EQ(
            actual_declaration.allowed_origins[j].CSPSourceForTest().scheme,
            origin.scheme());
        EXPECT_EQ(actual_declaration.allowed_origins[j].CSPSourceForTest().host,
                  origin.host());
        if (actual_declaration.allowed_origins[j].CSPSourceForTest().port !=
            url::PORT_UNSPECIFIED) {
          EXPECT_EQ(
              actual_declaration.allowed_origins[j].CSPSourceForTest().port,
              origin.port());
        }
        EXPECT_EQ(
            actual_declaration.allowed_origins[j]
                .CSPSourceForTest()
                .is_host_wildcard,
            expected_declaration.allowed_origins[j].has_subdomain_wildcard);
      }
    }
  }

  void CheckConsoleMessage(
      const Vector<PolicyParserMessageBuffer::Message>& actual,
      const std::vector<String> expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (wtf_size_t i = 0; i < actual.size(); ++i) {
      EXPECT_EQ(actual[i].content, expected[i]);
    }
  }

 public:
  static const PermissionsPolicyParserTestCase kCases[];
};

const PermissionsPolicyParserTestCase
    PermissionsPolicyParserParsingTest::kCases[] = {
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ ORIGIN_A,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {},
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ ORIGIN_A,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {},
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ true,
                    /* matches_opaque_src */ true,
                    {},
                },
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_B, /*has_subdomain_wildcard=*/false},
                     {ORIGIN_C, /*has_subdomain_wildcard=*/false}},
                },
                {
                    mojom::blink::PermissionsPolicyFeature::kPayment,
                    /* self_if_matches */ ORIGIN_A,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {},
                },
            },
        },
        {
            /* test_name */ "MultiplePoliciesIncludingBadFeatureName",
            /* feature_policy_string */
            "geolocation * " ORIGIN_B "; "
            "fullscreen " ORIGIN_B " bad_feature_name " ORIGIN_C ";"
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ true,
                    /* matches_opaque_src */ true,
                    {},
                },
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_B, /*has_subdomain_wildcard=*/false},
                     {ORIGIN_C, /*has_subdomain_wildcard=*/false}},
                },
                {
                    mojom::blink::PermissionsPolicyFeature::kPayment,
                    /* self_if_matches */ ORIGIN_A,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {},
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ ORIGIN_A,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {},
                },
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ ORIGIN_A,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {},
                },
                {
                    mojom::blink::PermissionsPolicyFeature::kPayment,
                    /* self_if_matches */ ORIGIN_A,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {},
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_B, /*has_subdomain_wildcard=*/false},
                     {ORIGIN_C, /*has_subdomain_wildcard=*/false}},
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ true,
                    {{ORIGIN_B, /*has_subdomain_wildcard=*/false}},
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
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
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {},
                },
            },
        },
        {
            /* test_name */ "ProperWildcardIncludedForFeaturePolicy",
            /* feature_policy_string */
            "fullscreen " ORIGIN_A_SUBDOMAIN_WILDCARD,
            /* permissions_policy_string */ NOT_APPLICABLE,
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {},
                },
            },
        },
        {
            /* test_name */ "ProperWildcardIncludedForPermissionsPolicy",
            /* feature_policy_string */ NOT_APPLICABLE,
            /* permissions_policy_string */
            "fullscreen=(\"" ORIGIN_A_SUBDOMAIN_WILDCARD "\")",
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_A,
                      /*has_subdomain_wildcard=*/true}},
                },
            },
        },
        {
            /* test_name */ "ImproperWildcardsIncluded",
            /* feature_policy_string */
            "fullscreen *://example.com https://foo.*.example.com "
            "https://*.*.example.com",
            /* permissions_policy_string */
            "fullscreen=(\"*://example.com\" \"https://foo.*.example.com\" "
            "\"https://*.*.example.com\")",
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {},
                },
            },
        },
        {
            /* test_name */ "AttributeWithLineBreaks",
            /* feature_policy_string */
            "geolocation;\n"
            "fullscreen",
            /* permissions_policy_string */ NOT_APPLICABLE,
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_B, /*has_subdomain_wildcard=*/false}},
                },
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_B, /*has_subdomain_wildcard=*/false}},
                },
            },
        },
        {
            /* test_name */ "AttributeWithCRLF",
            /* feature_policy_string */
            "geolocation;\r\n"
            "fullscreen",
            /* permissions_policy_string */ NOT_APPLICABLE,
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_B, /*has_subdomain_wildcard=*/false}},
                },
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_B, /*has_subdomain_wildcard=*/false}},
                },
            },
        },
        {
            /* test_name */ "AlternativeWhitespceBetweenTokens",
            /* feature_policy_string */
            "\r\n\r\ngeolocation\t 'self'\f\f" ORIGIN_B "\t",
            /* permissions_policy_string */ NOT_APPLICABLE,
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ ORIGIN_A,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_B, /*has_subdomain_wildcard=*/false}},
                },
            },
        },
        {
            /* test_name */ "ReportingEndpointWithStar",
            /* feature_policy_string */ NOT_APPLICABLE,
            /* permissions_policy_string */
            "fullscreen=*;report-to=endpoint",
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ true,
                    /* matches_opaque_src */ true,
                    {},
                    "endpoint",
                },
            },
        },
        {
            /* test_name */ "ReportingEndpointWithList",
            /* feature_policy_string */ NOT_APPLICABLE,
            /* permissions_policy_string */
            "fullscreen=(\"" ORIGIN_B "\" \"" ORIGIN_C "\");report-to=endpoint",
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_B, /*has_subdomain_wildcard=*/false},
                     {ORIGIN_C, /*has_subdomain_wildcard=*/false}},
                    "endpoint",
                },
            },
        },
        {
            /* test_name */ "ReportingEndpointWithNone",
            /* feature_policy_string */ NOT_APPLICABLE,
            /* permissions_policy_string */
            "fullscreen=();report-to=endpoint",
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {},
                    "endpoint",
                },
            },
        },
        {
            /* test_name */ "ReportingEndpointWithSelf",
            /* feature_policy_string */ NOT_APPLICABLE,
            /* permissions_policy_string */
            "fullscreen=self;report-to=endpoint",
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ ORIGIN_A,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {},
                    "endpoint",
                },
            },
        },
        {
            /* test_name */ "ReportingEndpointWithSingleOrigin",
            /* feature_policy_string */ NOT_APPLICABLE,
            /* permissions_policy_string */
            "fullscreen=\"" ORIGIN_C "\";report-to=endpoint",
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_C, /*has_subdomain_wildcard=*/false}},
                    "endpoint",
                },
            },
        },
        {
            /* test_name */ "InvalidReportingEndpointInList",
            /* feature_policy_string */ NOT_APPLICABLE,
            // Note: The reporting endpoint parameter needs to apply to the
            // entire value for the dictionary entry. In this example, it is
            // placed on a single inner list item, and should therefore be
            // ignored.
            /* permissions_policy_string */
            "fullscreen=(\"" ORIGIN_C "\";report-to=endpoint)",
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_C, /*has_subdomain_wildcard=*/false}},
                    /* reporting_endpoint */ std::nullopt,
                },
            },
        },
        {
            /* test_name */ "ReportingEndpointsInsideAndOutsideList",
            /* feature_policy_string */ NOT_APPLICABLE,
            /* permissions_policy_string */
            "fullscreen=(\"" ORIGIN_C
            "\";report-to=endpoint1);report-to=endpoint2",
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_C, /*has_subdomain_wildcard=*/false}},
                    /* reporting_endpoint */ "endpoint2",
                },
            },
        },
        // DifferentReportingEndpoints
        {
            /* test_name */ "DifferentReportingEndpoints",
            /* feature_policy_string */ NOT_APPLICABLE,
            /* permissions_policy_string */
            "fullscreen=\"" ORIGIN_B "\";report-to=endpoint1,"
            "geolocation=\"" ORIGIN_C "\";report-to=endpoint2",
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_B, /*has_subdomain_wildcard=*/false}},
                    "endpoint1",
                },
                {
                    mojom::blink::PermissionsPolicyFeature::kGeolocation,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ false,
                    /* matches_opaque_src */ false,
                    {{ORIGIN_C, /*has_subdomain_wildcard=*/false}},
                    "endpoint2",
                },
            },
        },
        {
            /* test_name */ "InvalidReportingEndpointsBool",
            /* feature_policy_string */ NOT_APPLICABLE,
            /* permissions_policy_string */
            "fullscreen=*;report-to",
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ true,
                    /* matches_opaque_src */ true,
                    {},
                    /* reporting_endpoint */ std::nullopt,
                },
            },
        },
        {
            /* test_name */ "InvalidReportingEndpointsNumber",
            /* feature_policy_string */ NOT_APPLICABLE,
            /* permissions_policy_string */
            "fullscreen=*;report-to=7",
            /* self_origin */ ORIGIN_A,
            /* src_origin */ ORIGIN_B,
            /* expected_parse_result */
            {
                {
                    mojom::blink::PermissionsPolicyFeature::kFullscreen,
                    /* self_if_matches */ std::nullopt,
                    /* matches_all_origins */ true,
                    /* matches_opaque_src */ true,
                    {},
                    /* reporting_endpoint */ std::nullopt,
                },
            },
        },
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PermissionsPolicyParserParsingTest,
    ::testing::ValuesIn(PermissionsPolicyParserParsingTest::kCases),
    [](const testing::TestParamInfo<PermissionsPolicyParserTestCase>&
           param_info) { return param_info.param.test_name; });

TEST_P(PermissionsPolicyParserParsingTest, FeaturePolicyParsedCorrectly) {
  PolicyParserMessageBuffer logger;
  const PermissionsPolicyParserTestCase& test_case = GetParam();
  if (test_case.feature_policy_string == NOT_APPLICABLE)
    return;

  ASSERT_NE(test_case.self_origin, nullptr);
  CheckParsedPolicy(
      ParseFeaturePolicy(test_case.feature_policy_string, test_case.self_origin,
                         test_case.src_origin, logger, test_feature_name_map),
      test_case.expected_parse_result);
}

TEST_P(PermissionsPolicyParserParsingTest, PermissionsPolicyParsedCorrectly) {
  PolicyParserMessageBuffer logger;
  const PermissionsPolicyParserTestCase& test_case = GetParam();
  if (test_case.permissions_policy_string == NOT_APPLICABLE)
    return;

  ASSERT_NE(test_case.self_origin, nullptr);
  CheckParsedPolicy(
      ParsePermissionsPolicy(test_case.permissions_policy_string,
                             test_case.self_origin, test_case.src_origin,
                             logger, test_feature_name_map),
      test_case.expected_parse_result);
}

TEST_F(PermissionsPolicyParserParsingTest,
       FeaturePolicyDuplicatedFeatureDeclaration) {
  PolicyParserMessageBuffer logger;

  // For Feature-Policy header, if there are multiple declaration for same
  // feature, the allowlist value from *FIRST* declaration will be taken.
  CheckParsedPolicy(
      PermissionsPolicyParser::ParseHeader(
          "geolocation 'none', geolocation 'self'", "", origin_a_.get(), logger,
          logger, nullptr /* context */),
      {
          {
              // allowlist value 'none' is expected.
              mojom::blink::PermissionsPolicyFeature::kGeolocation,
              /* self_if_matches */ std::nullopt,
              /* matches_all_origins */ false,
              /* matches_opaque_src */ false,
              {},
          },
      });

  EXPECT_TRUE(logger.GetMessages().empty());
}

TEST_F(PermissionsPolicyParserParsingTest,
       PermissionsPolicyDuplicatedFeatureDeclaration) {
  PolicyParserMessageBuffer logger;

  // For Permissions-Policy header, if there are multiple declaration for same
  // feature, the allowlist value from *LAST* declaration will be taken.
  CheckParsedPolicy(
      PermissionsPolicyParser::ParseHeader(
          "", "geolocation=(), geolocation=self", origin_a_.get(), logger,
          logger, nullptr /* context */),
      {
          {
              // allowlist value 'self' is expected.
              mojom::blink::PermissionsPolicyFeature::kGeolocation,
              /* self_if_matches */ ORIGIN_A,
              /* matches_all_origins */ false,
              /* matches_opaque_src */ false,
              {},
          },
      });

  EXPECT_TRUE(logger.GetMessages().empty());
}

TEST_F(PermissionsPolicyParserParsingTest,
       FeaturePolicyHeaderPermissionsPolicyHeaderCoExistConflictEntry) {
  PolicyParserMessageBuffer logger;

  // When there is conflict take the value from permission policy,
  // non-conflicting entries will be merged.
  CheckParsedPolicy(
      PermissionsPolicyParser::ParseHeader(
          "geolocation 'none', fullscreen 'self'",
          "geolocation=self, payment=*", origin_a_.get(), logger, logger,
          nullptr /* context */),
      {
          {
              // With geolocation appearing in both headers,
              // the value should be taken from permissions policy
              // header, which is 'self' here.
              mojom::blink::PermissionsPolicyFeature::kGeolocation,
              /* self_if_matches */ ORIGIN_A,
              /* matches_all_origins */ false,
              /* matches_opaque_src */ false,
              {},
          },
          {
              mojom::blink::PermissionsPolicyFeature::kPayment,
              /* self_if_matches */ std::nullopt,
              /* matches_all_origins */ true,
              /* matches_opaque_src */ true,
              {},
          },
          {
              mojom::blink::PermissionsPolicyFeature::kFullscreen,
              /* self_if_matches */ ORIGIN_A,
              /* matches_all_origins */ false,
              /* matches_opaque_src */ false,
              {},
          },
      });
}

TEST_F(PermissionsPolicyParserParsingTest,
       OverlapDeclarationSingleWarningMessage) {
  PolicyParserMessageBuffer feature_policy_logger("");
  PolicyParserMessageBuffer permissions_policy_logger("");

  CheckParsedPolicy(
      PermissionsPolicyParser::ParseHeader(
          "geolocation 'self', fullscreen 'self'" /* feature_policy_header */
          ,
          "geolocation=*, fullscreen=*" /* permissions_policy_header */
          ,
          origin_a_.get(), feature_policy_logger, permissions_policy_logger,
          nullptr /* context */
          ),
      {
          {
              mojom::blink::PermissionsPolicyFeature::kGeolocation,
              /* self_if_matches */ std::nullopt,
              /* matches_all_origins */ true,
              /* matches_opaque_src */ true,
              {},
          },
          {
              mojom::blink::PermissionsPolicyFeature::kFullscreen,
              /* self_if_matches */ std::nullopt,
              /* matches_all_origins */ true,
              /* matches_opaque_src */ true,
              {},
          },
      });

  CheckConsoleMessage(feature_policy_logger.GetMessages(),
                      {
                          "Some features are specified in both Feature-Policy "
                          "and Permissions-Policy header: geolocation, "
                          "fullscreen. Values defined in "
                          "Permissions-Policy header will be used.",
                      });
  CheckConsoleMessage(permissions_policy_logger.GetMessages(), {});
}

TEST_F(PermissionsPolicyParserParsingTest,
       FeaturePolicyHeaderPermissionsPolicyHeaderCoExistSeparateLogger) {
  PolicyParserMessageBuffer feature_policy_logger("Feature Policy: ");
  PolicyParserMessageBuffer permissions_policy_logger("Permissions Policy: ");

  // 'geolocation' in permissions policy has a invalid allowlist item, which
  // results in an empty allowlist, which is equivalent to "()" in permissions
  // policy syntax.
  CheckParsedPolicy(
      PermissionsPolicyParser::ParseHeader(
          "worse-feature 'none', geolocation 'self'" /* feature_policy_header */
          ,
          "bad-feature=*, geolocation=\"data:///bad-origin\"" /* permissions_policy_header
                                                               */
          ,
          origin_a_.get(), feature_policy_logger, permissions_policy_logger,
          nullptr /* context */
          ),
      {
          {
              mojom::blink::PermissionsPolicyFeature::kGeolocation,
              /* self_if_matches */ std::nullopt,
              /* matches_all_origins */ false,
              /* matches_opaque_src */ false,
              {},
          },
      });

  CheckConsoleMessage(
      feature_policy_logger.GetMessages(),
      {
          "Feature Policy: Unrecognized feature: 'worse-feature'.",
          "Feature Policy: Some features are specified in both Feature-Policy "
          "and Permissions-Policy header: geolocation. Values defined in "
          "Permissions-Policy header will be used.",
      });
  CheckConsoleMessage(
      permissions_policy_logger.GetMessages(),
      {
          "Permissions Policy: Unrecognized feature: 'bad-feature'.",
          "Permissions Policy: Unrecognized origin: 'data:///bad-origin'.",
      });
}

TEST_F(PermissionsPolicyParserParsingTest, CommaSeparatorInAttribute) {
  PolicyParserMessageBuffer logger;

  CheckParsedPolicy(
      PermissionsPolicyParser::ParseAttribute(
          "geolocation 'none', fullscreen 'self'",
          /* self_origin */ origin_a_.get(),
          /* src_origin */ origin_a_.get(), logger, /* context */ nullptr),
      {
          {
              mojom::blink::PermissionsPolicyFeature::kGeolocation,
              /* self_if_matches */ ORIGIN_A,
              /* matches_all_origins */ false,
              /* matches_opaque_src */ false,
              {},
          },
      });

  EXPECT_EQ(logger.GetMessages().size(), 2u)
      << "Parser should report parsing error.";

  EXPECT_EQ(logger.GetMessages().front().content.Ascii(),
            "Unrecognized origin: ''none','.")
      << "\"'none',\" should be treated as an invalid allowlist item ";

  EXPECT_EQ(logger.GetMessages().back().content.Ascii(),
            "Unrecognized origin: 'fullscreen'.")
      << "\"fullscreen\" should be treated as an invalid allowlist item";
}

TEST_F(PermissionsPolicyParserTest, ParseValidHeaderPolicy) {
  for (const char* policy_string : kValidHeaderPolicies) {
    PolicyParserMessageBuffer logger;
    PermissionsPolicyParser::ParseFeaturePolicyForTest(
        policy_string, origin_a_.get(), nullptr, logger, test_feature_name_map);
    EXPECT_EQ(0UL, logger.GetMessages().size())
        << "Should parse " << policy_string;
  }
}

TEST_F(PermissionsPolicyParserTest, ParseInvalidHeaderPolicy) {
  for (const char* policy_string : kInvalidHeaderPolicies) {
    PolicyParserMessageBuffer logger;
    PermissionsPolicyParser::ParseFeaturePolicyForTest(
        policy_string, origin_a_.get(), nullptr, logger, test_feature_name_map);
    EXPECT_LT(0UL, logger.GetMessages().size())
        << "Should fail to parse " << policy_string;
  }
}

TEST_F(PermissionsPolicyParserTest, ParseTooLongPolicy) {
  PolicyParserMessageBuffer logger;
  auto policy_string = "geolocation http://" + std::string(1 << 17, 'a');
  PermissionsPolicyParser::ParseFeaturePolicyForTest(
      policy_string.c_str(), origin_a_.get(), origin_b_.get(), logger,
      test_feature_name_map);
  EXPECT_EQ(1UL, logger.GetMessages().size())
      << "Should fail to parse feature policy string with size "
      << policy_string.size();
  PermissionsPolicyParser::ParsePermissionsPolicyForTest(
      policy_string.c_str(), origin_a_.get(), origin_b_.get(), logger,
      test_feature_name_map);
  EXPECT_EQ(2UL, logger.GetMessages().size())
      << "Should fail to parse permissions policy string with size "
      << policy_string.size();
}

// Test histogram counting the use of permissions policies in header.
TEST_F(PermissionsPolicyParserTest, HeaderHistogram) {
  const char* histogram_name = "Blink.UseCounter.FeaturePolicy.Header";
  base::HistogramTester tester;
  PolicyParserMessageBuffer logger;

  PermissionsPolicyParser::ParseFeaturePolicyForTest(
      "payment; fullscreen", origin_a_.get(), nullptr, logger,
      test_feature_name_map);
  tester.ExpectTotalCount(histogram_name, 2);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::blink::PermissionsPolicyFeature::kPayment),
      1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(
          blink::mojom::blink::PermissionsPolicyFeature::kFullscreen),
      1);
}

// Test counting the use of each permissions policy only once per header.
TEST_F(PermissionsPolicyParserTest, HistogramMultiple) {
  const char* histogram_name = "Blink.UseCounter.FeaturePolicy.Header";
  base::HistogramTester tester;
  PolicyParserMessageBuffer logger;

  // If the same feature is listed multiple times, it should only be counted
  // once.
  PermissionsPolicyParser::ParseFeaturePolicyForTest(
      "geolocation 'self'; payment; geolocation *", origin_a_.get(), nullptr,
      logger, test_feature_name_map);
  PermissionsPolicyParser::ParseFeaturePolicyForTest(
      "fullscreen 'self', fullscreen *", origin_a_.get(), nullptr, logger,
      test_feature_name_map);
  tester.ExpectTotalCount(histogram_name, 3);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(
          blink::mojom::blink::PermissionsPolicyFeature::kGeolocation),
      1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(
          blink::mojom::blink::PermissionsPolicyFeature::kFullscreen),
      1);
}

// Tests the use counter for comma separator in declarations.
TEST_F(PermissionsPolicyParserTest, CommaSeparatedUseCounter) {
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
TEST_F(PermissionsPolicyParserTest, SemicolonSeparatedUseCounter) {
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
  bool IsFeatureAllowedEverywhere(
      mojom::blink::PermissionsPolicyFeature feature,
      const ParsedPermissionsPolicy& policy) {
    const auto& result = base::ranges::find(
        policy, feature, &ParsedPermissionsPolicyDeclaration::feature);
    if (result == policy.end())
      return false;

    return result->feature == feature && result->matches_all_origins &&
           result->matches_opaque_src && result->allowed_origins.empty();
  }

  // Returns true if the policy contains a declaration for the feature which
  // disallows it in all origins.
  bool IsFeatureDisallowedEverywhere(
      mojom::blink::PermissionsPolicyFeature feature,
      const ParsedPermissionsPolicy& policy) {
    const auto& result = base::ranges::find(
        policy, feature, &ParsedPermissionsPolicyDeclaration::feature);
    if (result == policy.end())
      return false;

    return result->feature == feature && !result->matches_all_origins &&
           !result->matches_opaque_src && result->allowed_origins.empty();
  }

  ParsedPermissionsPolicy test_policy = {
      {mojom::blink::PermissionsPolicyFeature::kFullscreen,
       /*allowed_origins=*/
       {*blink::OriginWithPossibleWildcards::FromOrigin(url_origin_a_),
        *blink::OriginWithPossibleWildcards::FromOrigin(url_origin_b_)},
       /*self_if_matches=*/std::nullopt,
       /*matches_all_origins=*/false,
       /*matches_opaque_src=*/false},
      {mojom::blink::PermissionsPolicyFeature::kGeolocation,
       /*=allowed_origins*/
       {*blink::OriginWithPossibleWildcards::FromOrigin(url_origin_a_)},
       /*self_if_matches=*/std::nullopt,
       /*matches_all_origins=*/false,
       /*matches_opaque_src=*/false}};

  ParsedPermissionsPolicy empty_policy = {};
  test::TaskEnvironment task_environment_;
};

TEST_F(FeaturePolicyMutationTest, TestIsFeatureDeclared) {
  EXPECT_TRUE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kFullscreen, test_policy));
  EXPECT_TRUE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kGeolocation, test_policy));
  EXPECT_FALSE(IsFeatureDeclared(mojom::blink::PermissionsPolicyFeature::kUsb,
                                 test_policy));
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kNotFound, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestIsFeatureDeclaredWithEmptyPolicy) {
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kFullscreen, empty_policy));
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kNotFound, empty_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveAbsentFeature) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kPayment, test_policy));
  EXPECT_FALSE(RemoveFeatureIfPresent(
      mojom::blink::PermissionsPolicyFeature::kPayment, test_policy));
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kPayment, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveFromEmptyPolicy) {
  ASSERT_EQ(0UL, empty_policy.size());
  EXPECT_FALSE(RemoveFeatureIfPresent(
      mojom::blink::PermissionsPolicyFeature::kPayment, test_policy));
  ASSERT_EQ(0UL, empty_policy.size());
}

TEST_F(FeaturePolicyMutationTest, TestRemoveFeatureIfPresent) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_TRUE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kFullscreen, test_policy));
  EXPECT_TRUE(RemoveFeatureIfPresent(
      mojom::blink::PermissionsPolicyFeature::kFullscreen, test_policy));
  EXPECT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kFullscreen, test_policy));

  // Attempt to remove the feature again
  EXPECT_FALSE(RemoveFeatureIfPresent(
      mojom::blink::PermissionsPolicyFeature::kFullscreen, test_policy));
  EXPECT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kFullscreen, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveFeatureIfPresentOnSecondFeature) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_TRUE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kGeolocation, test_policy));
  EXPECT_TRUE(RemoveFeatureIfPresent(
      mojom::blink::PermissionsPolicyFeature::kGeolocation, test_policy));
  ASSERT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kGeolocation, test_policy));

  // Attempt to remove the feature again
  EXPECT_FALSE(RemoveFeatureIfPresent(
      mojom::blink::PermissionsPolicyFeature::kGeolocation, test_policy));
  EXPECT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kGeolocation, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveAllFeatures) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_TRUE(RemoveFeatureIfPresent(
      mojom::blink::PermissionsPolicyFeature::kFullscreen, test_policy));
  EXPECT_TRUE(RemoveFeatureIfPresent(
      mojom::blink::PermissionsPolicyFeature::kGeolocation, test_policy));
  EXPECT_EQ(0UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kFullscreen, test_policy));
  EXPECT_FALSE(IsFeatureDeclared(
      mojom::blink::PermissionsPolicyFeature::kGeolocation, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestDisallowIfNotPresent) {
  ParsedPermissionsPolicy copy = test_policy;
  // Try to disallow a feature which already exists
  EXPECT_FALSE(DisallowFeatureIfNotPresent(
      mojom::blink::PermissionsPolicyFeature::kFullscreen, copy));
  ASSERT_EQ(copy, test_policy);

  // Disallow a new feature
  EXPECT_TRUE(DisallowFeatureIfNotPresent(
      mojom::blink::PermissionsPolicyFeature::kPayment, copy));
  EXPECT_EQ(3UL, copy.size());
  // Verify that the feature is, in fact, now disallowed everywhere
  EXPECT_TRUE(IsFeatureDisallowedEverywhere(
      mojom::blink::PermissionsPolicyFeature::kPayment, copy));
}

TEST_F(FeaturePolicyMutationTest, TestAllowEverywhereIfNotPresent) {
  ParsedPermissionsPolicy copy = test_policy;
  // Try to allow a feature which already exists
  EXPECT_FALSE(AllowFeatureEverywhereIfNotPresent(
      mojom::blink::PermissionsPolicyFeature::kFullscreen, copy));
  ASSERT_EQ(copy, test_policy);

  // Allow a new feature
  EXPECT_TRUE(AllowFeatureEverywhereIfNotPresent(
      mojom::blink::PermissionsPolicyFeature::kPayment, copy));
  EXPECT_EQ(3UL, copy.size());
  // Verify that the feature is, in fact, allowed everywhere
  EXPECT_TRUE(IsFeatureAllowedEverywhere(
      mojom::blink::PermissionsPolicyFeature::kPayment, copy));
}

TEST_F(FeaturePolicyMutationTest, TestDisallowUnconditionally) {
  // Try to disallow a feature which already exists
  DisallowFeature(mojom::blink::PermissionsPolicyFeature::kFullscreen,
                  test_policy);
  // Should not have changed the number of declarations
  EXPECT_EQ(2UL, test_policy.size());
  // Verify that the feature is, in fact, now disallowed everywhere
  EXPECT_TRUE(IsFeatureDisallowedEverywhere(
      mojom::blink::PermissionsPolicyFeature::kFullscreen, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestDisallowNewFeatureUnconditionally) {
  // Try to disallow a feature which does not yet exist
  DisallowFeature(mojom::blink::PermissionsPolicyFeature::kPayment,
                  test_policy);
  // Should have added a new declaration
  EXPECT_EQ(3UL, test_policy.size());
  // Verify that the feature is, in fact, now disallowed everywhere
  EXPECT_TRUE(IsFeatureDisallowedEverywhere(
      mojom::blink::PermissionsPolicyFeature::kPayment, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestAllowUnconditionally) {
  // Try to allow a feature which already exists
  AllowFeatureEverywhere(mojom::blink::PermissionsPolicyFeature::kFullscreen,
                         test_policy);
  // Should not have changed the number of declarations
  EXPECT_EQ(2UL, test_policy.size());
  // Verify that the feature is, in fact, now allowed everywhere
  EXPECT_TRUE(IsFeatureAllowedEverywhere(
      mojom::blink::PermissionsPolicyFeature::kFullscreen, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestAllowNewFeatureUnconditionally) {
  // Try to allow a feature which does not yet exist
  AllowFeatureEverywhere(mojom::blink::PermissionsPolicyFeature::kPayment,
                         test_policy);
  // Should have added a new declaration
  EXPECT_EQ(3UL, test_policy.size());
  // Verify that the feature is, in fact, now allowed everywhere
  EXPECT_TRUE(IsFeatureAllowedEverywhere(
      mojom::blink::PermissionsPolicyFeature::kPayment, test_policy));
}

class FeaturePolicyVisibilityTest
    : public testing::Test,
      public testing::WithParamInterface</*is_isolated=*/bool> {
 public:
  FeaturePolicyVisibilityTest() : is_isolated_(GetParam()) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kDirectSockets},
        /*disabled_features=*/{});
  }

  bool GetIsIsolated() { return is_isolated_; }

 private:
  test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  bool is_isolated_{false};
};

INSTANTIATE_TEST_SUITE_P(All, FeaturePolicyVisibilityTest, testing::Bool());

TEST_P(FeaturePolicyVisibilityTest, VerifyIsolated) {
  EXPECT_TRUE(RuntimeEnabledFeatures::ControlledFrameEnabled());
  EXPECT_TRUE(RuntimeEnabledFeatures::DirectSocketsEnabled());

  auto dummy_page_holder = std::make_unique<DummyPageHolder>();
  ExecutionContext* execution_context =
      dummy_page_holder->GetFrame().DomWindow();

  Agent::ResetIsIsolatedContextForTest();
  Agent::SetIsIsolatedContext(GetIsIsolated());
  bool is_isolated_context = execution_context->IsIsolatedContext();
  EXPECT_EQ(is_isolated_context, GetIsIsolated());

  const String kControlledFrameFeature = "controlled-frame";
  EXPECT_EQ(GetDefaultFeatureNameMap(is_isolated_context)
                .Contains(kControlledFrameFeature),
            GetIsIsolated());

  const String kDirectSocketsFeature = "direct-sockets";
  EXPECT_EQ(GetDefaultFeatureNameMap(is_isolated_context)
                .Contains(kDirectSocketsFeature),
            GetIsIsolated());
}

}  // namespace blink

