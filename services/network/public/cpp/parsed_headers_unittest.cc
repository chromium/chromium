// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/parsed_headers.h"

#include <string>
#include <string_view>
#include <tuple>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "net/base/features.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

mojom::ParsedHeadersPtr ParseHeaders(std::string_view headers) {
  std::string raw_headers = net::HttpUtil::AssembleRawHeaders(headers);
  auto parsed = base::MakeRefCounted<net::HttpResponseHeaders>(raw_headers);
  return network::PopulateParsedHeaders(parsed.get(), GURL("https://a.com"));
}

TEST(NoVarySearchPrefetchTest, ParsingNVSReturnsDefaultURLVariance) {
  const std::string_view& headers =
      "HTTP/1.1 200 OK\r\n"
      "Set-Cookie: a\r\n"
      "Set-Cookie: b\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  EXPECT_TRUE(parsed_headers);
  EXPECT_TRUE(parsed_headers->no_vary_search_with_parse_error);
  EXPECT_TRUE(
      parsed_headers->no_vary_search_with_parse_error->is_parse_error());
  EXPECT_EQ(mojom::NoVarySearchParseError::kOk,
            parsed_headers->no_vary_search_with_parse_error->get_parse_error());
}

TEST(NoVarySearchPrefetchTest, ParsingNVSReturnsDefaultValue) {
  const std::string_view& headers =
      "HTTP/1.1 200 OK\r\n"
      "Set-Cookie: a\r\n"
      "Set-Cookie: b\r\n"
      "No-Vary-Search: params=?0\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  EXPECT_TRUE(parsed_headers);
  EXPECT_TRUE(parsed_headers->no_vary_search_with_parse_error);
  EXPECT_TRUE(
      parsed_headers->no_vary_search_with_parse_error->is_parse_error());
  EXPECT_EQ(mojom::NoVarySearchParseError::kDefaultValue,
            parsed_headers->no_vary_search_with_parse_error->get_parse_error());
}

TEST(NoVarySearchPrefetchTest, ParsingNVSReturnsNotDictionary) {
  const std::string_view& headers =
      "HTTP/1.1 200 OK\r\n"
      "Set-Cookie: a\r\n"
      "Set-Cookie: b\r\n"
      "No-Vary-Search: (a)\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  EXPECT_TRUE(parsed_headers);
  EXPECT_TRUE(parsed_headers->no_vary_search_with_parse_error);
  EXPECT_TRUE(
      parsed_headers->no_vary_search_with_parse_error->is_parse_error());
  EXPECT_EQ(mojom::NoVarySearchParseError::kNotDictionary,
            parsed_headers->no_vary_search_with_parse_error->get_parse_error());
}

TEST(NoVarySearchPrefetchTest, ParsingNVSReturnsUnknownDictionaryKey) {
  if (base::FeatureList::IsEnabled(
          net::features::kNoVarySearchIgnoreUnrecognizedKeys)) {
    GTEST_SKIP() << "unrecognized keys are now ignored";
  }
  const std::string_view& headers =
      "HTTP/1.1 200 OK\r\n"
      "Set-Cookie: a\r\n"
      "Set-Cookie: b\r\n"
      "No-Vary-Search: a\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  EXPECT_TRUE(parsed_headers);
  EXPECT_TRUE(parsed_headers->no_vary_search_with_parse_error);
  EXPECT_TRUE(
      parsed_headers->no_vary_search_with_parse_error->is_parse_error());
  EXPECT_EQ(mojom::NoVarySearchParseError::kUnknownDictionaryKey,
            parsed_headers->no_vary_search_with_parse_error->get_parse_error());
}

TEST(NoVarySearchPrefetchTest, ParsingNVSReturnsNonBooleanKeyOrder) {
  const std::string_view& headers =
      "HTTP/1.1 200 OK\r\n"
      "Set-Cookie: a\r\n"
      "Set-Cookie: b\r\n"
      "No-Vary-Search: key-order=a\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  EXPECT_TRUE(parsed_headers);
  EXPECT_TRUE(parsed_headers->no_vary_search_with_parse_error);
  EXPECT_TRUE(
      parsed_headers->no_vary_search_with_parse_error->is_parse_error());
  EXPECT_EQ(mojom::NoVarySearchParseError::kNonBooleanKeyOrder,
            parsed_headers->no_vary_search_with_parse_error->get_parse_error());
}

TEST(NoVarySearchPrefetchTest, ParsingNVSReturnsParamsNotStringList) {
  const std::string_view& headers =
      "HTTP/1.1 200 OK\r\n"
      "Set-Cookie: a\r\n"
      "Set-Cookie: b\r\n"
      "No-Vary-Search: params=a\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  EXPECT_TRUE(parsed_headers);
  EXPECT_TRUE(parsed_headers->no_vary_search_with_parse_error);
  EXPECT_TRUE(
      parsed_headers->no_vary_search_with_parse_error->is_parse_error());
  EXPECT_EQ(mojom::NoVarySearchParseError::kParamsNotStringList,
            parsed_headers->no_vary_search_with_parse_error->get_parse_error());
}

