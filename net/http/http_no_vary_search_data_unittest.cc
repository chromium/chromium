// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_no_vary_search_data.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

using testing::IsEmpty;
using testing::UnorderedElementsAreArray;

TEST(HttpNoVarySearchCreateTest, CreateFromNoVaryParamsNonEmptyVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromNoVaryParams({"a"}, true);
  EXPECT_THAT(no_vary_search.no_vary_params(),
              UnorderedElementsAreArray({"a"}));
  EXPECT_THAT(no_vary_search.vary_params(), IsEmpty());
  EXPECT_TRUE(no_vary_search.vary_on_key_order());
  EXPECT_TRUE(no_vary_search.vary_by_default());
}

TEST(HttpNoVarySearchCreateTest,
     CreateFromNoVaryParamsNonEmptyNoVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromNoVaryParams({"a"}, false);
  EXPECT_THAT(no_vary_search.no_vary_params(),
              UnorderedElementsAreArray({"a"}));
  EXPECT_THAT(no_vary_search.vary_params(), IsEmpty());
  EXPECT_FALSE(no_vary_search.vary_on_key_order());
  EXPECT_TRUE(no_vary_search.vary_by_default());
}

TEST(HttpNoVarySearchCreateTest, CreateFromNoVaryParamsEmptyNoVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromNoVaryParams({}, false);
  EXPECT_THAT(no_vary_search.no_vary_params(), IsEmpty());
  EXPECT_THAT(no_vary_search.vary_params(), IsEmpty());
  EXPECT_FALSE(no_vary_search.vary_on_key_order());
  EXPECT_TRUE(no_vary_search.vary_by_default());
}

TEST(HttpNoVarySearchCreateTest, CreateFromNoVaryParamsEmptyVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromNoVaryParams({}, true);
  EXPECT_THAT(no_vary_search.no_vary_params(), IsEmpty());
  EXPECT_THAT(no_vary_search.vary_params(), IsEmpty());
  EXPECT_TRUE(no_vary_search.vary_on_key_order());
  EXPECT_TRUE(no_vary_search.vary_by_default());
}

TEST(HttpNoVarySearchCreateTest, CreateFromVaryParamsNonEmptyVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromVaryParams({"a"}, true);
  EXPECT_THAT(no_vary_search.no_vary_params(), IsEmpty());
  EXPECT_THAT(no_vary_search.vary_params(), UnorderedElementsAreArray({"a"}));
  EXPECT_TRUE(no_vary_search.vary_on_key_order());
  EXPECT_FALSE(no_vary_search.vary_by_default());
}

TEST(HttpNoVarySearchCreateTest, CreateFromVaryParamsNonEmptyNoVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromVaryParams({"a"}, false);
  EXPECT_THAT(no_vary_search.no_vary_params(), IsEmpty());
  EXPECT_THAT(no_vary_search.vary_params(), UnorderedElementsAreArray({"a"}));
  EXPECT_FALSE(no_vary_search.vary_on_key_order());
  EXPECT_FALSE(no_vary_search.vary_by_default());
}

TEST(HttpNoVarySearchCreateTest, CreateFromVaryParamsEmptyNoVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromVaryParams({}, false);
  EXPECT_THAT(no_vary_search.no_vary_params(), IsEmpty());
  EXPECT_THAT(no_vary_search.vary_params(), IsEmpty());
  EXPECT_FALSE(no_vary_search.vary_on_key_order());
  EXPECT_FALSE(no_vary_search.vary_by_default());
}

TEST(HttpNoVarySearchCreateTest, CreateFromVaryParamsEmptyVaryOnKeyOrder) {
  const auto no_vary_search =
      HttpNoVarySearchData::CreateFromVaryParams({}, true);
  EXPECT_THAT(no_vary_search.no_vary_params(), IsEmpty());
  EXPECT_THAT(no_vary_search.vary_params(), IsEmpty());
  EXPECT_TRUE(no_vary_search.vary_on_key_order());
  EXPECT_FALSE(no_vary_search.vary_by_default());
}

