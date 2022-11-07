// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_no_vary_search_data.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

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

  std::string raw_headers = net::HttpUtil::AssembleRawHeaders(test.raw_headers);

  auto parsed = base::MakeRefCounted<HttpResponseHeaders>(raw_headers);

  absl::optional<HttpNoVarySearchData> no_vary_search_data =
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
  std::string raw_headers = net::HttpUtil::AssembleRawHeaders(GetParam());

  auto parsed = base::MakeRefCounted<HttpResponseHeaders>(raw_headers);

  absl::optional<HttpNoVarySearchData> no_vary_search_data =
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

TestData response_headers_tests[] = {
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

}  // namespace

}  // namespace net
