// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_no_vary_search_data.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "net/base/features.h"
#include "net/base/pickle.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/hash/hash_testing.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "url/gurl.h"

namespace net {

namespace {

using ::testing::Combine;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::UnorderedElementsAreArray;
using ::testing::Values;
using ::testing::ValuesIn;

TEST(HttpNoVarySearchCreateTest, CreateFromNoVaryParamsNonEmptyVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromNoVaryParams({"a"}, true);
  EXPECT_THAT(no_vary_search.affected_params(),
              UnorderedElementsAreArray({"a"}));
  EXPECT_TRUE(no_vary_search.vary_on_key_order());
  EXPECT_TRUE(no_vary_search.vary_by_default());
}

TEST(HttpNoVarySearchCreateTest,
     CreateFromNoVaryParamsNonEmptyNoVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromNoVaryParams({"a"}, false);
  EXPECT_THAT(no_vary_search.affected_params(),
              UnorderedElementsAreArray({"a"}));
  EXPECT_FALSE(no_vary_search.vary_on_key_order());
  EXPECT_TRUE(no_vary_search.vary_by_default());
}

TEST(HttpNoVarySearchCreateTest, CreateFromNoVaryParamsEmptyNoVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromNoVaryParams({}, false);
  EXPECT_THAT(no_vary_search.affected_params(), IsEmpty());
  EXPECT_FALSE(no_vary_search.vary_on_key_order());
  EXPECT_TRUE(no_vary_search.vary_by_default());
}

TEST(HttpNoVarySearchCreateTest, CreateFromVaryParamsNonEmptyVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromVaryParams({"a"}, true);
  EXPECT_THAT(no_vary_search.affected_params(),
              UnorderedElementsAreArray({"a"}));
  EXPECT_TRUE(no_vary_search.vary_on_key_order());
  EXPECT_FALSE(no_vary_search.vary_by_default());
}

TEST(HttpNoVarySearchCreateTest, CreateFromVaryParamsNonEmptyNoVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromVaryParams({"a"}, false);
  EXPECT_THAT(no_vary_search.affected_params(),
              UnorderedElementsAreArray({"a"}));
  EXPECT_FALSE(no_vary_search.vary_on_key_order());
  EXPECT_FALSE(no_vary_search.vary_by_default());
}

TEST(HttpNoVarySearchCreateTest, CreateFromVaryParamsEmptyNoVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromVaryParams({}, false);
  EXPECT_THAT(no_vary_search.affected_params(), IsEmpty());
  EXPECT_FALSE(no_vary_search.vary_on_key_order());
  EXPECT_FALSE(no_vary_search.vary_by_default());
}

TEST(HttpNoVarySearchCreateTest, CreateFromVaryParamsEmptyVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromVaryParams({}, true);
  EXPECT_THAT(no_vary_search.affected_params(), IsEmpty());
  EXPECT_TRUE(no_vary_search.vary_on_key_order());
  EXPECT_FALSE(no_vary_search.vary_by_default());
}

struct TestData {
  const char* raw_headers;
  const base::flat_set<std::string> expected_affected_params;
  const bool expected_vary_on_key_order;
  const bool expected_vary_by_default;
};

class HttpNoVarySearchResponseHeadersTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<TestData> {};

TEST_P(HttpNoVarySearchResponseHeadersTest, ParsingSuccess) {
  const TestData test = GetParam();

  const std::string raw_headers =
      HttpUtil::AssembleRawHeaders(test.raw_headers);

  const auto parsed = base::MakeRefCounted<HttpResponseHeaders>(raw_headers);
  ASSERT_OK_AND_ASSIGN(const auto no_vary_search_data,
                       HttpNoVarySearchData::ParseFromHeaders(*parsed));

  EXPECT_EQ(no_vary_search_data.vary_on_key_order(),
            test.expected_vary_on_key_order);
  EXPECT_EQ(no_vary_search_data.vary_by_default(),
            test.expected_vary_by_default);

  EXPECT_EQ(no_vary_search_data.affected_params(),
            test.expected_affected_params);
}

struct FailureData {
  const char* raw_headers;
  const HttpNoVarySearchData::ParseErrorEnum expected_error;
};

class HttpNoVarySearchResponseHeadersParseFailureTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<FailureData> {};

TEST_P(HttpNoVarySearchResponseHeadersParseFailureTest,
       ParsingFailureOrDefaultValue) {
  const std::string raw_headers =
      HttpUtil::AssembleRawHeaders(GetParam().raw_headers);

  const auto parsed = base::MakeRefCounted<HttpResponseHeaders>(raw_headers);
  const auto no_vary_search_data =
      HttpNoVarySearchData::ParseFromHeaders(*parsed);

  EXPECT_THAT(no_vary_search_data,
              base::test::ErrorIs(GetParam().expected_error))
      << "Headers = " << GetParam().raw_headers;
}

FailureData response_header_failed[] = {
    {// No No-Vary-Search Header case
     "HTTP/1.1 200 OK\r\n"
     "Set-Cookie: a\r\n"
     "Set-Cookie: b\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kOk},

    {// No-Vary-Search Header doesn't parse as a dictionary.
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: "a")"
     "\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kNotDictionary},

    {// No-Vary-Search Header doesn't parse as a dictionary.
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: (a)\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kNotDictionary},

    {// When except is specified, params cannot be a list of strings.
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=("b"),except=("a"))"
     "\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kExceptWithoutTrueParams},

    {// An unknown dictionary key should behave as if the key was not
     // specified.
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: unknown-key\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kDefaultValue},

    {// params not a boolean or a list of strings.
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params="a")"
     "\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kParamsNotStringList},

    {// params not a boolean or a list of strings.
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: params=a\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kParamsNotStringList},

    {// params as an empty list of strings should behave as if the header was
     // not specified.
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: params=()\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kDefaultValue},

    {// params not a boolean or a list of strings.
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=("a" b))"
     "\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kParamsNotStringList},

    {// params defaulting to ?0 which is the same as no header.
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=("a"))"
     "\r\n"
     "No-Vary-Search: params=?0\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kDefaultValue},

    {// except without params.
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: except=()\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kExceptWithoutTrueParams},

    {// except without params.
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: except=()\r\n"
     R"(No-Vary-Search: except=("a"))"
     "\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kExceptWithoutTrueParams},

    {// except without params.
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: except=("a" "b"))"
     "\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kExceptWithoutTrueParams},

    {// except with params set to a list of strings is incorrect.
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=("a"))"
     "\r\n"
     "No-Vary-Search: except=()\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kExceptWithoutTrueParams},

    {// except with params set to a list of strings is incorrect.
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: params=(),except=()\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kExceptWithoutTrueParams},

    {// except with params set to a list of strings is incorrect.
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params,except=(),params=())"
     "\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kExceptWithoutTrueParams},

    {// except with params set to a list of strings is incorrect.
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: except=("a" "b"))"
     "\r\n"
     R"(No-Vary-Search: params=("a"))"
     "\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kExceptWithoutTrueParams},

    {// except with params set to a list of strings is incorrect.
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=("a"),except=("b"))"
     "\r\n"
     "No-Vary-Search: except=()\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kExceptWithoutTrueParams},

    {// except with params set to false is incorrect.
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=?0,except=("a"))"
     "\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kExceptWithoutTrueParams},

    {// except with params set to a list of strings is incorrect.
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params,except=("a" "b"))"
     "\r\n"
     R"(No-Vary-Search: params=("a"))"
     "\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kExceptWithoutTrueParams},

    {// key-order not a boolean
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: key-order="a")"
     "\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kNonBooleanKeyOrder},

    {// key-order not a boolean
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: key-order=a\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kNonBooleanKeyOrder},

    {// key-order not a boolean
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: key-order=()\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kNonBooleanKeyOrder},

    {// key-order not a boolean
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: key-order=(a)\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kNonBooleanKeyOrder},

    {// key-order not a boolean
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: key-order=("a"))"
     "\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kNonBooleanKeyOrder},

    {// key-order not a boolean
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: key-order=(?1)\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kNonBooleanKeyOrder},

    {// key-order set to false should behave as if the
     // header was not specified at all
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: key-order=?0\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kDefaultValue},

    {// params set to false should behave as if the
     // header was not specified at all
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: params=?0\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kDefaultValue},

    {// params set to false should behave as if the
     // header was not specified at all. except set to
     // a list of tokens is incorrect.
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: params=?0\r\n"
     "No-Vary-Search: except=(\"a\")\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kExceptWithoutTrueParams},

    {// except set to a list of tokens is incorrect.
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: params=?1\r\n"
     "No-Vary-Search: except=(a)\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kExceptNotStringList},

    {// except set to true
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: params=?1\r\n"
     "No-Vary-Search: except\r\n\r\n",
     HttpNoVarySearchData::ParseErrorEnum::kExceptNotStringList},
};

const TestData response_headers_tests[] = {
    // params set to a list of strings with one element.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a"))"
        "\r\n\r\n",  // raw_headers
        {"a"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        true,        // expected_vary_by_default
    },
    // params set to a list of strings with one non-ASCII character.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("%C2%A2"))"
        "\r\n\r\n",  // raw_headers
        {"¢"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        true,        // expected_vary_by_default
    },
    // params set to a list of strings with one ASCII and one non-ASCII
    // character.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("c%C2%A2"))"
        "\r\n\r\n",  // raw_headers
        {"c¢"},      // expected_affected_params
        true,        // expected_vary_on_key_order
        true,        // expected_vary_by_default
    },
    // params set to a list of strings with one space and one non-ASCII
    // character.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("+%C2%A2"))"
        "\r\n\r\n",  // raw_headers
        {" ¢"},      // expected_affected_params
        true,        // expected_vary_on_key_order
        true,        // expected_vary_by_default
    },
    // params set to true.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n\r\n",  // raw_headers
        {},                                // expected_affected_params
        true,                              // expected_vary_on_key_order
        false,                             // expected_vary_by_default
    },
    // params set to true.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params=?1\r\n\r\n",  // raw_headers
        {},                                   // expected_affected_params
        true,                                 // expected_vary_on_key_order
        false,                                // expected_vary_by_default
    },
    // params overridden by a list of strings.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a" b))"
        "\r\n"
        R"(No-Vary-Search: params=("c"))"
        "\r\n\r\n",  // raw_headers
        {"c"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        true,        // expected_vary_by_default
    },
    // Vary on all with one excepted search param.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        "No-Vary-Search: except=()\r\n\r\n",  // raw_headers
        {},                                   // expected_affected_params
        true,                                 // expected_vary_on_key_order
        false,                                // expected_vary_by_default
    },
    // Vary on all with one excepted search param.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        R"(No-Vary-Search: except=("a"))"
        "\r\n\r\n",  // raw_headers
        {"a"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Vary on all with one excepted non-ASCII search param.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        R"(No-Vary-Search: except=("%C2%A2"))"
        "\r\n\r\n",  // raw_headers
        {"¢"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Vary on all with one excepted search param that includes non-ASCII
    // character.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        R"(No-Vary-Search: except=("c+%C2%A2"))"
        "\r\n\r\n",  // raw_headers
        {"c ¢"},     // expected_affected_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Vary on all with one excepted search param. Set params as
    // part of the same header line.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params,except=("a"))"
        "\r\n\r\n",  // raw_headers
        {"a"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Vary on all with one excepted search param. Override except
    // on different header line.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params,except=("a" b))"
        "\r\n"
        R"(No-Vary-Search: except=("c"))"
        "\r\n\r\n",  // raw_headers
        {"c"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Vary on all with more than one excepted search param.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        R"(No-Vary-Search: except=("a" "b"))"
        "\r\n\r\n",  // raw_headers
        {"a", "b"},  // expected_affected_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Vary on all with more than one excepted search param. params appears
    // after except in header definition.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: except=("a" "b"))"
        "\r\n"
        "No-Vary-Search: params\r\n\r\n",  // raw_headers
        {"a", "b"},                        // expected_affected_params
        true,                              // expected_vary_on_key_order
        false,                             // expected_vary_by_default
    },
    // Vary on all with more than one excepted search param. Set params as
    // part of the same header line.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params,except=("a" "b"))"
        "\r\n\r\n",  // raw_headers
        {"a", "b"},  // expected_affected_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Don't vary on two search params.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a" "b"))"
        "\r\n\r\n",  // raw_headers
        {"a", "b"},  // expected_affected_params
        true,        // expected_vary_on_key_order
        true,        // expected_vary_by_default
    },
    // Don't vary on search params order.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: key-order\r\n\r\n",  // raw_headers
        {},                                   // expected_affected_params
        false,                                // expected_vary_on_key_order
        true,                                 // expected_vary_by_default
    },
    // Don't vary on search params order.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: key-order=?1\r\n\r\n",  // raw_headers
        {},                                      // expected_affected_params
        false,                                   // expected_vary_on_key_order
        true,                                    // expected_vary_by_default
    },
    // Don't vary on search params order and on two specific search params.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a" "b"))"
        "\r\n"
        "No-Vary-Search: key-order\r\n\r\n",  // raw_headers
        {"a", "b"},                           // expected_affected_params
        false,                                // expected_vary_on_key_order
        true,                                 // expected_vary_by_default
    },
    // Don't vary on search params order and on two specific search params.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a" "b"))"
        "\r\n"
        "No-Vary-Search: key-order=?1\r\n\r\n",  // raw_headers
        {"a", "b"},                              // expected_affected_params
        false,                                   // expected_vary_on_key_order
        true,                                    // expected_vary_by_default
    },
    // Vary on search params order and do not vary on two specific search
    // params.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a" "b"))"
        "\r\n"
        "No-Vary-Search: key-order=?0\r\n\r\n",  // raw_headers
        {"a", "b"},                              // expected_affected_params
        true,                                    // expected_vary_on_key_order
        true,                                    // expected_vary_by_default
    },
    // Vary on all search params except one, and do not vary on search params
    // order.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        R"(No-Vary-Search: except=("a"))"
        "\r\n"
        "No-Vary-Search: key-order\r\n\r\n",  // raw_headers
        {"a"},                                // expected_affected_params
        false,                                // expected_vary_on_key_order
        false,                                // expected_vary_by_default
    },
    // Vary on all search params except one, and do not vary on search params
    // order.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params=?1\r\n"
        R"(No-Vary-Search: except=("a"))"
        "\r\n"
        "No-Vary-Search: key-order\r\n\r\n",  // raw_headers
        {"a"},                                // expected_affected_params
        false,                                // expected_vary_on_key_order
        false,                                // expected_vary_by_default
    },
    // Vary on all search params except one, and do not vary on search params
    // order.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        R"(No-Vary-Search: except=("a"))"
        "\r\n"
        "No-Vary-Search: key-order=?1\r\n\r\n",  // raw_headers
        {"a"},                                   // expected_affected_params
        false,                                   // expected_vary_on_key_order
        false,                                   // expected_vary_by_default
    },
    // Vary on all search params except one, and vary on search params order.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params=?1\r\n"
        R"(No-Vary-Search: except=("a"))"
        "\r\n"
        "No-Vary-Search: key-order=?0\r\n\r\n",  // raw_headers
        {"a"},                                   // expected_affected_params
        true,                                    // expected_vary_on_key_order
        false,                                   // expected_vary_by_default
    },
    // Vary on all search params except two, and do not vary on search params
    // order.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        R"(No-Vary-Search: except=("a" "b"))"
        "\r\n"
        "No-Vary-Search: key-order\r\n\r\n",  // raw_headers
        {"a", "b"},                           // expected_affected_params
        false,                                // expected_vary_on_key_order
        false,                                // expected_vary_by_default
    },
    // Do not vary on one search params. Override params on a different header
    // line.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a"))"
        "\r\n"
        R"(No-Vary-Search: params=("b"))"
        "\r\n\r\n",  // raw_headers
        {"b"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        true,        // expected_vary_by_default
    },
    // Do not vary on any search params. Override params on a different header
    // line.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a"))"
        "\r\n"
        "No-Vary-Search: params\r\n\r\n",  // raw_headers
        {},                                // expected_affected_params
        true,                              // expected_vary_on_key_order
        false,                             // expected_vary_by_default
    },
    // Do not vary on any search params except one. Override except on a
    // different header line.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        R"(No-Vary-Search: except=("a"))"
        "\r\n"
        R"(No-Vary-Search: except=("b"))"
        "\r\n\r\n",  // raw_headers
        {"b"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Allow extension via parameters.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params;unknown\r\n\r\n",  // raw_headers
        {},                                        // expected_affected_params
        true,                                      // expected_vary_on_key_order
        false,                                     // expected_vary_by_default
    },
    // Allow extension via parameters.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a");unknown)"
        "\r\n\r\n",  // raw_headers
        {"a"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        true,        // expected_vary_by_default
    },
    // Allow extension via parameters.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params;unknown,except=("a");unknown)"
        "\r\n\r\n",  // raw_headers
        {"a"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Allow extension via parameters.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: key-order;unknown\r\n\r\n",  // raw_headers
        {},     // expected_affected_params
        false,  // expected_vary_on_key_order
        true,   // expected_vary_by_default
    },
    // Allow extension via parameters.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a";unknown))"
        "\r\n\r\n",  // raw_headers
        {"a"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        true,        // expected_vary_by_default
    },
    // Allow extension via parameters.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        R"(No-Vary-Search: except=("a";unknown))"
        "\r\n\r\n",  // raw_headers
        {"a"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Vary on all search params except one. Override except on a different
    // header line.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params,except=(a)\r\n"
        R"(No-Vary-Search: except=("a"))"
        "\r\n\r\n",  // raw_headers
        {"a"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Continue parsing if an unknown key is in the dictionary.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params,except=(a)\r\n"
        "No-Vary-Search: unknown-key\r\n"
        R"(No-Vary-Search: except=("a"))"
        "\r\n\r\n",  // raw_headers
        {"a"},       // expected_affected_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    }};

INSTANTIATE_TEST_SUITE_P(HttpNoVarySearchResponseHeadersTest,
                         HttpNoVarySearchResponseHeadersTest,
                         ValuesIn(response_headers_tests));

INSTANTIATE_TEST_SUITE_P(HttpNoVarySearchResponseHeadersParseFailureTest,
                         HttpNoVarySearchResponseHeadersParseFailureTest,
                         ValuesIn(response_header_failed));

struct NoVarySearchCompareTestData {
  const GURL request_url;
  const GURL cached_url;
  const std::string_view raw_headers;
  const bool expected_match;
};

HttpNoVarySearchData CreateFromRawHeaders(std::string_view raw_headers) {
  const std::string headers = HttpUtil::AssembleRawHeaders(raw_headers);
  const auto parsed = base::MakeRefCounted<HttpResponseHeaders>(headers);
  return HttpNoVarySearchData::ParseFromHeaders(*parsed).value();
}

TEST(HttpNoVarySearchAreEquivalentTest, CheckUrlEqualityWithSpecialCharacters) {
  // Use special characters in both `keys` and `values`.
  const base::flat_map<std::string, std::string> percent_encoding = {
      {"!", "%21"},    {"#", "%23"},    {"$", "%24"},    {"%", "%25"},
      {"&", "%26"},    {"'", "%27"},    {"(", "%28"},    {")", "%29"},
      {"*", R"(%2A)"}, {"+", R"(%2B)"}, {",", R"(%2C)"}, {"-", R"(%2D)"},
      {".", R"(%2E)"}, {"/", R"(%2F)"}, {":", R"(%3A)"}, {";", "%3B"},
      {"<", R"(%3C)"}, {"=", R"(%3D)"}, {">", R"(%3E)"}, {"?", R"(%3F)"},
      {"@", "%40"},    {"[", "%5B"},    {"]", R"(%5D)"}, {"^", R"(%5E)"},
      {"_", R"(%5F)"}, {"`", "%60"},    {"{", "%7B"},    {"|", R"(%7C)"},
      {"}", R"(%7D)"}, {"~", R"(%7E)"}, {"", ""}};
  const std::string_view raw_headers =
      "HTTP/1.1 200 OK\r\n"
      R"(No-Vary-Search: params=("c"))"
      "\r\n\r\n";

  const auto no_vary_search_data = CreateFromRawHeaders(raw_headers);

  for (const auto& [key, value] : percent_encoding) {
    std::string request_url_template =
        R"(https://a.test/index.html?$key=$value)";
    std::string cached_url_template =
        R"(https://a.test/index.html?c=3&$key=$value)";

    base::ReplaceSubstringsAfterOffset(&request_url_template, 0, "$key", value);
    base::ReplaceSubstringsAfterOffset(&request_url_template, 0, "$value",
                                       value);
    base::ReplaceSubstringsAfterOffset(&cached_url_template, 0, "$key", value);
    base::ReplaceSubstringsAfterOffset(&cached_url_template, 0, "$value",
                                       value);

    EXPECT_TRUE(no_vary_search_data.AreEquivalent(GURL(request_url_template),
                                                  GURL(cached_url_template)));

    std::string header_template =
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params, except=("$key"))"
        "\r\n\r\n";
    base::ReplaceSubstringsAfterOffset(&header_template, 0, "$key", key);

    const auto no_vary_search_data_special_char =
        CreateFromRawHeaders(header_template);

    EXPECT_TRUE(no_vary_search_data_special_char.AreEquivalent(
        GURL(request_url_template), GURL(cached_url_template)));
  }
}

constexpr std::pair<std::string_view, std::string_view>
    kPercentEncodedNonAsciiKeys[] = {
        {"¢", R"(%C2%A2)"},
        {"¢ ¢", R"(%C2%A2+%C2%A2)"},
        {"é 気", R"(%C3%A9+%E6%B0%97)"},
        {"é", R"(%C3%A9)"},
        {"気", R"(%E6%B0%97)"},
        {"ぁ", R"(%E3%81%81)"},
        {"𐨀", R"(%F0%90%A8%80)"},
};

enum class AreEquivalentImplementation {
  kOld,
  kNew,
};

// Configures the ImplementationOverrideForTesting object to simulate
// enabling/disabling feature "HttpNoVarySearchDataUseNewAreEquivalent".
std::unique_ptr<
    ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting>
ConfigureAreEquivalentImplementation(
    AreEquivalentImplementation implementation) {
  switch (implementation) {
    case AreEquivalentImplementation::kOld:
      return std::make_unique<
          ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting>(
          false);

    case AreEquivalentImplementation::kNew:
      return std::make_unique<
          ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting>(
          true);
  }
}

class HttpNoVarySearchAreEquivalentTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<AreEquivalentImplementation> {
 public:
  HttpNoVarySearchAreEquivalentTest() {
    are_equivalent_implementation_override_ =
        ConfigureAreEquivalentImplementation(GetParam());
  }

 private:
  std::unique_ptr<
      ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting>
      are_equivalent_implementation_override_;
};

INSTANTIATE_TEST_SUITE_P(HttpNoVarySearchAreEquivalentTest,
                         HttpNoVarySearchAreEquivalentTest,
                         Values(AreEquivalentImplementation::kOld,
                                AreEquivalentImplementation::kNew));

TEST_P(HttpNoVarySearchAreEquivalentTest,
       CheckUrlEqualityWithPercentEncodedNonASCIICharactersExcept) {
  for (const auto& [key, value] : kPercentEncodedNonAsciiKeys) {
    std::string request_url_template = R"(https://a.test/index.html?$key=c)";
    std::string cached_url_template = R"(https://a.test/index.html?c=3&$key=c)";
    base::ReplaceSubstringsAfterOffset(&request_url_template, 0, "$key", key);
    base::ReplaceSubstringsAfterOffset(&cached_url_template, 0, "$key", key);
    std::string header_template =
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params, except=("$key"))"
        "\r\n\r\n";
    base::ReplaceSubstringsAfterOffset(&header_template, 0, "$key", value);

    const auto no_vary_search_data_special_char =
        CreateFromRawHeaders(header_template);

    EXPECT_TRUE(no_vary_search_data_special_char.AreEquivalent(
        GURL(request_url_template), GURL(cached_url_template)))
        << "request_url = " << request_url_template
        << " cached_url = " << cached_url_template
        << " headers = " << header_template;
  }
}