TEST(NoVarySearchPrefetchTest, ParsingNVSReturnsExceptNotStringList) {
  const std::string_view& headers =
      "HTTP/1.1 200 OK\r\n"
      "Set-Cookie: a\r\n"
      "Set-Cookie: b\r\n"
      "No-Vary-Search: params, except=a\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  EXPECT_TRUE(parsed_headers);
  EXPECT_TRUE(parsed_headers->no_vary_search_with_parse_error);
  EXPECT_TRUE(
      parsed_headers->no_vary_search_with_parse_error->is_parse_error());
  EXPECT_EQ(mojom::NoVarySearchParseError::kExceptNotStringList,
            parsed_headers->no_vary_search_with_parse_error->get_parse_error());
}

TEST(NoVarySearchPrefetchTest, ParsingNVSReturnsExceptWithoutTrueParams) {
  const std::string_view& headers =
      "HTTP/1.1 200 OK\r\n"
      "Set-Cookie: a\r\n"
      "Set-Cookie: b\r\n"
      "No-Vary-Search: except=()\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  EXPECT_TRUE(parsed_headers);
  EXPECT_TRUE(parsed_headers->no_vary_search_with_parse_error);
  EXPECT_TRUE(
      parsed_headers->no_vary_search_with_parse_error->is_parse_error());
  EXPECT_EQ(mojom::NoVarySearchParseError::kExceptWithoutTrueParams,
            parsed_headers->no_vary_search_with_parse_error->get_parse_error());
}

struct NoVarySearchTestData {
  const char* raw_headers;
  const std::vector<std::string> expected_no_vary_params;
  const std::vector<std::string> expected_vary_params;
  const bool expected_vary_on_key_order;
  const bool expected_vary_by_default;
};

class NoVarySearchPrefetchTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<NoVarySearchTestData> {};

TEST_P(NoVarySearchPrefetchTest, ParsingSuccess) {
  const auto& test_data = GetParam();
  std::string headers =
      net::HttpUtil::AssembleRawHeaders(test_data.raw_headers);

  auto parsed = base::MakeRefCounted<net::HttpResponseHeaders>(headers);
  const auto parsed_headers =
      network::PopulateParsedHeaders(parsed.get(), GURL("https://a.com"));

  EXPECT_TRUE(parsed_headers);
  ASSERT_TRUE(parsed_headers->no_vary_search_with_parse_error);
  ASSERT_TRUE(
      parsed_headers->no_vary_search_with_parse_error->is_no_vary_search());
  auto& no_vary_search =
      parsed_headers->no_vary_search_with_parse_error->get_no_vary_search();
  ASSERT_TRUE(no_vary_search->search_variance);
  if (test_data.expected_vary_by_default) {
    EXPECT_THAT(no_vary_search->search_variance->get_no_vary_params(),
                test_data.expected_no_vary_params);
  } else {
    EXPECT_THAT(no_vary_search->search_variance->get_vary_params(),
                test_data.expected_vary_params);
  }
  EXPECT_EQ(no_vary_search->vary_on_key_order,
            test_data.expected_vary_on_key_order);
}