struct TestData {
  const char* raw_headers;
  const base::flat_set<std::string> expected_no_vary_params;
  const base::flat_set<std::string> expected_vary_params;
  const bool expected_vary_on_key_order;
  const bool expected_vary_by_default;
};

class HttpNoVarySearchResponseHeadersParseFailureTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<base::StringPiece> {};

class HttpNoVarySearchResponseHeadersTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<TestData> {};

TEST_P(HttpNoVarySearchResponseHeadersTest, ParsingSuccess) {
  const TestData test = GetParam();

  const std::string raw_headers =
      net::HttpUtil::AssembleRawHeaders(test.raw_headers);

  const auto parsed = base::MakeRefCounted<HttpResponseHeaders>(raw_headers);

  const absl::optional<HttpNoVarySearchData> no_vary_search_data =
      HttpNoVarySearchData::ParseFromHeaders(*parsed);

  EXPECT_EQ(no_vary_search_data->vary_on_key_order(),
            test.expected_vary_on_key_order);
  EXPECT_EQ(no_vary_search_data->vary_by_default(),
            test.expected_vary_by_default);

  EXPECT_EQ(no_vary_search_data->no_vary_params(),
            test.expected_no_vary_params);
  EXPECT_EQ(no_vary_search_data->vary_params(), test.expected_vary_params);
}

TEST_P(HttpNoVarySearchResponseHeadersParseFailureTest,
       ParsingFailureOrDefaultValue) {
  const std::string raw_headers = net::HttpUtil::AssembleRawHeaders(GetParam());

  const auto parsed = base::MakeRefCounted<HttpResponseHeaders>(raw_headers);

  const absl::optional<HttpNoVarySearchData> no_vary_search_data =
      HttpNoVarySearchData::ParseFromHeaders(*parsed);

  EXPECT_FALSE(no_vary_search_data.has_value());
}