TEST_P(HttpNoVarySearchAreEquivalentTest,
       CheckUrlEqualityWithPercentEncodedNonASCIICharacters) {
  for (const auto& [key, value] : kPercentEncodedNonAsciiKeys) {
    std::string request_url_template =
        R"(https://a.test/index.html?a=2&$key=c)";
    std::string cached_url_template = R"(https://a.test/index.html?$key=d&a=2)";
    base::ReplaceSubstringsAfterOffset(&request_url_template, 0, "$key", key);
    base::ReplaceSubstringsAfterOffset(&cached_url_template, 0, "$key", key);
    std::string header_template =
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("$key"))"
        "\r\n\r\n";
    base::ReplaceSubstringsAfterOffset(&header_template, 0, "$key", value);

    const auto no_vary_search_data_special_char =
        CreateFromRawHeaders(header_template);

    EXPECT_TRUE(no_vary_search_data_special_char.AreEquivalent(
        GURL(request_url_template), GURL(cached_url_template)))
        << "request_url = " << request_url_template
        << " cached_url = " << cached_url_template
        << " headers = " << header_template;
  }
}

class HttpNoVarySearchAreEquivalentParameterizedTest
    : public ::testing::TestWithParam<std::tuple<NoVarySearchCompareTestData,
                                                 AreEquivalentImplementation>> {
 protected:
  HttpNoVarySearchAreEquivalentParameterizedTest() {
    are_equivalent_implementation_override_ =
        ConfigureAreEquivalentImplementation(std::get<1>(GetParam()));
  }

  const NoVarySearchCompareTestData& GetTestData() const {
    return std::get<0>(GetParam());
  }

 private:
  std::unique_ptr<
      ScopedHttpNoVarySearchDataEquivalentImplementationOverrideForTesting>
      are_equivalent_implementation_override_;
};

