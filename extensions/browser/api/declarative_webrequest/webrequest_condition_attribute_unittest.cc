// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/declarative_webrequest/webrequest_condition_attribute.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/values.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_condition.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace keys = declarative_webrequest_constants;

namespace {
const char kUnknownConditionName[] = "unknownType";

TEST(WebRequestConditionAttributeTest, CreateConditionAttribute) {
  std::string error;
  scoped_refptr<const WebRequestConditionAttribute> result;
  base::Value string_value("main_frame");
  base::Value resource_types(base::Value::Type::LIST);
  resource_types.GetList().Append("main_frame");

  // Test wrong condition name passed.
  error.clear();
  result = WebRequestConditionAttribute::Create(
      kUnknownConditionName, &resource_types, &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test wrong data type passed
  error.clear();
  result = WebRequestConditionAttribute::Create(
      keys::kResourceTypeKey, &string_value, &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  error.clear();
  result = WebRequestConditionAttribute::Create(
      keys::kContentTypeKey, &string_value, &error);
  EXPECT_FALSE(error.empty());
  EXPECT_FALSE(result.get());

  // Test success
  error.clear();
  result = WebRequestConditionAttribute::Create(
      keys::kResourceTypeKey, &resource_types, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(WebRequestConditionAttribute::CONDITION_RESOURCE_TYPE,
            result->GetType());
  EXPECT_EQ(std::string(keys::kResourceTypeKey), result->GetName());
}

TEST(WebRequestConditionAttributeTest, ResourceType) {
  std::string error;
  base::Value resource_types(base::Value::Type::LIST);
  resource_types.GetList().Append("sub_frame");

  scoped_refptr<const WebRequestConditionAttribute> attribute =
      WebRequestConditionAttribute::Create(
          keys::kResourceTypeKey, &resource_types, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(attribute.get());
  EXPECT_EQ(std::string(keys::kResourceTypeKey), attribute->GetName());

  WebRequestInfoInitParams ok_params;
  ok_params.web_request_type = WebRequestResourceType::SUB_FRAME;
  WebRequestInfo request_ok_info(std::move(ok_params));
  EXPECT_TRUE(attribute->IsFulfilled(
      WebRequestData(&request_ok_info, ON_BEFORE_REQUEST)));

  WebRequestInfoInitParams fail_params;
  fail_params.web_request_type = WebRequestResourceType::MAIN_FRAME;
  WebRequestInfo request_fail_info(std::move(fail_params));
  EXPECT_FALSE(attribute->IsFulfilled(
      WebRequestData(&request_fail_info, ON_BEFORE_REQUEST)));
}

TEST(WebRequestConditionAttributeTest, ContentType) {
  std::string error;
  scoped_refptr<const WebRequestConditionAttribute> result;

  WebRequestInfo request_info(WebRequestInfoInitParams{});
  auto response_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders("HTTP/1.1 200 OK\r\n"
                                        "Content-Type: text/plain; UTF-8\r\n"));

  base::Value content_types(base::Value::Type::LIST);
  base::Value::List& content_types_list = content_types.GetList();
  content_types_list.Append("text/plain");
  scoped_refptr<const WebRequestConditionAttribute> attribute_include =
      WebRequestConditionAttribute::Create(
          keys::kContentTypeKey, &content_types, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(attribute_include.get());
  EXPECT_FALSE(attribute_include->IsFulfilled(WebRequestData(
      &request_info, ON_BEFORE_REQUEST, response_headers.get())));
  EXPECT_TRUE(attribute_include->IsFulfilled(WebRequestData(
      &request_info, ON_HEADERS_RECEIVED, response_headers.get())));
  EXPECT_EQ(std::string(keys::kContentTypeKey), attribute_include->GetName());

  scoped_refptr<const WebRequestConditionAttribute> attribute_exclude =
      WebRequestConditionAttribute::Create(
          keys::kExcludeContentTypeKey, &content_types, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(attribute_exclude.get());
  EXPECT_FALSE(attribute_exclude->IsFulfilled(WebRequestData(
      &request_info, ON_HEADERS_RECEIVED, response_headers.get())));

  content_types_list.clear();
  content_types_list.Append("something/invalid");
  scoped_refptr<const WebRequestConditionAttribute> attribute_unincluded =
      WebRequestConditionAttribute::Create(
          keys::kContentTypeKey, &content_types, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(attribute_unincluded.get());
  EXPECT_FALSE(attribute_unincluded->IsFulfilled(WebRequestData(
      &request_info, ON_HEADERS_RECEIVED, response_headers.get())));

  scoped_refptr<const WebRequestConditionAttribute> attribute_unexcluded =
      WebRequestConditionAttribute::Create(
          keys::kExcludeContentTypeKey, &content_types, &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(attribute_unexcluded.get());
  EXPECT_TRUE(attribute_unexcluded->IsFulfilled(WebRequestData(
      &request_info, ON_HEADERS_RECEIVED, response_headers.get())));
  EXPECT_EQ(std::string(keys::kExcludeContentTypeKey),
            attribute_unexcluded->GetName());
}

// Testing WebRequestConditionAttributeStages. This iterates over all stages,
// and tests a couple of "stage" attributes -- one created with an empty set of
// applicable stages, one for each stage applicable for that stage, and one
// applicable in all stages.
TEST(WebRequestConditionAttributeTest, Stages) {
  typedef std::pair<RequestStage, const char*> StageNamePair;
  static const StageNamePair active_stages[] = {
    StageNamePair(ON_BEFORE_REQUEST, keys::kOnBeforeRequestEnum),
    StageNamePair(ON_BEFORE_SEND_HEADERS, keys::kOnBeforeSendHeadersEnum),
    StageNamePair(ON_HEADERS_RECEIVED, keys::kOnHeadersReceivedEnum),
    StageNamePair(ON_AUTH_REQUIRED, keys::kOnAuthRequiredEnum)
  };

  // Check that exactly all active stages are considered in this test.
  unsigned int covered_stages = 0;
  for (size_t i = 0; i < std::size(active_stages); ++i)
    covered_stages |= active_stages[i].first;
  EXPECT_EQ(kActiveStages, covered_stages);

  std::string error;

  // Create an attribute with an empty set of applicable stages.
  base::Value empty_list(base::Value::Type::LIST);
  scoped_refptr<const WebRequestConditionAttribute> empty_attribute =
      WebRequestConditionAttribute::Create(keys::kStagesKey,
                                           &empty_list,
                                           &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(empty_attribute.get());
  EXPECT_EQ(std::string(keys::kStagesKey), empty_attribute->GetName());

  // Create an attribute with all possible applicable stages.
  base::Value all_stages(base::Value::Type::LIST);
  for (size_t i = 0; i < std::size(active_stages); ++i)
    all_stages.GetList().Append(active_stages[i].second);
  scoped_refptr<const WebRequestConditionAttribute> attribute_with_all =
      WebRequestConditionAttribute::Create(keys::kStagesKey,
                                           &all_stages,
                                           &error);
  EXPECT_EQ("", error);
  ASSERT_TRUE(attribute_with_all.get());
  EXPECT_EQ(std::string(keys::kStagesKey), attribute_with_all->GetName());

  // Create one attribute for each single stage, to be applicable in that stage.
  std::vector<scoped_refptr<const WebRequestConditionAttribute> >
      one_stage_attributes;

  for (size_t i = 0; i < std::size(active_stages); ++i) {
    base::Value single_stage_list(base::Value::Type::LIST);
    single_stage_list.GetList().Append(active_stages[i].second);
    one_stage_attributes.push_back(
        WebRequestConditionAttribute::Create(keys::kStagesKey,
                                             &single_stage_list,
                                             &error));
    EXPECT_EQ("", error);
    ASSERT_TRUE(one_stage_attributes.back().get() != nullptr);
  }

  WebRequestInfo request_info(WebRequestInfoInitParams{});

  for (size_t i = 0; i < std::size(active_stages); ++i) {
    EXPECT_FALSE(empty_attribute->IsFulfilled(
        WebRequestData(&request_info, active_stages[i].first)));

    for (size_t j = 0; j < one_stage_attributes.size(); ++j) {
      EXPECT_EQ(i == j, one_stage_attributes[j]->IsFulfilled(WebRequestData(
                            &request_info, active_stages[i].first)));
    }

    EXPECT_TRUE(attribute_with_all->IsFulfilled(
        WebRequestData(&request_info, active_stages[i].first)));
  }
}

namespace {

// Builds a vector of vectors of string pointers from an array of strings.
// |array| is in fact a sequence of arrays. The array |sizes| captures the sizes
// of all parts of |array|, and |size| is the length of |sizes| itself.
// Example (this is pseudo-code, not C++):
// array = { "a", "b", "c", "d", "e", "f" }
// sizes = { 2, 0, 4 }
// size = 3
// results in out == { {&"a", &"b"}, {}, {&"c", &"d", &"e", &"f"} }
void GetArrayAsVector(const std::string array[],
                      const size_t sizes[],
                      const size_t size,
                      std::vector< std::vector<const std::string*> >* out) {
  out->clear();
  size_t next = 0;
  for (size_t i = 0; i < size; ++i) {
    out->push_back(std::vector<const std::string*>());
    for (size_t j = next; j < next + sizes[i]; ++j) {
      out->back().push_back(&(array[j]));
    }
    next += sizes[i];
  }
}

// Builds a base::Value::Dict from an array of the form {name1, value1, name2,
// value2, ...}. Values for the same key are grouped in a List.
base::Value::Dict GetDictFromArray(
    const std::vector<const std::string*>& array) {
  const size_t length = array.size();
  CHECK(length % 2 == 0);

  base::Value::Dict dict;
  for (size_t i = 0; i < length; i += 2) {
    const std::string* name = array[i];
    const std::string* value = array[i+1];
    if (base::Value* entry = dict.Find(*name)) {
      std::optional<base::Value> entry_owned;
      switch (entry->type()) {
        case base::Value::Type::STRING: {
          // Replace the present string with a list.
          base::Value::List list;
          // No need to check again, we already verified the entry is there.
          entry_owned = dict.Extract(*name);
          list.Append(std::move(*entry_owned));
          list.Append(*value);
          dict.Set(*name, std::move(list));
          break;
        }
        case base::Value::Type::LIST:  // Just append to the list.
          entry->GetList().Append(*value);
          break;
        default:
          NOTREACHED_IN_MIGRATION();  // We never put other Values here.
          return base::Value::Dict();
      }
    } else {
      dict.Set(*name, *value);
    }
  }
  return dict;
}

// Returns whether the response headers from |request_info| satisfy the match
// criteria given in |tests|. For at least one |i| all tests from |tests[i]|
// must pass.
void MatchAndCheck(const std::vector<std::vector<const std::string*>>& tests,
                   const std::string& key,
                   RequestStage stage,
                   const WebRequestInfo& request_info,
                   bool* result) {
  base::Value contains_headers(base::Value::Type::LIST);
  for (const auto& test : tests) {
    contains_headers.GetList().Append(GetDictFromArray(test));
  }

  std::string error;
  scoped_refptr<const WebRequestConditionAttribute> attribute =
      WebRequestConditionAttribute::Create(key, &contains_headers, &error);
  ASSERT_EQ("", error);
  ASSERT_TRUE(attribute.get());
  EXPECT_EQ(key, attribute->GetName());

  *result = attribute->IsFulfilled(WebRequestData(
      &request_info, stage, request_info.response_headers.get()));
}

}  // namespace

// Here we test WebRequestConditionAttributeRequestHeaders for matching
// correctly against request headers. This test is not as extensive as
// "ResponseHeaders" (below), because the header-matching code is shared
// by both types of condition attributes, so it is enough to test it once.
TEST(WebRequestConditionAttributeTest, RequestHeaders) {
  WebRequestInfoInitParams params;
  params.extra_request_headers.SetHeader("Custom-header", "custom/value");
  WebRequestInfo request_info(std::move(params));

  std::vector<std::vector<const std::string*> > tests;
  bool result = false;

  const RequestStage stage = ON_BEFORE_SEND_HEADERS;

  // First set of test data -- passing conjunction.
  const std::string kPassingCondition[] = {
    keys::kNameContainsKey, "CuStOm",  // Header names are case insensitive.
    keys::kNameEqualsKey, "custom-header",
    keys::kValueSuffixKey, "alue",
    keys::kValuePrefixKey, "custom/value"
  };
  const size_t kPassingConditionSizes[] = {std::size(kPassingCondition)};
  GetArrayAsVector(kPassingCondition, kPassingConditionSizes, 1u, &tests);
  // Positive filter, passing (conjunction of tests).
  MatchAndCheck(tests, keys::kRequestHeadersKey, stage, request_info, &result);
  EXPECT_TRUE(result);
  // Negative filter, failing (conjunction of tests).
  MatchAndCheck(tests, keys::kExcludeRequestHeadersKey, stage, request_info,
                &result);
  EXPECT_FALSE(result);

  // Second set of test data -- failing disjunction.
  const std::string kFailCondition[] = {
    keys::kNameSuffixKey, "Custom",      // Test 1.
    keys::kNameEqualsKey, "ustom-valu",  // Test 2.
    keys::kValuePrefixKey, "custom ",    // Test 3.
    keys::kValueContainsKey, " value"    // Test 4.
  };
  const size_t kFailConditionSizes[] = { 2u, 2u, 2u, 2u };
  GetArrayAsVector(kFailCondition, kFailConditionSizes, 4u, &tests);
  // Positive filter, failing (disjunction of tests).
  MatchAndCheck(tests, keys::kRequestHeadersKey, stage, request_info, &result);
  EXPECT_FALSE(result);
  // Negative filter, passing (disjunction of tests).
  MatchAndCheck(tests, keys::kExcludeRequestHeadersKey, stage, request_info,
                &result);
  EXPECT_TRUE(result);

  // Third set of test data, corner case -- empty disjunction.
  GetArrayAsVector(nullptr, nullptr, 0u, &tests);
  // Positive filter, failing (no test to pass).
  MatchAndCheck(tests, keys::kRequestHeadersKey, stage, request_info, &result);
  EXPECT_FALSE(result);
  // Negative filter, passing (no test to fail).
  MatchAndCheck(tests, keys::kExcludeRequestHeadersKey, stage, request_info,
                &result);
  EXPECT_TRUE(result);

  // Fourth set of test data, corner case -- empty conjunction.
  const size_t kEmptyConjunctionSizes[] = { 0u };
  GetArrayAsVector(nullptr, kEmptyConjunctionSizes, 1u, &tests);
  // Positive filter, passing (trivial test).
  MatchAndCheck(tests, keys::kRequestHeadersKey, stage, request_info, &result);
  EXPECT_TRUE(result);
  // Negative filter, failing.
  MatchAndCheck(tests, keys::kExcludeRequestHeadersKey, stage, request_info,
                &result);
  EXPECT_FALSE(result);
}

// Here we test WebRequestConditionAttributeResponseHeaders for:
// 1. Correct implementation of prefix/suffix/contains/equals matching.
// 2. Performing logical disjunction (||) between multiple specifications.
// 3. Negating the match in case of 'doesNotContainHeaders'.
TEST(WebRequestConditionAttributeTest, ResponseHeaders) {
  ExtensionsAPIClient api_client;
  WebRequestInfo request_info(WebRequestInfoInitParams{});
  request_info.response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          net::HttpUtil::AssembleRawHeaders(
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain; UTF-8\r\n"
              "Custom-Header: custom/value\r\n"
              "Custom-Header-B: valueA\r\n"
              "Custom-Header-B: valueB\r\n"
              "Custom-Header-C: valueC, valueD\r\n"
              "Custom-Header-D:\r\n"));

  // In all the tests below we assume that the server includes the headers
  // Custom-Header: custom/value
  // Custom-Header-B: valueA
  // Custom-Header-B: valueB
  // Custom-Header-C: valueC, valueD
  // Custom-Header-D:
  // in the response, but does not include "Non-existing: void".

  std::vector< std::vector<const std::string*> > tests;
  bool result;

  const RequestStage stage = ON_HEADERS_RECEIVED;

  // 1.a. -- All these tests should pass.
  const std::string kPassingCondition[] = {
    keys::kNamePrefixKey, "Custom",
    keys::kNameSuffixKey, "m-header",  // Header names are case insensitive.
    keys::kValueContainsKey, "alu",
    keys::kValueEqualsKey, "custom/value"
  };
  const size_t kPassingConditionSizes[] = {std::size(kPassingCondition)};
  GetArrayAsVector(kPassingCondition, kPassingConditionSizes, 1u, &tests);
  MatchAndCheck(tests, keys::kResponseHeadersKey, stage, request_info, &result);
  EXPECT_TRUE(result);

  // 1.b. -- None of the following tests in the discjunction should pass.
  const std::string kFailCondition[] = {
    keys::kNamePrefixKey, " Custom",  // Test 1.
    keys::kNameContainsKey, " -",     // Test 2.
    keys::kValueSuffixKey, "alu",     // Test 3.
    keys::kValueEqualsKey, "custom"   // Test 4.
  };
  const size_t kFailConditionSizes[] = { 2u, 2u, 2u, 2u };
  GetArrayAsVector(kFailCondition, kFailConditionSizes, 4u, &tests);
  MatchAndCheck(tests, keys::kResponseHeadersKey, stage, request_info, &result);
  EXPECT_FALSE(result);

  // 1.c. -- This should fail (mixing name and value from different headers)
  const std::string kMixingCondition[] = {
    keys::kNameSuffixKey, "Header-B",
    keys::kValueEqualsKey, "custom/value"
  };
  const size_t kMixingConditionSizes[] = {std::size(kMixingCondition)};
  GetArrayAsVector(kMixingCondition, kMixingConditionSizes, 1u, &tests);
  MatchAndCheck(tests, keys::kResponseHeadersKey, stage, request_info, &result);
  EXPECT_FALSE(result);

  // 1.d. -- Test handling multiple values for one header (both should pass).
  const std::string kMoreValues1[] = {
    keys::kNameEqualsKey, "Custom-header-b",
    keys::kValueEqualsKey, "valueA"
  };
  const size_t kMoreValues1Sizes[] = {std::size(kMoreValues1)};
  GetArrayAsVector(kMoreValues1, kMoreValues1Sizes, 1u, &tests);
  MatchAndCheck(tests, keys::kResponseHeadersKey, stage, request_info, &result);
  EXPECT_TRUE(result);
  const std::string kMoreValues2[] = {
    keys::kNameEqualsKey, "Custom-header-b",
    keys::kValueEqualsKey, "valueB"
  };
  const size_t kMoreValues2Sizes[] = {std::size(kMoreValues2)};
  GetArrayAsVector(kMoreValues2, kMoreValues2Sizes, 1u, &tests);
  MatchAndCheck(tests, keys::kResponseHeadersKey, stage, request_info, &result);
  EXPECT_TRUE(result);

  // 1.e. -- This should fail as conjunction but pass as disjunction.
  const std::string kConflict[] = {
    keys::kNameSuffixKey, "Header",      // True for some header.
    keys::kNameContainsKey, "Header-B"   // True for a different header.
  };
  // First disjunction, no conflict.
  const size_t kNoConflictSizes[] = { 2u, 2u };
  GetArrayAsVector(kConflict, kNoConflictSizes, 2u, &tests);
  MatchAndCheck(tests, keys::kResponseHeadersKey, stage, request_info, &result);
  EXPECT_TRUE(result);
  // Then conjunction, conflict.
  const size_t kConflictSizes[] = {std::size(kConflict)};
  GetArrayAsVector(kConflict, kConflictSizes, 1u, &tests);
  MatchAndCheck(tests, keys::kResponseHeadersKey, stage, request_info, &result);
  EXPECT_FALSE(result);

  // 1.f. -- This should pass, checking for correct treatment of ',' in values.
  const std::string kComma[] = {
    keys::kNameSuffixKey, "Header-C",
    keys::kValueEqualsKey, "valueC, valueD"
  };
  const size_t kCommaSizes[] = {std::size(kComma)};
  GetArrayAsVector(kComma, kCommaSizes, 1u, &tests);
  MatchAndCheck(tests, keys::kResponseHeadersKey, stage, request_info, &result);
  EXPECT_TRUE(result);

  // 1.g. -- This should pass, empty values are values as well.
  const std::string kEmpty[] = {
    keys::kNameEqualsKey, "custom-header-d",
    keys::kValueEqualsKey, ""
  };
  const size_t kEmptySizes[] = {std::size(kEmpty)};
  GetArrayAsVector(kEmpty, kEmptySizes, 1u, &tests);
  MatchAndCheck(tests, keys::kResponseHeadersKey, stage, request_info, &result);
  EXPECT_TRUE(result);

  // 1.h. -- Values are case-sensitive, this should fail.
  const std::string kLowercase[] = {
    keys::kNameEqualsKey, "Custom-header-b",
    keys::kValuePrefixKey, "valueb",  // valueb != valueB
    keys::kNameEqualsKey, "Custom-header-b",
    keys::kValueSuffixKey, "valueb",
    keys::kNameEqualsKey, "Custom-header-b",
    keys::kValueContainsKey, "valueb",
    keys::kNameEqualsKey, "Custom-header-b",
    keys::kValueEqualsKey, "valueb"
  };
  const size_t kLowercaseSizes[] = { 4u, 4u, 4u, 4u };  // As disjunction.
  GetArrayAsVector(kLowercase, kLowercaseSizes, 4u, &tests);
  MatchAndCheck(tests, keys::kResponseHeadersKey, stage, request_info, &result);
  EXPECT_FALSE(result);

  // 1.i. -- Names are case-insensitive, this should pass.
  const std::string kUppercase[] = {
    keys::kNamePrefixKey, "CUSTOM-HEADER-B",
    keys::kNameSuffixKey, "CUSTOM-HEADER-B",
    keys::kNameEqualsKey, "CUSTOM-HEADER-B",
    keys::kNameContainsKey, "CUSTOM-HEADER-B"
  };
  const size_t kUppercaseSizes[] = {std::size(kUppercase)};  // Conjunction.
  GetArrayAsVector(kUppercase, kUppercaseSizes, 1u, &tests);
  MatchAndCheck(tests, keys::kResponseHeadersKey, stage, request_info, &result);
  EXPECT_TRUE(result);

  // 2.a. -- This should pass as disjunction, because one of the tests passes.
  const std::string kDisjunction[] = {
    keys::kNamePrefixKey, "Non-existing",  // This one fails.
    keys::kNameSuffixKey, "Non-existing",  // This one fails.
    keys::kValueEqualsKey, "void",         // This one fails.
    keys::kValueContainsKey, "alu"         // This passes.
  };
  const size_t kDisjunctionSizes[] = { 2u, 2u, 2u, 2u };
  GetArrayAsVector(kDisjunction, kDisjunctionSizes, 4u, &tests);
  MatchAndCheck(tests, keys::kResponseHeadersKey, stage, request_info, &result);
  EXPECT_TRUE(result);

  // 3.a. -- This should pass.
  const std::string kNonExistent[] = {
    keys::kNameEqualsKey, "Non-existing",
    keys::kValueEqualsKey, "void"
  };
  const size_t kNonExistentSizes[] = {std::size(kNonExistent)};
  GetArrayAsVector(kNonExistent, kNonExistentSizes, 1u, &tests);
  MatchAndCheck(tests, keys::kExcludeResponseHeadersKey, stage, request_info,
                &result);
  EXPECT_TRUE(result);

  // 3.b. -- This should fail.
  const std::string kExisting[] = {
    keys::kNameEqualsKey, "custom-header-b",
    keys::kValueEqualsKey, "valueB"
  };
  const size_t kExistingSize[] = {std::size(kExisting)};
  GetArrayAsVector(kExisting, kExistingSize, 1u, &tests);
  MatchAndCheck(tests, keys::kExcludeResponseHeadersKey, stage, request_info,
                &result);
  EXPECT_FALSE(result);
}

TEST(WebRequestConditionAttributeTest, HideResponseHeaders) {
  const GURL url("http://a.com");
  WebRequestInfoInitParams params;
  params.url = url;
  WebRequestInfo request_info(std::move(params));
  request_info.response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(
          net::HttpUtil::AssembleRawHeaders(
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/plain; UTF-8\r\n"
              "Custom-Header: custom/value\r\n"));

  // In all the test below we assume that the server includes the headers
  // Custom-Header: custom/value

  std::vector<std::vector<const std::string*>> tests;
  bool result;
  const RequestStage stage = ON_HEADERS_RECEIVED;
  const std::string kCondition[] = {keys::kValueEqualsKey, "custom/value"};
  const size_t kConditionSizes[] = {std::size(kCondition)};
  GetArrayAsVector(kCondition, kConditionSizes, 1u, &tests);

  {
    // Default client does not hide the response header.
    ExtensionsAPIClient api_client;
    MatchAndCheck(tests, keys::kResponseHeadersKey, stage, request_info,
                  &result);
    EXPECT_TRUE(result);
  }

  {
    // Custom client hides the response header.
    class TestExtensionsAPIClient : public ExtensionsAPIClient {
     public:
      TestExtensionsAPIClient(const GURL& url) : url_(url) {}

     private:
      bool ShouldHideResponseHeader(
          const GURL& url,
          const std::string& header_name) const override {
        // Check that the client is called with the right URL.
        EXPECT_EQ(url_, url);
        // Hide the header.
        return header_name == "Custom-Header";
      }

      GURL url_;
    };

    TestExtensionsAPIClient api_client(url);
    MatchAndCheck(tests, keys::kResponseHeadersKey, stage, request_info,
                  &result);
    EXPECT_FALSE(result);
  }
}

}  // namespace
}  // namespace extensions
