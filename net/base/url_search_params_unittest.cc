// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/url_search_params.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {
namespace {

using ::testing::ElementsAre;
using ::testing::Pair;

TEST(UrlSearchParamsTest, ParseAllSearchParams) {
  const UrlSearchParams search_params(
      GURL("https://a.test/index.html?a=1&b=2&c=3"));
  EXPECT_THAT(search_params.params(),
              ElementsAre(Pair("a", "1"), Pair("b", "2"), Pair("c", "3")));
}

TEST(UrlSearchParamsTest, ParseSearchParamUnescapeValue) {
  const UrlSearchParams search_params(
      GURL(R"(https://a.test/index.html?a=a%20b%20c)"));
  EXPECT_EQ(search_params.params().size(), 1u);
  EXPECT_EQ(search_params.params()[0].second, "a b c");
}

TEST(UrlSearchParamsTest, DeleteOneSearchParams) {
  UrlSearchParams search_params(GURL("https://a.test/index.html?a=1&b=2&c=3"));
  search_params.DeleteAllWithNames({"b"});
  EXPECT_THAT(search_params.params(),
              ElementsAre(Pair("a", "1"), Pair("c", "3")));
}

TEST(UrlSearchParamsTest, DeleteAllExceptOneSearchParams) {
  UrlSearchParams search_params(GURL("https://a.test/index.html?a=1&b=2&c=3"));
  search_params.DeleteAllExceptWithNames({"b"});
  EXPECT_THAT(search_params.params(), ElementsAre(Pair("b", "2")));
}

TEST(UrlSearchParamsTest, SortSearchParams) {
  UrlSearchParams search_params(
      GURL("https://a.test/index.html?c=3&b=2&a=1&c=2&a=5"));
  search_params.Sort();
  EXPECT_THAT(search_params.params(),
              ElementsAre(Pair("a", "1"), Pair("a", "5"), Pair("b", "2"),
                          Pair("c", "3"), Pair("c", "2")));
}

TEST(UrlSearchParamsTest, SortSearchParamsPercentEncoded) {
  UrlSearchParams search_params(
      GURL("https://a.test/index.html?c=3&b=2&a=1&%63=2&a=5"));
  search_params.Sort();
  EXPECT_THAT(search_params.params(),
              ElementsAre(Pair("a", "1"), Pair("a", "5"), Pair("b", "2"),
                          Pair("c", "3"), Pair("c", "2")));
}

TEST(UrlSearchParamsTest, ParseSearchParamsSpacePlusAndPercentEncoded) {
  const UrlSearchParams search_params(
      GURL(R"(https://a.test/index.html?c+1=3&b+%202=2&a=1&%63%201=2&a=5)"));
  EXPECT_THAT(search_params.params(),
              ElementsAre(Pair("c 1", "3"), Pair("b  2", "2"), Pair("a", "1"),
                          Pair("c 1", "2"), Pair("a", "5")));
}

TEST(UrlSearchParamsTest, ParseSearchParamsDoubleCodePoint) {
  const UrlSearchParams search_params(
      GURL(R"(https://a.test/index.html?%C3%A9=foo)"));
  EXPECT_THAT(search_params.params(), ElementsAre(Pair("é", "foo")));
}

TEST(UrlSearchParamsTest, SortSearchParamsDoubleCodePoint) {
  UrlSearchParams search_params(
      GURL(R"(https://a.test/index.html?%C3%A9=f&a=2&c=4&é=b)"));
  search_params.Sort();
  EXPECT_THAT(search_params.params(),
              ElementsAre(Pair("a", "2"), Pair("c", "4"), Pair("é", "f"),
                          Pair("é", "b")));
}

TEST(UrlSearchParamsTest, ParseSearchParamsTripleCodePoint) {
  const UrlSearchParams search_params(
      GURL(R"(https://a.test/index.html?%E3%81%81=foo)"));
  EXPECT_THAT(search_params.params(), ElementsAre(Pair("ぁ", "foo")));
}

TEST(UrlSearchParamsTest, ParseSearchParamsQuadrupleCodePoint) {
  const UrlSearchParams search_params(
      GURL(R"(https://a.test/index.html?%F0%90%A8%80=foo)"));
  EXPECT_THAT(search_params.params(), ElementsAre(Pair("𐨀", "foo")));
}

// In case an invalid UTF-8 sequence is entered, it would be replaced with
// the U+FFFD REPLACEMENT CHARACTER: �.
TEST(UrlSearchParamsTest, ParseSearchParamsInvalidCodePoint) {
  const UrlSearchParams search_params(
      GURL(R"(https://a.test/index.html?%C3=foo)"));
  EXPECT_THAT(search_params.params(), ElementsAre(Pair("�", "foo")));
}

TEST(UrlSearchParamsTest, ParseSearchParamsSpecialCharacters) {
  // Use special characters in both `keys` and `values`.
  const base::flat_map<std::string, std::string> percent_encoding = {
      {"!", "%21"},    {R"(")", "%22"},  // double quote character: "
      {"#", "%23"},    {"$", "%24"},       {"%", "%25"},    {"&", "%26"},
      {"'", "%27"},    {"(", "%28"},       {")", "%29"},    {"*", R"(%2A)"},
      {"+", R"(%2B)"}, {",", R"(%2C)"},    {"-", R"(%2D)"}, {".", R"(%2E)"},
      {"/", R"(%2F)"}, {":", R"(%3A)"},    {";", "%3B"},    {"<", R"(%3C)"},
      {"=", R"(%3D)"}, {">", R"(%3E)"},    {"?", R"(%3F)"}, {"@", "%40"},
      {"[", "%5B"},    {R"(\)", R"(%5C)"}, {"]", R"(%5D)"}, {"^", R"(%5E)"},
      {"_", R"(%5F)"}, {"`", "%60"},       {"{", "%7B"},    {"|", R"(%7C)"},
      {"}", R"(%7D)"}, {"~", R"(%7E)"},    {"", ""},
  };

  for (const auto& [key, value] : percent_encoding) {
    std::string template_url = R"(https://a.test/index.html?$key=$value)";

    base::ReplaceSubstringsAfterOffset(&template_url, 0, "$key", value);
    base::ReplaceSubstringsAfterOffset(&template_url, 0, "$value", value);

    const UrlSearchParams search_params = UrlSearchParams(GURL(template_url));
    EXPECT_THAT(search_params.params(), ElementsAre(Pair(key, key)));
  }
}

TEST(UrlSearchParamsTest, ParseSearchParamsEmptyKeyOrValues) {
  const UrlSearchParams search_params(
      GURL("https://a.test/index.html?a&b&c&d&=5&=1"));
  EXPECT_THAT(search_params.params(),
              ElementsAre(Pair("a", ""), Pair("b", ""), Pair("c", ""),
                          Pair("d", ""), Pair("", "5"), Pair("", "1")));
}

TEST(UrlSearchParamsTest, ParseSearchParamsInvalidEscapeTest) {
  const UrlSearchParams search_params(
      GURL(R"(https://a.test/index.html?a=%3&%3=b)"));
  EXPECT_THAT(search_params.params(),
              ElementsAre(Pair("a", "%3"), Pair("%3", "b")));
}

}  // namespace
}  // namespace net