TEST_P(HttpNoVarySearchAreEquivalentParameterizedTest,
       CheckUrlEqualityByNoVarySearch) {
  const auto& test_data = GetTestData();

  const auto no_vary_search_data = CreateFromRawHeaders(test_data.raw_headers);

  EXPECT_EQ(no_vary_search_data.AreEquivalent(test_data.request_url,
                                              test_data.cached_url),
            test_data.expected_match)
      << "request_url = " << test_data.request_url
      << " cached_url = " << test_data.cached_url
      << " headers = " << test_data.raw_headers
      << " match = " << test_data.expected_match;
}

const NoVarySearchCompareTestData no_vary_search_compare_tests[] = {
    // Url's for same page with same username but different passwords.
    {GURL("https://owner:correct@a.test/index.html?a=2&b=3"),
     GURL("https://owner:incorrect@a.test/index.html?a=2&b=3"),
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: params\r\n\r\n",
     false},
    // Url's for same page with different username.
    {GURL("https://anonymous@a.test/index.html?a=2&b=3"),
     GURL("https://owner@a.test/index.html?a=2&b=3"),
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: params\r\n\r\n",
     false},
    // Url's for same origin with different path.
    {GURL("https://a.test/index.html?a=2&b=3"),
     GURL("https://a.test/home.html?a=2&b=3"),
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: params\r\n\r\n",
     false},
    // Url's for same page with different protocol.
    {GURL("http://a.test/index.html?a=2&b=3"),
     GURL("https://a.test/index.html?a=2&b=3"),
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: params\r\n\r\n",
     false},
    // Url's for different pages without the query and reference part
    // are not equivalent.
    {GURL("https://a.test/index.html?a=2&b=3"),
     GURL("https://b.test/index.html?b=4&c=5"),
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: params\r\n\r\n",
     false},
    // Cached page requested again with different order of query parameters with
    // the same values.
    {GURL("https://a.test/index.html?a=2&b=3"),
     GURL("https://a.test/index.html?b=3&a=2"),
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: key-order\r\n\r\n",
     true},
    // Cached page requested again with different order of query parameters but
    // with different values.
    {GURL("https://a.test/index.html?a=2&c=5&b=3"),
     GURL("https://a.test/index.html?c=4&b=3&a=2"),
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: key-order\r\n\r\n",
     false},
    // Cached page requested again with values in different order for the query
    // parameters with the same name. Key order is ignored.
    {GURL("https://a.test/index.html?d=6&a=4&b=5&b=3&c=5&a=3"),
     GURL("https://a.test/index.html?b=5&a=3&a=4&d=6&c=5&b=3"),
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: key-order"
     "\r\n\r\n",
     false},
    // Cached page requested again with values in the same order for the query
    // parameters with the same name. Key order is ignored.
    {GURL("https://a.test/index.html?d=6&a=3&b=5&b=3&c=5&a=4"),
     GURL("https://a.test/index.html?b=5&a=3&a=4&d=6&c=5&b=3"),
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: key-order"
     "\r\n\r\n",
     true},
    // Cached page requested again with different order of query parameters but
    // with one of the query parameters marked to be ignored.
    {GURL("https://a.test/index.html?a=2&c=3&b=2"),
     GURL("https://a.test/index.html?a=2&b=2&c=5"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=("c"))"
     "\r\n\r\n",
     true},
    // Cached page requested again without any query parameters, but
    // the cached URL's query parameter marked to be ignored.
    {GURL("https://a.test/index.html"), GURL("https://a.test/index.html?a=2"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=("a"))"
     "\r\n\r\n",
     true},
    // Cached page requested again with different values for the query
    // parameters that are marked to be ignored. Same value for the query
    // parameter that is marked as to vary.
    {GURL("https://a.test/index.html?a=1&b=2&c=3"),
     GURL("https://a.test/index.html?b=5&a=3&d=6&c=3"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params, except=("c"))"
     "\r\n\r\n",
     true},
    // Cached page requested again with different values for the query
    // parameters that are marked to be ignored. Different value for the query
    // parameter that is marked as to vary.
    {GURL("https://a.test/index.html?a=1&b=2&c=5"),
     GURL("https://a.test/index.html?b=5&a=3&d=6&c=3"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params, except=("c"))"
     "\r\n\r\n",
     false},
    // Cached page requested again with different values for the query
    // parameters that are marked to be ignored. Same values for the query
    // parameters that are marked as to vary.
    {GURL("https://a.test/index.html?d=6&a=1&b=2&c=5"),
     GURL("https://a.test/index.html?b=5&a=3&d=6&c=5"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params, except=("c" "d"))"
     "\r\n\r\n",
     true},
    // Cached page requested again with different values for the query
    // parameters that are marked to be ignored. Same values for the query
    // parameters that are marked as to vary. Some query parameters to be
    // ignored appear multiple times in the query.
    {GURL("https://a.test/index.html?d=6&a=1&a=2&b=2&b=3&c=5"),
     GURL("https://a.test/index.html?b=5&a=3&a=4&d=6&c=5"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params, except=("c" "d"))"
     "\r\n\r\n",
     true},
    // Cached page requested again with query parameters. All query parameters
    // are marked as to be ignored.
    {GURL("https://a.test/index.html?a=1&b=2&c=5"),
     GURL("https://a.test/index.html"),
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: params\r\n\r\n",
     true},
    // Cached page requested again with query parameters. All query parameters
    // are marked as to be ignored. Both request url and cached url have query
    // parameters.
    {GURL("https://a.test/index.html?a=1&b=2&c=5"),
     GURL("https://a.test/index.html?a=5&b=6&c=8&d=1"),
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: params\r\n\r\n",
     true},
    // Add test for when the keys are percent encoded.
    {GURL(R"(https://a.test/index.html?c+1=3&b+%202=2&a=1&%63%201=2&a=5)"),
     GURL(R"(https://a.test/index.html?a=1&b%20%202=2&%63%201=3&a=5&c+1=2)"),
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: key-order\r\n\r\n",
     true},
    // Add test for when there are different representations of a character
    {GURL(R"(https://a.test/index.html?%C3%A9=f&a=2&c=4&é=b)"),
     GURL(R"(https://a.test/index.html?a=2&é=f&c=4&d=7&é=b)"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=("d"), key-order)"
     "\r\n\r\n",
     true},
    // Add test for when there are triple code point
    {GURL(R"(https://a.test/index.html?%E3%81%81=f&a=2&c=4&%E3%81%81=b)"),
     GURL(R"(https://a.test/index.html?a=2&%E3%81%81=f&c=4&d=7&%E3%81%81=b)"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=("d"), key-order)"
     "\r\n\r\n",
     true},
    // Add test for when there are quadruple code point
    {GURL(
         R"(https://a.test/index.html?%F0%90%A8%80=%F0%90%A8%80&a=2&c=4&%F0%90%A8%80=b)"),
     GURL(
         R"(https://a.test/index.html?a=2&%F0%90%A8%80=%F0%90%A8%80&c=4&d=7&%F0%90%A8%80=b)"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=("d"), key-order)"
     "\r\n\r\n",
     true},
    // Add test for when there are params with empty values / keys.
    {GURL("https://a.test/index.html?a&b&c&a=2&d&=5&=1&=3"),
     GURL("https://a.test/index.html?c&d&b&a&=5&=1&a=2&=3"),
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: key-order\r\n\r\n",
     true},
    // Add test for when there are params with empty values / keys, an empty
    // key pair missing.
    {GURL("https://a.test/index.html?a&b&c&a=2&d&=5&=1&=3"),
     GURL("https://a.test/index.html?c&d&b&a&=5&a=2&=3"),
     "HTTP/1.1 200 OK\r\n"
     "No-Vary-Search: key-order\r\n\r\n",
     false},
    // Add test when there are params with keys / values that are wrongly
    // escaped.
    {GURL(R"(https://a.test/index.html?a=%3&%3=b)"),
     GURL(R"(https://a.test/index.html?a=%3&c=3&%3=b)"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=("c"))"
     "\r\n\r\n",
     true},
    // Add test when there is a param with key starting with a percent encoded
    // space (+).
    {GURL(R"(https://a.test/index.html?+a=3)"),
     GURL(R"(https://a.test/index.html?+a=2)"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=("+a"))"
     "\r\n\r\n",
     true},
    // Add test when there is a param with key starting with a percent encoded
    // space (+) and gets compared with same key without the leading space.
    {GURL(R"(https://a.test/index.html?+a=3)"),
     GURL(R"(https://a.test/index.html?a=2)"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=("+a"))"
     "\r\n\r\n",
     false},
    // Add test for when there are different representations of the character é
    // and we are ignoring that key.
    {GURL(R"(https://a.test/index.html?%C3%A9=g&a=2&c=4&é=b)"),
     GURL(R"(https://a.test/index.html?a=2&é=f&c=4&d=7&é=b)"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params=("d" "%C3%A9"))"
     "\r\n\r\n",
     true},
    // Add test for when there are different representations of the character é
    // and we are not ignoring that key.
    {GURL(R"(https://a.test/index.html?%C3%A9=f&a=2&c=4&é=b)"),
     GURL(R"(https://a.test/index.html?a=2&é=f&c=4&d=7&é=b)"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params, except=("%C3%A9"))"
     "\r\n\r\n",
     true},
    // Add test for when there are different representations of the character é
    // and we are not ignoring that key.
    {GURL(R"(https://a.test/index.html?%C3%A9=g&a=2&c=4&é=b)"),
     GURL(R"(https://a.test/index.html?a=2&é=f&c=4&d=7&é=b)"),
     "HTTP/1.1 200 OK\r\n"
     R"(No-Vary-Search: params, except=("%C3%A9"))"
     "\r\n\r\n",
     false},
};

