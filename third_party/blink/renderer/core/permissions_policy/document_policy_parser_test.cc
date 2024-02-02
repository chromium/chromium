// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/permissions_policy/document_policy_parser.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions_policy/document_policy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-blink.h"

namespace blink {

constexpr const mojom::blink::DocumentPolicyFeature kDefault =
    mojom::blink::DocumentPolicyFeature::kDefault;
constexpr const mojom::blink::DocumentPolicyFeature kBoolFeature =
    static_cast<mojom::blink::DocumentPolicyFeature>(1);
constexpr const mojom::blink::DocumentPolicyFeature kDoubleFeature =
    static_cast<mojom::blink::DocumentPolicyFeature>(2);

// This is the test version of |PolicyParserMessageBuffer::Message| as
// WTF::String cannot be statically allocated.
struct MessageForTest {
  mojom::ConsoleMessageLevel level;
  const char* content;
};

struct ParseTestCase {
  const char* test_name;
  const char* input_string;
  DocumentPolicy::ParsedDocumentPolicy parsed_policy;
  std::vector<MessageForTest> messages;
};

class DocumentPolicyParserTest
    : public ::testing::TestWithParam<ParseTestCase> {
 protected:
  DocumentPolicyParserTest()
      : name_feature_map(DocumentPolicyNameFeatureMap{
            {"*", kDefault},
            {"f-bool", kBoolFeature},
            {"f-double", kDoubleFeature},
        }),
        feature_info_map(DocumentPolicyFeatureInfoMap{
            {kDefault, {"*", PolicyValue::CreateBool(true)}},
            {kBoolFeature, {"f-bool", PolicyValue::CreateBool(true)}},
            {kDoubleFeature, {"f-double", PolicyValue::CreateDecDouble(1.0)}},
        }) {
    available_features.insert(kBoolFeature);
    available_features.insert(kDoubleFeature);
  }

  ~DocumentPolicyParserTest() override = default;

  std::optional<DocumentPolicy::ParsedDocumentPolicy> Parse(
      const String& policy_string,
      PolicyParserMessageBuffer& logger) {
    return DocumentPolicyParser::ParseInternal(policy_string, name_feature_map,
                                               feature_info_map,
                                               available_features, logger);
  }

  std::optional<std::string> Serialize(
      const DocumentPolicyFeatureState& policy) {
    return DocumentPolicy::SerializeInternal(policy, feature_info_map);
  }

 private:
  const DocumentPolicyNameFeatureMap name_feature_map;
  const DocumentPolicyFeatureInfoMap feature_info_map;
  DocumentPolicyFeatureSet available_features;

 public:
  static const ParseTestCase kCases[];
};

const ParseTestCase DocumentPolicyParserTest::kCases[] = {
    //
    // Parse valid policy strings.
    //
    {
        "ParseEmptyPolicyString",
        "",
        /* parsed_policy */
        {
            /* feature_state */ {},
            /* endpoint_map */ {},
        },
        /* messages */ {},
    },
    {
        "ParseWhitespaceOnlyString",
        " ",
        /* parsed_policy */
        {
            /* feature_state */ {},
            /* endpoint_map */ {},
        },
        /* messages */ {},
    },
    {
        "ParseBoolFeatureWithValueTrue",
        "f-bool",
        /* parsed_policy */
        {
            /* feature_state */ {{kBoolFeature, PolicyValue::CreateBool(true)}},
            /* endpoint_map */ {},
        },
        /* messages */ {},
    },
    {
        "ParseBoolFeatureWithValueFalse",
        "f-bool=?0",
        /* parsed_policy */
        {
            /* feature_state */ {
                {kBoolFeature, PolicyValue::CreateBool(false)}},
            /* endpoint_map */ {},
        },
        /* messages */ {},
    },
    {
        "ParseDoubleFeature1",
        "f-double=1.0",
        /* parsed_policy */
        {
            /* feature_state */ {
                {kDoubleFeature, PolicyValue::CreateDecDouble(1.0)}},
            /* endpoint_map */ {},
        },
        /* messages */ {},
    },
    {
        "ParseDoubleFeature2",
        "f-double=2",
        /* parsed_policy */
        {
            /* feature_state */ {
                {kDoubleFeature, PolicyValue::CreateDecDouble(2.0)}},
            /* endpoint_map */ {},
        },
        /* messages */ {},
    },
    {
        "ParseDoubleFeatureAndBoolFeature",
        "f-double=1,f-bool=?0",
        /* parsed_policy */
        {
            /* feature_state */ {
                {kBoolFeature, PolicyValue::CreateBool(false)},
                {kDoubleFeature, PolicyValue::CreateDecDouble(1.0)}},
            /* endpoint_map */ {},
        },
        /* messages */ {},
    },
    {
        "ParseBoolFeatureAndDoubleFeature",
        "f-bool=?0,f-double=1",
        /* parsed_policy */
        {
            /* feature_state */ {
                {kBoolFeature, PolicyValue::CreateBool(false)},
                {kDoubleFeature, PolicyValue::CreateDecDouble(1.0)}},
            /* endpoint_map */ {},
        },
        /* messages */ {},
    },
    {
        "WhitespaceIsAllowedInSomePositionsInStructuredHeader",
        "f-bool=?0,   f-double=1",
        /* parsed_policy */
        {/* feature_state */ {
             {kBoolFeature, PolicyValue::CreateBool(false)},
             {kDoubleFeature, PolicyValue::CreateDecDouble(1.0)}},
         /* endpoint_map */ {}},
        /* messages */ {},
    },
    {
        "UnrecognizedParametersAreIgnoredButTheFeatureEntryShould"
        "RemainValid",
        "f-bool=?0,f-double=1;unknown_param=xxx",
        /* parsed_policy */
        {/* feature_state */ {
             {kBoolFeature, PolicyValue::CreateBool(false)},
             {kDoubleFeature, PolicyValue::CreateDecDouble(1.0)}},
         /* endpoint_map */ {}},
        /* messages */
        {{mojom::blink::ConsoleMessageLevel::kWarning,
          "Unrecognized parameter name unknown_param for feature f-double."}},
    },
    {
        "ParsePolicyWithReportEndpointSpecified1",
        "f-bool=?0,f-double=1;report-to=default",
        /* parsed_policy */
        {/* feature_state */ {
             {kBoolFeature, PolicyValue::CreateBool(false)},
             {kDoubleFeature, PolicyValue::CreateDecDouble(1.0)}},
         /* endpoint_map */ {{kDoubleFeature, "default"}}},
        /* messages */ {},
    },
    {
        "ParsePolicyWithReportEndpointSpecified2",
        "f-bool=?0;report-to=default,f-double=1",
        /* parsed_policy */
        {/* feature_state */ {
             {kBoolFeature, PolicyValue::CreateBool(false)},
             {kDoubleFeature, PolicyValue::CreateDecDouble(1.0)}},
         /* endpoint_map */ {{kBoolFeature, "default"}}},
        /* messages */ {},
    },
    {
        "ParsePolicyWithDefaultReportEndpointAndNone"
        "KeywordShouldOverwriteDefaultValue",
        "f-bool=?0;report-to=none, f-double=2.0, *;report-to=default",
        /* parsed_policy */
        {/* feature_state */ {
             {kBoolFeature, PolicyValue::CreateBool(false)},
             {kDoubleFeature, PolicyValue::CreateDecDouble(2.0)}},
         /* endpoint_map */ {{kDoubleFeature, "default"}}},
        /* messages */ {},
    },
    {
        "ParsePolicyWithDefaultReportEndpointSpecified",
        "f-bool=?0;report-to=not_none, f-double=2.0, "
        "*;report-to=default",
        /* parsed_policy */
        {/* feature_state */ {
             {kBoolFeature, PolicyValue::CreateBool(false)},
             {kDoubleFeature, PolicyValue::CreateDecDouble(2.0)}},
         /* endpoint_map */ {{kBoolFeature, "not_none"},
                             {kDoubleFeature, "default"}}},
        /* messages */ {},
    },
    {
        "ParsePolicyWithDefaultReportEndpointSpecifiedAsNone",
        "f-bool=?0;report-to=not_none, f-double=2.0, *;report-to=none",
        /* parsed_policy */
        {/* feature_state */ {
             {kBoolFeature, PolicyValue::CreateBool(false)},
             {kDoubleFeature, PolicyValue::CreateDecDouble(2.0)}},
         /* endpoint_map */ {{kBoolFeature, "not_none"}}},
        /* messages */ {},
    },
    {
        "DefaultEndpointCanBeSpecifiedAnywhereInTheHeader",
        "f-bool=?0;report-to=not_none, *;report-to=default, "
        "f-double=2.0",
        /* parsed_policy */
        {/* feature_state */ {
             {kBoolFeature, PolicyValue::CreateBool(false)},
             {kDoubleFeature, PolicyValue::CreateDecDouble(2.0)}},
         /* endpoint_map */ {{kBoolFeature, "not_none"},
                             {kDoubleFeature, "default"}}},
        /* messages */ {},
    },
    {
        "DefaultEndpointCanBeSpecifiedMultipleTimesInTheHeader",
        "f-bool=?0;report-to=not_none, f-double=2.0, "
        "*;report-to=default, *;report-to=none",
        /* parsed_policy */
        {/* feature_state */ {
             {kBoolFeature, PolicyValue::CreateBool(false)},
             {kDoubleFeature, PolicyValue::CreateDecDouble(2.0)}},
         /* endpoint_map */ {{kBoolFeature, "not_none"}}},
        /* messages */ {},
    },
    {
        "EvenIfDefaultEndpointIsNotSpecifiedNoneStillShouldBe"
        "TreatedAsReservedKeywordForEndpointNames",
        "f-bool=?0;report-to=none",
        /* parsed_policy */
        {/* feature_state */ {{kBoolFeature, PolicyValue::CreateBool(false)}},
         /* endpoint_map */ {}},
        /* messages */ {},
    },
    {
        "MissingEndpointGroupForDefaultFeature1",
        "*",
        /* parsed_policy */
        {
            /* feature_state */ {},
            /* endpoint_map */ {},
        },
        /* messages */ {},
    },
    {
        "MissingEndpointGroupForDefaultFeature2",
        "*,f-bool=?0,f-double=1;report-to=default",
        /* parsed_policy */
        {/* feature_state */ {
             {kBoolFeature, PolicyValue::CreateBool(false)},
             {kDoubleFeature, PolicyValue::CreateDecDouble(1.0)}},
         /* endpoint_map */ {{kDoubleFeature, "default"}}},
        /* messages */ {},
    },
    //
    // Parse invalid policies.
    //
    {
        "ParsePolicyWithUnrecognizedFeatureName1",
        "bad-feature-name",
        /* parsed_policy */
        {
            /* feature_state */ {},
            /* endpoint_map */ {},
        },
        /* messages */
        {{mojom::blink::ConsoleMessageLevel::kWarning,
          "Unrecognized document policy feature name "
          "bad-feature-name."}},
    },
    {
        "ParsePolicyWithUnrecognizedFeatureName2",
        "no-bad-feature-name",
        /* parsed_policy */
        {
            /* feature_state */ {},
            /* endpoint_map */ {},
        },
        /* messages */
        {{mojom::blink::ConsoleMessageLevel::kWarning,
          "Unrecognized document policy feature name "
          "no-bad-feature-name."}},
    },
    {
        "ParsePolicyWithWrongTypeOfParamExpectedDoubleTypeButGet"
        "BooleanType",
        "f-double=?0",
        /* parsed_policy */
        {
            /* feature_state */ {},
            /* endpoint_map */ {},
        },
        /* messages */
        {{mojom::blink::ConsoleMessageLevel::kWarning,
          "Parameter for feature f-double should be double, not "
          "boolean."}},
    },
    {
        "ParsePolicyWithWrongTypeOfParamExpectedBooleanTypeButGet"
        "DoubleType",
        "f-bool=1.0",
        /* parsed_policy */
        {
            /* feature_state */ {},
            /* endpoint_map */ {},
        },
        /* messages */
        {{mojom::blink::ConsoleMessageLevel::kWarning,
          "Parameter for feature f-bool should be boolean, not "
          "decimal."}},
    },
    {
        "FeatureValueItemShouldNotBeEmpty",
        "f-double=()",
        /* parsed_policy */
        {
            /* feature_state */ {},
            /* endpoint_map */ {},
        },
        /* messages */
        {{mojom::blink::ConsoleMessageLevel::kWarning,
          "Parameter for feature f-double should be single item, but get list "
          "of items(length=0)."}},
    },
    {
        "TooManyFeatureValueItems",
        "f-double=(1.1 2.0)",
        /* parsed_policy */
        {
            /* feature_state */ {},
            /* endpoint_map */ {},
        },
        /* messages */
        {{mojom::blink::ConsoleMessageLevel::kWarning,
          "Parameter for feature f-double should be single item, but get list "
          "of items(length=2)."}},
    },
    {
        "ReportToParameterValueTypeShouldBeTokenInsteadOf"
        "String",
        "f-bool;report-to=\"default\"",
        /* parsed_policy */
        {
            /* feature_state */ {},
            /* endpoint_map */ {},
        },
        /* messages */
        {{mojom::blink::ConsoleMessageLevel::kWarning,
          "\"report-to\" parameter should be a token in feature f-bool."}},
    },
};

const std::pair<DocumentPolicyFeatureState, std::string>
    kPolicySerializationTestCases[] = {
        {{{kBoolFeature, PolicyValue::CreateBool(false)},
          {kDoubleFeature, PolicyValue::CreateDecDouble(1.0)}},
         "f-bool=?0, f-double=1.0"},
        // Changing ordering of FeatureState element should not affect
        // serialization result.
        {{{kDoubleFeature, PolicyValue::CreateDecDouble(1.0)},
          {kBoolFeature, PolicyValue::CreateBool(false)}},
         "f-bool=?0, f-double=1.0"},
        // Flipping boolean-valued policy from false to true should not affect
        // result ordering of feature.
        {{{kBoolFeature, PolicyValue::CreateBool(true)},
          {kDoubleFeature, PolicyValue::CreateDecDouble(1.0)}},
         "f-bool, f-double=1.0"}};

const DocumentPolicyFeatureState kParsedPolicies[] = {
    {},  // An empty policy
    {{kBoolFeature, PolicyValue::CreateBool(false)}},
    {{kBoolFeature, PolicyValue::CreateBool(true)}},
    {{kDoubleFeature, PolicyValue::CreateDecDouble(1.0)}},
    {{kBoolFeature, PolicyValue::CreateBool(true)},
     {kDoubleFeature, PolicyValue::CreateDecDouble(1.0)}}};

// Serialize and then Parse the result of serialization should cancel each
// other out, i.e. d == Parse(Serialize(d)).
// The other way s == Serialize(Parse(s)) is not always true because structured
// header allows some optional white spaces in its parsing targets and floating
// point numbers will be rounded, e.g. value=1 will be parsed to
// PolicyValue::CreateDecDouble(1.0) and get serialized to value=1.0.
TEST_F(DocumentPolicyParserTest, SerializeAndParse) {
  for (const auto& policy : kParsedPolicies) {
    const std::optional<std::string> policy_string = Serialize(policy);
    ASSERT_TRUE(policy_string.has_value());
    PolicyParserMessageBuffer logger;
    const std::optional<DocumentPolicy::ParsedDocumentPolicy> reparsed_policy =
        Parse(policy_string.value().c_str(), logger);

    ASSERT_TRUE(reparsed_policy.has_value());
    EXPECT_EQ(reparsed_policy.value().feature_state, policy);
  }
}

TEST_F(DocumentPolicyParserTest, SerializeResultShouldMatch) {
  for (const auto& test_case : kPolicySerializationTestCases) {
    const DocumentPolicyFeatureState& policy = test_case.first;
    const std::string& expected = test_case.second;
    const auto result = Serialize(policy);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expected);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DocumentPolicyParserTest,
    ::testing::ValuesIn(DocumentPolicyParserTest::kCases),
    [](const ::testing::TestParamInfo<ParseTestCase>& param_info) {
      return param_info.param.test_name;
    });

TEST_P(DocumentPolicyParserTest, ParseResultShouldMatch) {
  const ParseTestCase& test_case = GetParam();
  PolicyParserMessageBuffer logger;

  const auto result = Parse(test_case.input_string, logger);

  // All tese cases should not return std::nullopt because they all comply to
  // structured header syntax.
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(result->endpoint_map, test_case.parsed_policy.endpoint_map)
      << "\n endpoint map should match";
  EXPECT_EQ(result->feature_state, test_case.parsed_policy.feature_state)
      << "\n feature state should match";
  EXPECT_EQ(logger.GetMessages().size(), test_case.messages.size())
      << "\n messages length should match";

  const auto& actual_messages = logger.GetMessages();
  const std::vector<MessageForTest>& expected_messages = test_case.messages;

  ASSERT_EQ(actual_messages.size(), expected_messages.size())
      << "message count should match";
  for (wtf_size_t i = 0; i < expected_messages.size(); ++i) {
    const auto& actual_message = actual_messages[i];
    const MessageForTest& expected_message = expected_messages[i];

    EXPECT_EQ(actual_message.level, expected_message.level)
        << "\n message level should match";
    EXPECT_EQ(actual_message.content, String(expected_message.content))
        << "\n message content should match";
  }
}

}  // namespace blink