constexpr base::StringPiece response_header_failed[] = {
    // No No-Vary-Search Header case
    "HTTP/1.1 200 OK\r\n"
    "Set-Cookie: a\r\n"
    "Set-Cookie: b\r\n\r\n",

    // No-Vary-Search Header doesn't parse as a dictionary.
    "HTTP/1.1 200 OK\r\n"
    R"(No-Vary-Search: "a")"
    "\r\n\r\n",

    // No-Vary-Search Header doesn't parse as a dictionary.
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: (a)\r\n\r\n",

    // When except is specified, params cannot be a list of strings.
    "HTTP/1.1 200 OK\r\n"
    R"(No-Vary-Search: params=("b"),except=("a"))"
    "\r\n\r\n",

    // An unknown dictionary key should behave as if the header was not
    // specified.
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: unknown-key\r\n\r\n",

    // params not a boolean or a list of strings.
    "HTTP/1.1 200 OK\r\n"
    R"(No-Vary-Search: params="a")"
    "\r\n\r\n",

    // params not a boolean or a list of strings.
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: params=a\r\n\r\n",

    // params as an empty list of strings should behave as if the header was
    // not specified.
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: params=()\r\n\r\n",

    // params not a boolean or a list of strings.
    "HTTP/1.1 200 OK\r\n"
    R"(No-Vary-Search: params=("a" b))"
    "\r\n\r\n",

    // params defaulting to ?0 which is the same as no header.
    "HTTP/1.1 200 OK\r\n"
    R"(No-Vary-Search: params=("a"))"
    "\r\n"
    "No-Vary-Search: params=?0\r\n\r\n",

    // except without params.
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: except=()\r\n\r\n",

    // except without params.
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: except=()\r\n"
    R"(No-Vary-Search: except=("a"))"
    "\r\n\r\n",

    // except without params.
    "HTTP/1.1 200 OK\r\n"
    R"(No-Vary-Search: except=("a" "b"))"
    "\r\n\r\n",

    // except with params set to a list of strings is incorrect.
    "HTTP/1.1 200 OK\r\n"
    R"(No-Vary-Search: params=("a"))"
    "\r\n"
    "No-Vary-Search: except=()\r\n\r\n",

    // except with params set to a list of strings is incorrect.
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: params=(),except=()\r\n\r\n",

    // except with params set to a list of strings is incorrect.
    "HTTP/1.1 200 OK\r\n"
    R"(No-Vary-Search: params,except=(),params=())"
    "\r\n\r\n",

    // except with params set to a list of strings is incorrect.
    "HTTP/1.1 200 OK\r\n"
    R"(No-Vary-Search: except=("a" "b"))"
    "\r\n"
    R"(No-Vary-Search: params=("a"))"
    "\r\n\r\n",

    // except with params set to a list of strings is incorrect.
    "HTTP/1.1 200 OK\r\n"
    R"(No-Vary-Search: params=("a"),except=("b"))"
    "\r\n"
    "No-Vary-Search: except=()\r\n\r\n",

    // except with params set to false is incorrect.
    "HTTP/1.1 200 OK\r\n"
    R"(No-Vary-Search: params=?0,except=("a"))"
    "\r\n\r\n",

    // except with params set to a list of strings is incorrect.
    "HTTP/1.1 200 OK\r\n"
    R"(No-Vary-Search: params,except=("a" "b"))"
    "\r\n"
    R"(No-Vary-Search: params=("a"))"
    "\r\n\r\n",

    // key-order not a boolean
    "HTTP/1.1 200 OK\r\n"
    R"(No-Vary-Search: key-order="a")"
    "\r\n\r\n",

    // key-order not a boolean
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: key-order=a\r\n\r\n",

    // key-order not a boolean
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: key-order=()\r\n\r\n",

    // key-order not a boolean
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: key-order=(a)\r\n\r\n",

    // key-order not a boolean
    "HTTP/1.1 200 OK\r\n"
    R"(No-Vary-Search: key-order=("a"))"
    "\r\n\r\n",

    // key-order not a boolean
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: key-order=(?1)\r\n\r\n",

    // key-order set to false should behave as if the
    // header was not specified at all
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: key-order=?0\r\n\r\n",

    // params set to false should behave as if the
    // header was not specified at all
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: params=?0\r\n\r\n",

    // params set to false should behave as if the
    // header was not specified at all. except set to
    // a list of tokens is incorrect.
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: params=?0\r\n"
    "No-Vary-Search: except=(a)\r\n\r\n",

    // except set to a list of tokens is incorrect.
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: params=?1\r\n"
    "No-Vary-Search: except=(a)\r\n\r\n",

    // Fail parsing if an unknown key is in the dictionary.
    "HTTP/1.1 200 OK\r\n"
    "No-Vary-Search: params,except=(a)\r\n"
    "No-Vary-Search: unknown-key\r\n"
    R"(No-Vary-Search: except=("a"))"
    "\r\n\r\n",
};