INSTANTIATE_TEST_SUITE_P(HttpNoVarySearchAreEquivalentParameterizedTest,
                         HttpNoVarySearchAreEquivalentParameterizedTest,
                         Combine(ValuesIn(no_vary_search_compare_tests),
                                 Values(AreEquivalentImplementation::kOld,
                                        AreEquivalentImplementation::kNew)));

// AreEquivalent() needs to operate on a URL that has a scheme that has a query
// and fragment. Rather than forcing the fuzzer to work that it needs to start
// the string with an http(s) scheme by itself, this function always creates an
// https URL.
GURL CreateUrlFromSuffix(const std::string& suffix) {
  return GURL(base::StrCat({"https://", suffix}));
}

// Verifies that the old and new implementations of AreEquivalent() give the
// same output for the same input. `url_suffix_a` and `url_suffix_b` are the
// URLs to test without the initial "https://". `params`, `vary_on_params` and
// `vary_on_key_order` are used to configure the HttpNoVarySearchData object.
void AreEquivalentImplementationsMatch(const std::string& url_suffix_a,
                                       const std::string& url_suffix_b,
                                       const std::vector<std::string>& params,
                                       bool vary_on_params,
                                       bool vary_on_key_order) {
  // Discard invalid configurations early so we don't waste time on them.
  if (vary_on_params && params.empty() && vary_on_key_order) {
    // This configuration is equivalent to the default configuration, so is
    // invalid.
    return;
  }
  const GURL url_a = CreateUrlFromSuffix(url_suffix_a);
  if (!url_a.is_valid()) {
    return;
  }
  const GURL url_b = CreateUrlFromSuffix(url_suffix_b);
  if (!url_b.is_valid()) {
    return;
  }
  const HttpNoVarySearchData data =
      vary_on_params ? HttpNoVarySearchData::CreateFromNoVaryParams(
                           params, vary_on_key_order)
                     : HttpNoVarySearchData::CreateFromVaryParams(
                           params, vary_on_key_order);
  EXPECT_EQ(data.AreEquivalentOldImplForTesting(url_a, url_b),
            data.AreEquivalentNewImplForTesting(url_a, url_b));
}