NoVarySearchTestData response_headers_tests[] = {
    // params set to a list of strings with one element.
    {
        "HTTP/1.1 200 OK\r\n"
        R"(No-Vary-Search: params=("a"))"
        "\r\n\r\n",                       // raw_headers
        std::vector<std::string>({"a"}),  // expected_no_vary_params
        {},                               // expected_vary_params
        true,                             // expected_vary_on_key_order
        true,                             // expected_vary_by_default
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
    // Vary on one search param.
    {
        "HTTP/1.1 200 OK\r\n"
        "No-Vary-Search: params\r\n"
        R"(No-Vary-Search: except=("a"))"
        "\r\n\r\n",                       // raw_headers
        {},                               // expected_no_vary_params
        std::vector<std::string>({"a"}),  // expected_vary_params
        true,                             // expected_vary_on_key_order
        false,                            // expected_vary_by_default
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
};

INSTANTIATE_TEST_SUITE_P(NoVarySearchPrefetchTest,
                         NoVarySearchPrefetchTest,
                         testing::ValuesIn(response_headers_tests));

TEST(ParseHeadersClientHintsTest, AcceptCHAndClearCHWithoutClearSiteDataTest) {
  const std::string_view& headers =
      "HTTP/1.1 200 OK\r\n"
      "Accept-CH: sec-ch-dpr\r\n"
      "Critical-CH: sec-ch-dpr\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  EXPECT_TRUE(parsed_headers);
  EXPECT_FALSE(
      parsed_headers->client_hints_ignored_due_to_clear_site_data_header);
  EXPECT_TRUE(parsed_headers->accept_ch);
  EXPECT_EQ(parsed_headers->accept_ch->size(), 1u);
  EXPECT_EQ(parsed_headers->accept_ch->at(0),
            network::mojom::WebClientHintsType::kDpr);
  EXPECT_TRUE(parsed_headers->critical_ch);
  EXPECT_EQ(parsed_headers->critical_ch->size(), 1u);
  EXPECT_EQ(parsed_headers->critical_ch->at(0),
            network::mojom::WebClientHintsType::kDpr);
}

TEST(ParseHeadersClientHintsTest,
     AcceptCHAndClearCHWithClearSiteDataCacheTest) {
  const std::string_view& headers =
      "HTTP/1.1 200 OK\r\n"
      "Accept-CH: sec-ch-dpr\r\n"
      "Critical-CH: sec-ch-dpr\r\n"
      "Clear-Site-Data: \"cache\"\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  EXPECT_TRUE(parsed_headers);
  EXPECT_TRUE(
      parsed_headers->client_hints_ignored_due_to_clear_site_data_header);
  EXPECT_FALSE(parsed_headers->accept_ch);
  EXPECT_FALSE(parsed_headers->critical_ch);
}

TEST(ParseHeadersClientHintsTest,
     AcceptCHAndClearCHWithClearSiteDataClientHintsTest) {
  const std::string_view& headers =
      "HTTP/1.1 200 OK\r\n"
      "Accept-CH: sec-ch-dpr\r\n"
      "Critical-CH: sec-ch-dpr\r\n"
      "Clear-Site-Data: \"clientHints\"\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  EXPECT_TRUE(parsed_headers);
  EXPECT_TRUE(
      parsed_headers->client_hints_ignored_due_to_clear_site_data_header);
  EXPECT_FALSE(parsed_headers->accept_ch);
  EXPECT_FALSE(parsed_headers->critical_ch);
}

TEST(ParseHeadersClientHintsTest,
     AcceptCHAndClearCHWithClearSiteDataCookiesTest) {
  const std::string_view& headers =
      "HTTP/1.1 200 OK\r\n"
      "Accept-CH: sec-ch-dpr\r\n"
      "Critical-CH: sec-ch-dpr\r\n"
      "Clear-Site-Data: \"cookies\"\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  EXPECT_TRUE(parsed_headers);
  EXPECT_TRUE(
      parsed_headers->client_hints_ignored_due_to_clear_site_data_header);
  EXPECT_FALSE(parsed_headers->accept_ch);
  EXPECT_FALSE(parsed_headers->critical_ch);
}

TEST(ParseHeadersClientHintsTest,
     AcceptCHAndClearCHWithClearSiteDataStorageTest) {
  const std::string_view& headers =
      "HTTP/1.1 200 OK\r\n"
      "Accept-CH: sec-ch-dpr\r\n"
      "Critical-CH: sec-ch-dpr\r\n"
      "Clear-Site-Data: \"storage\"\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  EXPECT_TRUE(parsed_headers);
  EXPECT_FALSE(
      parsed_headers->client_hints_ignored_due_to_clear_site_data_header);
  EXPECT_TRUE(parsed_headers->accept_ch);
  EXPECT_EQ(parsed_headers->accept_ch->size(), 1u);
  EXPECT_EQ(parsed_headers->accept_ch->at(0),
            network::mojom::WebClientHintsType::kDpr);
  EXPECT_TRUE(parsed_headers->critical_ch);
  EXPECT_EQ(parsed_headers->critical_ch->size(), 1u);
  EXPECT_EQ(parsed_headers->critical_ch->at(0),
            network::mojom::WebClientHintsType::kDpr);
}

TEST(ParseHeadersClientHintsTest, AcceptCHAndClearCHWithClearSiteDataAllTest) {
  const std::string_view& headers =
      "HTTP/1.1 200 OK\r\n"
      "Accept-CH: sec-ch-dpr\r\n"
      "Critical-CH: sec-ch-dpr\r\n"
      "Clear-Site-Data: \"*\"\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  EXPECT_TRUE(parsed_headers);
  EXPECT_TRUE(
      parsed_headers->client_hints_ignored_due_to_clear_site_data_header);
  EXPECT_FALSE(parsed_headers->accept_ch);
  EXPECT_FALSE(parsed_headers->critical_ch);
}

TEST(ParsedHeadersTest, CookieIndices) {
  base::test::ScopedFeatureList enable{features::kCookieIndicesHeader};
  const std::string_view headers =
      "HTTP/1.1 200 OK\r\n"
      "Cookie-Indices: \"logged_in\", \"user_lang\"\r\n\r\n";
  const auto parsed_headers = ParseHeaders(headers);

  ASSERT_TRUE(parsed_headers);
  EXPECT_THAT(
      parsed_headers->cookie_indices,
      ::testing::Optional(::testing::ElementsAre("logged_in", "user_lang")));
}

}  // namespace
}  // namespace network