const TestData response_headers_tests[] = {
    // params set to a list of strings with one element.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a"))"
        "\r\n\r\n",  // raw_headers
        {"a"},       // expected_no_vary_params
        {},          // expected_vary_params
        true,        // expected_vary_on_key_order
        true,        // expected_vary_by_default
    },
    // params set to true.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n\r\n",  // raw_headers
        {},                                // expected_no_vary_params
        {},                                // expected_vary_params
        true,                              // expected_vary_on_key_order
        false,                             // expected_vary_by_default
    },
    // params set to true.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params=?1\r\n\r\n",  // raw_headers
        {},                                   // expected_no_vary_params
        {},                                   // expected_vary_params
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
        {"c"},       // expected_no_vary_params
        {},          // expected_vary_params
        true,        // expected_vary_on_key_order
        true,        // expected_vary_by_default
    },
    // Vary on all with one excepted search param.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        "No-Vary-Search: except=()\r\n\r\n",  // raw_headers
        {},                                   // expected_no_vary_params
        {},                                   // expected_vary_params
        true,                                 // expected_vary_on_key_order
        false,                                // expected_vary_by_default
    },
    // Vary on all with one excepted search param.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        R"(No-Vary-Search: except=("a"))"
        "\r\n\r\n",  // raw_headers
        {},          // expected_no_vary_params
        {"a"},       // expected_vary_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Vary on all with one excepted search param. Set params as
    // part of the same header line.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params,except=("a"))"
        "\r\n\r\n",  // raw_headers
        {},          // expected_no_vary_params
        {"a"},       // expected_vary_params
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
        {},          // expected_no_vary_params
        {"c"},       // expected_vary_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Vary on all with more than one excepted search param.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        R"(No-Vary-Search: except=("a" "b"))"
        "\r\n\r\n",  // raw_headers
        {},          // expected_no_vary_params
        {"a", "b"},  // expected_vary_params
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
        {},                                // expected_no_vary_params
        {"a", "b"},                        // expected_vary_params
        true,                              // expected_vary_on_key_order
        false,                             // expected_vary_by_default
    },
    // Vary on all with more than one excepted search param. Set params as
    // part of the same header line.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params,except=("a" "b"))"
        "\r\n\r\n",  // raw_headers
        {},          // expected_no_vary_params
        {"a", "b"},  // expected_vary_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Don't vary on two search params.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a" "b"))"
        "\r\n\r\n",  // raw_headers
        {"a", "b"},  // expected_no_vary_params
        {},          // expected_vary_params
        true,        // expected_vary_on_key_order
        true,        // expected_vary_by_default
    },
    // Don't vary on search params order.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: key-order\r\n\r\n",  // raw_headers
        {},                                   // expected_no_vary_params
        {},                                   // expected_vary_params
        false,                                // expected_vary_on_key_order
        true,                                 // expected_vary_by_default
    },
    // Don't vary on search params order.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: key-order=?1\r\n\r\n",  // raw_headers
        {},                                      // expected_no_vary_params
        {},                                      // expected_vary_params
        false,                                   // expected_vary_on_key_order
        true,                                    // expected_vary_by_default
    },
    // Don't vary on search params order and on two specific search params.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a" "b"))"
        "\r\n"
        "No-Vary-Search: key-order\r\n\r\n",  // raw_headers
        {"a", "b"},                           // expected_no_vary_params
        {},                                   // expected_vary_params
        false,                                // expected_vary_on_key_order
        true,                                 // expected_vary_by_default
    },
    // Don't vary on search params order and on two specific search params.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a" "b"))"
        "\r\n"
        "No-Vary-Search: key-order=?1\r\n\r\n",  // raw_headers
        {"a", "b"},                              // expected_no_vary_params
        {},                                      // expected_vary_params
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
        {"a", "b"},                              // expected_no_vary_params
        {},                                      // expected_vary_params
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
        {},                                   // expected_no_vary_params
        {"a"},                                // expected_vary_params
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
        {},                                   // expected_no_vary_params
        {"a"},                                // expected_vary_params
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
        {},                                      // expected_no_vary_params
        {"a"},                                   // expected_vary_params
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
        {},                                      // expected_no_vary_params
        {"a"},                                   // expected_vary_params
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
        {},                                   // expected_no_vary_params
        {"a", "b"},                           // expected_vary_params
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
        {"b"},       // expected_no_vary_params
        {},          // expected_vary_params
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
        {},                                // expected_no_vary_params
        {},                                // expected_vary_params
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
        {},          // expected_no_vary_params
        {"b"},       // expected_vary_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Allow extension via parameters.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params;unknown\r\n\r\n",  // raw_headers
        {},                                        // expected_no_vary_params
        {},                                        // expected_vary_params
        true,                                      // expected_vary_on_key_order
        false,                                     // expected_vary_by_default
    },
    // Allow extension via parameters.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a");unknown)"
        "\r\n\r\n",  // raw_headers
        {"a"},       // expected_no_vary_params
        {},          // expected_vary_params
        true,        // expected_vary_on_key_order
        true,        // expected_vary_by_default
    },
    // Allow extension via parameters.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params;unknown,except=("a");unknown)"
        "\r\n\r\n",  // raw_headers
        {},          // expected_no_vary_params
        {"a"},       // expected_vary_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    },
    // Allow extension via parameters.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: key-order;unknown\r\n\r\n",  // raw_headers
        {},                                           // expected_no_vary_params
        {},                                           // expected_vary_params
        false,  // expected_vary_on_key_order
        true,   // expected_vary_by_default
    },
    // Allow extension via parameters.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a";unknown))"
        "\r\n\r\n",  // raw_headers
        {"a"},       // expected_no_vary_params
        {},          // expected_vary_params
        true,        // expected_vary_on_key_order
        true,        // expected_vary_by_default
    },
    // Allow extension via parameters.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        R"(No-Vary-Search: except=("a";unknown))"
        "\r\n\r\n",  // raw_headers
        {},          // expected_no_vary_params
        {"a"},       // expected_vary_params
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
        {},          // expected_no_vary_params
        {"a"},       // expected_vary_params
        true,        // expected_vary_on_key_order
        false,       // expected_vary_by_default
    }};