FUZZ_TEST(HttpNoVarySearchTest, AreEquivalentImplementationsMatch);

TEST(HttpNoVarySearchTest, CanonicalizeQuery) {
  HttpNoVarySearchData data =
      HttpNoVarySearchData::CreateFromNoVaryParams({"rd"}, false);
  static constexpr char kInputQuery[] =
      "q=1&rd=e2f2a976&a&a=+&%61=%62&%c0=%c1&%61=1&a=2&a=5&b=%6&a=%c2%a2&%c2%"
      "a2";
  // Because `vary_on_key_order` is false, the canonicalized output is sorted by
  // key. The original order of values must be preserved.
  static constexpr char kExpectedOutput[] =
      "a=&a= "
      "&a=b&a=1&a=2&a=5&a=\xC2\xA2&b=%256&q=1&\xC2\xA2=&\xEF\xBF\xBD="
      "\xEF\xBF\xBD";
  GURL url(base::StrCat({"https://example.com/?", kInputQuery}));
  EXPECT_EQ(data.CanonicalizeQuery(url), kExpectedOutput);
}

class HttpNoVarySearchCanonicalizeQueryTest
    : public testing::TestWithParam<NoVarySearchCompareTestData> {
 protected:
  const NoVarySearchCompareTestData& GetTestData() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(HttpNoVarySearchCanonicalizeQueryTest,
                         HttpNoVarySearchCanonicalizeQueryTest,
                         ValuesIn(no_vary_search_compare_tests));

GURL ExtractBaseUrl(const GURL& url) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearQuery();
  return url.ReplaceComponents(replacements);
}

TEST_P(HttpNoVarySearchCanonicalizeQueryTest, ResultsSameAsAreEquivalent) {
  const auto& [request_url, cached_url, raw_headers, expected_match] =
      GetTestData();
  if (ExtractBaseUrl(request_url) != ExtractBaseUrl(cached_url)) {
    GTEST_SKIP() << "Differing base URLs are not interesting for this test";
  }

  const auto no_vary_search_data = CreateFromRawHeaders(raw_headers);
  if (expected_match) {
    EXPECT_EQ(no_vary_search_data.CanonicalizeQuery(request_url),
              no_vary_search_data.CanonicalizeQuery(cached_url))
        << "request_url = " << request_url << " cached_url = " << cached_url
        << " headers = " << raw_headers << " match = " << expected_match;
  } else {
    EXPECT_NE(no_vary_search_data.CanonicalizeQuery(request_url),
              no_vary_search_data.CanonicalizeQuery(cached_url))
        << "request_url = " << request_url << " cached_url = " << cached_url
        << " headers = " << raw_headers << " match = " << expected_match;
  }
}

TEST(HttpNoVarySearchResponseHeadersParseHistogramTest, NoUnrecognizedKeys) {
  base::HistogramTester histogram_tester;
  const std::string raw_headers = HttpUtil::AssembleRawHeaders(
      "HTTP/1.1 200 OK\r\nNo-Vary-Search: params\r\n\r\n");
  const auto parsed = base::MakeRefCounted<HttpResponseHeaders>(raw_headers);
  const auto no_vary_search_data =
      HttpNoVarySearchData::ParseFromHeaders(*parsed);
  EXPECT_THAT(no_vary_search_data, base::test::HasValue());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpNoVarySearch.HasUnrecognizedKeys", false, 1);
}