INSTANTIATE_TEST_SUITE_P(HttpNoVarySearchResponseHeadersTest,
                         HttpNoVarySearchResponseHeadersTest,
                         testing::ValuesIn(response_headers_tests));

INSTANTIATE_TEST_SUITE_P(HttpNoVarySearchResponseHeadersParseFailureTest,
                         HttpNoVarySearchResponseHeadersParseFailureTest,
                         testing::ValuesIn(response_header_failed));

struct NoVarySearchCompareTestData {
  const GURL request_url;
  const GURL cached_url;
  const base::StringPiece raw_headers;
  const bool expected_match;
};

TEST(HttpNoVarySearchCompare,
     CheckUrlEqualityByNoVarySearchWithSpecialCharacters) {
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

  const base::StringPiece raw_headers =
      "HTTP/1.1 200 OK\r\n"
      R"(No-Vary-Search: params=("c"))"
      "\r\n\r\n";
  const std::string headers = net::HttpUtil::AssembleRawHeaders(raw_headers);
  const auto parsed = base::MakeRefCounted<HttpResponseHeaders>(headers);

  const auto no_vary_search_data =
      HttpNoVarySearchData::ParseFromHeaders(*parsed).value();

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

    const auto parsed_header = base::MakeRefCounted<HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(header_template));
    const auto no_vary_search_data_special_char =
        HttpNoVarySearchData::ParseFromHeaders(*parsed_header).value();

    EXPECT_TRUE(no_vary_search_data_special_char.AreEquivalent(
        GURL(request_url_template), GURL(cached_url_template)));
  }
}

class HttpNoVarySearchCompare
    : public ::testing::Test,
      public ::testing::WithParamInterface<NoVarySearchCompareTestData> {};

TEST_P(HttpNoVarySearchCompare, CheckUrlEqualityByNoVarySearch) {
  const auto& test_data = GetParam();

  const std::string headers =
      net::HttpUtil::AssembleRawHeaders(test_data.raw_headers);
  const auto parsed = base::MakeRefCounted<HttpResponseHeaders>(headers);

  const auto no_vary_search_data =
      HttpNoVarySearchData::ParseFromHeaders(*parsed).value();

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
};

INSTANTIATE_TEST_SUITE_P(HttpNoVarySearchCompare,
                         HttpNoVarySearchCompare,
                         testing::ValuesIn(no_vary_search_compare_tests));

}  // namespace

}  // namespace net