TEST(HttpNoVarySearchResponseHeadersParseHistogramTest, UnrecognizedKeys) {
  base::HistogramTester histogram_tester;
  const std::string raw_headers = HttpUtil::AssembleRawHeaders(
      "HTTP/1.1 200 OK\r\nNo-Vary-Search: params, rainbows\r\n\r\n");
  const auto parsed = base::MakeRefCounted<HttpResponseHeaders>(raw_headers);
  const auto no_vary_search_data =
      HttpNoVarySearchData::ParseFromHeaders(*parsed);
  EXPECT_THAT(no_vary_search_data, base::test::HasValue());
  histogram_tester.ExpectUniqueSample(
      "Net.HttpNoVarySearch.HasUnrecognizedKeys", true, 1);
}

TEST(HttpNoVarySearchDataTest, ComparisonOperators) {
  constexpr auto kValues = std::to_array<std::string_view>(
      {"params", "key-order", "params, key-order", R"(params=("a"))",
       R"(params=("b"))", R"(params, except=("a"))", R"(params, except=("b"))",
       R"(params, except=("a"), key-order)"});
  auto data_vector = base::ToVector(kValues, [](std::string_view value) {
    auto headers = HttpResponseHeaders::Builder({1, 1}, "200 OK")
                       .AddHeader("No-Vary-Search", value)
                       .Build();
    auto result = HttpNoVarySearchData::ParseFromHeaders(*headers);
    CHECK(result.has_value());
    return result.value();
  });
  // We don't actually care what the order is, just that it is consistent, so
  // sort the vector.
  std::ranges::sort(data_vector);

  // Compare everything to itself.
  for (const auto& data : data_vector) {
    EXPECT_EQ(data, data);
    EXPECT_EQ(data <=> data, std::strong_ordering::equal);
  }
  // Compare everything to everything else.
  for (size_t i = 0; i < data_vector.size() - 1; ++i) {
    for (size_t j = i + 1; j < data_vector.size(); ++j) {
      // Commutativity of !=.
      EXPECT_NE(data_vector[i], data_vector[j]);
      EXPECT_NE(data_vector[j], data_vector[i]);

      // Transitivity of <.
      EXPECT_LT(data_vector[i], data_vector[j]);
      EXPECT_GT(data_vector[j], data_vector[i]);
    }
  }
}

// Use the `no_vary_search_compare_tests` as a convenient data set for testing
// serialization and deserialization.
class HttpNoVarySearchSerializationParameterizedTest
    : public ::testing::TestWithParam<NoVarySearchCompareTestData> {};

TEST_P(HttpNoVarySearchSerializationParameterizedTest, RoundTrip) {
  const auto test_data = GetParam();

  const std::string headers =
      HttpUtil::AssembleRawHeaders(test_data.raw_headers);
  const auto parsed = base::MakeRefCounted<HttpResponseHeaders>(headers);
  ASSERT_OK_AND_ASSIGN(const auto no_vary_search_data,
                       HttpNoVarySearchData::ParseFromHeaders(*parsed));

  base::Pickle pickle;
  WriteToPickle(pickle, no_vary_search_data);

  // This requires that the whole Pickle is consumed.
  std::optional<HttpNoVarySearchData> extracted =
      ReadValueFromPickle<HttpNoVarySearchData>(pickle);

  EXPECT_THAT(extracted, Optional(no_vary_search_data));
}

INSTANTIATE_TEST_SUITE_P(HttpNoVarySearchSerializationParameterizedTest,
                         HttpNoVarySearchSerializationParameterizedTest,
                         ValuesIn(no_vary_search_compare_tests));

base::Pickle MakeBadPickle(uint32_t magic_number,
                           const base::flat_set<std::string>& affected_params,
                           bool vary_on_key_order,
                           bool vary_by_default) {
  base::Pickle result;
  WriteToPickle(result, magic_number, affected_params, vary_on_key_order,
                vary_by_default);
  return result;
}

struct BadPickleParams {
  std::string_view why_bad;  // Should be alphanumeric.
  uint32_t magic_number;
  base::flat_set<std::string> affected_params;
  bool vary_on_key_order;
  bool vary_by_default;
};

class HttpNoVarySearchBadPickleTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<BadPickleParams> {};

TEST_P(HttpNoVarySearchBadPickleTest, VerifyFails) {
  const auto [_, magic_number, affected_params, vary_on_key_order,
              vary_by_default] = GetParam();
  base::Pickle pickle = MakeBadPickle(magic_number, affected_params,
                                      vary_on_key_order, vary_by_default);
  std::optional<HttpNoVarySearchData> result =
      ReadValueFromPickle<HttpNoVarySearchData>(pickle);
  EXPECT_EQ(result, std::nullopt);
}

// This value and the bad pickle tests need to be updated if the corresponding
// value in the declaration of HttpNoVarySearchData is updated.
constexpr uint32_t kMagicNumber = 0xfe1056f3;

const auto bad_pickle_params = std::to_array<BadPickleParams>({
    {"BadMagicNumber", 0xfeeddad0, {}, false, false},
    {"DefaultBehavior", kMagicNumber, {}, true, true},
});

INSTANTIATE_TEST_SUITE_P(
    HttpNoVarySearchBadPickleTest,
    HttpNoVarySearchBadPickleTest,
    ValuesIn(bad_pickle_params),
    [](const testing::TestParamInfo<BadPickleParams>& info) {
      return std::string(info.param.why_bad);
    });

TEST(HttpNoVarySearchEmptyPickleTest, ReadEmptyPickle) {
  base::Pickle pickle;
  EXPECT_EQ(ReadValueFromPickle<HttpNoVarySearchData>(pickle), std::nullopt);
}

TEST(HttpNoVarySearchDataTest, AbslHashValue) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      // Two identical objects.
      HttpNoVarySearchData::CreateFromNoVaryParams({"a", "b"}, true),
      HttpNoVarySearchData::CreateFromNoVaryParams({"a", "b"}, true),
      // Order of params shouldn't matter as they are stored in a flat_set.
      HttpNoVarySearchData::CreateFromNoVaryParams({"b", "a"}, true),
      // Different objects.
      HttpNoVarySearchData::CreateFromNoVaryParams({"a"}, true),
      HttpNoVarySearchData::CreateFromNoVaryParams({"a", "b"}, false),
      HttpNoVarySearchData::CreateFromVaryParams({"a", "b"}, true),
      HttpNoVarySearchData::CreateFromVaryParams({"a", "b"}, false),
      HttpNoVarySearchData::CreateFromVaryParams({"c"}, true),
      // Object with only vary_on_key_order set to false.
      HttpNoVarySearchData::CreateFromNoVaryParams({}, false),
      // Object that only varies on one param.
      HttpNoVarySearchData::CreateFromVaryParams({"a"}, true),
  }));
}

}  // namespace

}  // namespace net
