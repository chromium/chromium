// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/url_search_params_view.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {
namespace {

using ::testing::ElementsAre;
using ::testing::Pair;

TEST(UrlSearchParamsViewTest, ParseAllSearchParams) {
  const GURL url("https://a.test/index.html?a=1&b=2&c=3");
  const UrlSearchParamsView search_params(url);
  EXPECT_THAT(search_params.GetDecodedParamsForTesting(),
              ElementsAre(Pair("a", "1"), Pair("b", "2"), Pair("c", "3")));
}

TEST(UrlSearchParamsViewTest, ParseSearchParamUnescapeValue) {
  const GURL url(R"(https://a.test/index.html?a=a%20b%20c)");
  const UrlSearchParamsView search_params(url);
  EXPECT_EQ(search_params.GetDecodedParamsForTesting().size(), 1u);
  EXPECT_EQ(search_params.GetDecodedParamsForTesting()[0].second, "a b c");
}

TEST(UrlSearchParamsViewTest, DeleteOneSearchParams) {
  const GURL url("https://a.test/index.html?a=1&b=2&c=3");
  UrlSearchParamsView search_params(url);
  search_params.DeleteAllWithNames({"b"});
  EXPECT_THAT(search_params.GetDecodedParamsForTesting(),
              ElementsAre(Pair("a", "1"), Pair("c", "3")));
}

TEST(UrlSearchParamsViewTest, DeleteAllExceptOneSearchParams) {
  const GURL url("https://a.test/index.html?a=1&b=2&c=3");
  UrlSearchParamsView search_params(url);
  search_params.DeleteAllExceptWithNames({"b"});
  EXPECT_THAT(search_params.GetDecodedParamsForTesting(),
              ElementsAre(Pair("b", "2")));
}

TEST(UrlSearchParamsViewTest, SortSearchParams) {
  const GURL url("https://a.test/index.html?c=3&b=2&a=1&c=2&a=5");
  UrlSearchParamsView search_params(url);
  search_params.Sort();
  EXPECT_THAT(search_params.GetDecodedParamsForTesting(),
              ElementsAre(Pair("a", "1"), Pair("a", "5"), Pair("b", "2"),
                          Pair("c", "3"), Pair("c", "2")));
}

TEST(UrlSearchParamsViewTest, SortSearchParamsPercentEncoded) {
  const GURL url("https://a.test/index.html?c=3&b=2&a=1&%63=2&a=5");
  UrlSearchParamsView search_params(url);
  search_params.Sort();
  EXPECT_THAT(search_params.GetDecodedParamsForTesting(),
              ElementsAre(Pair("a", "1"), Pair("a", "5"), Pair("b", "2"),
                          Pair("c", "3"), Pair("c", "2")));
}

TEST(UrlSearchParamsViewTest, ParseSearchParamsSpacePlusAndPercentEncoded) {
  const GURL url(
      R"(https://a.test/index.html?c+1=3&b+%202=2&a=1&%63%201=2&a=5)");
  const UrlSearchParamsView search_params(url);
  EXPECT_THAT(search_params.GetDecodedParamsForTesting(),
              ElementsAre(Pair("c 1", "3"), Pair("b  2", "2"), Pair("a", "1"),
                          Pair("c 1", "2"), Pair("a", "5")));
}

TEST(UrlSearchParamsViewTest, ParseSearchParamsDoubleCodePoint) {
  const GURL url(R"(https://a.test/index.html?%C3%A9=foo)");
  const UrlSearchParamsView search_params(url);
  EXPECT_THAT(search_params.GetDecodedParamsForTesting(),
              ElementsAre(Pair("é", "foo")));
}

TEST(UrlSearchParamsViewTest, SortSearchParamsDoubleCodePoint) {
  const GURL url(R"(https://a.test/index.html?%C3%A9=f&a=2&c=4&é=b)");
  UrlSearchParamsView search_params(url);
  search_params.Sort();
  EXPECT_THAT(search_params.GetDecodedParamsForTesting(),
              ElementsAre(Pair("a", "2"), Pair("c", "4"), Pair("é", "f"),
                          Pair("é", "b")));
}

TEST(UrlSearchParamsViewTest, ParseSearchParamsTripleCodePoint) {
  const GURL url(R"(https://a.test/index.html?%E3%81%81=foo)");
  const UrlSearchParamsView search_params(url);
  EXPECT_THAT(search_params.GetDecodedParamsForTesting(),
              ElementsAre(Pair("ぁ", "foo")));
}

TEST(UrlSearchParamsViewTest, ParseSearchParamsQuadrupleCodePoint) {
  const GURL url(R"(https://a.test/index.html?%F0%90%A8%80=foo)");
  const UrlSearchParamsView search_params(url);
  EXPECT_THAT(search_params.GetDecodedParamsForTesting(),
              ElementsAre(Pair("𐨀", "foo")));
}

// In case an invalid UTF-8 sequence is entered, it would be replaced with
// the U+FFFD REPLACEMENT CHARACTER: �.
TEST(UrlSearchParamsViewTest, ParseSearchParamsInvalidCodePoint) {
  const GURL url(R"(https://a.test/index.html?%C3=foo)");
  const UrlSearchParamsView search_params(url);
  EXPECT_THAT(search_params.GetDecodedParamsForTesting(),
              ElementsAre(Pair("�", "foo")));
}

TEST(UrlSearchParamsViewTest, ParseSearchParamsSpecialCharacters) {
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

    const GURL url(template_url);
    const UrlSearchParamsView search_params = UrlSearchParamsView(url);
    EXPECT_THAT(search_params.GetDecodedParamsForTesting(),
                ElementsAre(Pair(key, key)));
  }
}

TEST(UrlSearchParamsViewTest, ParseSearchParamsEmptyKeyOrValues) {
  const GURL url("https://a.test/index.html?a&b&c&d&=5&=1");
  const UrlSearchParamsView search_params(url);
  EXPECT_THAT(search_params.GetDecodedParamsForTesting(),
              ElementsAre(Pair("a", ""), Pair("b", ""), Pair("c", ""),
                          Pair("d", ""), Pair("", "5"), Pair("", "1")));
}

TEST(UrlSearchParamsViewTest, ParseSearchParamsInvalidEscapeTest) {
  const GURL url(R"(https://a.test/index.html?a=%3&%3=b)");
  const UrlSearchParamsView search_params(url);
  EXPECT_THAT(search_params.GetDecodedParamsForTesting(),
              ElementsAre(Pair("a", "%3"), Pair("%3", "b")));
}

struct QueryPair {
  std::string_view query_a;
  std::string_view query_b;
};

GURL MakeUrlWithQuery(std::string_view query) {
  // The trailing "#" makes GURL preserve trailing spaces. It doesn't form part
  // of the query and makes no difference in other cases.
  return GURL(base::StrCat({"http://a.test/?", query, "#"}));
}

TEST(UrlSearchParamsViewTest, Equals) {
  static constexpr QueryPair kEqualsCases[] = {
      {"a", "a="},
      {"a=+", "a=%20"},
      {"=&", "="},
      {"%61=%62", "a=b"},
      {"%c0=%c1", "%c1=%c0"},
      {"%61=1&a=2", "a=1&%61=2"},
      {"a=5&b=%6", "a=5&b=%6"},
  };

  for (const auto& [query_a, query_b] : kEqualsCases) {
    const GURL url_a = MakeUrlWithQuery(query_a);
    const GURL url_b = MakeUrlWithQuery(query_b);
    const UrlSearchParamsView search_params_a(url_a);
    const UrlSearchParamsView search_params_b(url_b);
    EXPECT_EQ(search_params_a, search_params_b)
        << "a=" << url_a << ", b=" << url_b;
  }
}

TEST(UrlSearchParamsViewTest, NotEquals) {
  static constexpr QueryPair kNotEqualsCases[] = {
      {"a=1&a=2", "a=2&a=1"},
      {"a=%c2%a2", "a=%c2%a3"},
      {"a", "b"},
      {"%c2%a2", "%c2%a3"},
      {"=&=", "="},
      {"A=0", "a=0"},
      {"%cO=1", "%co=1"},
      {"%c0=", "=%c0"},
  };

  for (const auto& [query_a, query_b] : kNotEqualsCases) {
    const GURL url_a = MakeUrlWithQuery(query_a);
    const GURL url_b = MakeUrlWithQuery(query_b);
    const UrlSearchParamsView search_params_a(url_a);
    const UrlSearchParamsView search_params_b(url_b);
    EXPECT_NE(search_params_a, search_params_b)
        << "a=" << url_a << ", b=" << url_b;
  }
}

TEST(UrlSearchParamsViewTest, SerializeAsUtf8WithNoQuery) {
  const GURL no_query("http://a.test/");
  const UrlSearchParamsView params(no_query);
  EXPECT_EQ(params.SerializeAsUtf8(), "");
}

TEST(UrlSearchParamsViewTest, SerializeAsUtf8Cases) {
  static constexpr std::string_view kNulEqualsNul("\0=\0", 3);
  static constexpr QueryPair kQueryAndExpected[] = {
      {"", ""},
      {"a", "a="},
      {"&", "="},
      {"&b", "=&b="},
      {"&&", "=&="},
      {"a==b", "a=%3Db"},
      {"%3d=b", "%3D=b"},
      {"%26&=%26", "%26=&=%26"},   // '&' is escaped in keys and values
      {"+=+", " = "},              // ' ' is not escaped
      {"%20=+%20+", " =   "},      // %20 is ' '
      {"%=%", "%25=%25"},          // Invalid escape is made valid
      {"%23=%23", "%23=%23"},      // '#' is escaped
      {kNulEqualsNul, "%00=%00"},  // '\0' is escaped
      {"%c2%a5", "\xc2\xa5="},     // Valid UTF-8 is not escaped
      {"=%c2%a5", "=\xc2\xa5"},
      {"%c2=%a5", "\xef\xbf\xbd=\xef\xbf\xbd"},  // Invalid UTF-8
  };
  for (const auto& [query, expected] : kQueryAndExpected) {
    SCOPED_TRACE(testing::Message() << "query=\"" << query << "\", expected=\""
                                    << expected << "\"");
    const GURL url(MakeUrlWithQuery(query));
    const UrlSearchParamsView params(url);
    const std::string serialized = params.SerializeAsUtf8();
    EXPECT_EQ(serialized, expected);

    // Loop the result through GURL and UrlSearchParamsView again to verify it
    // is unchanged. This property is not strictly necessary, but it is a useful
    // way to guarantee that the output is SerializeAsUtf8() is an injective
    // mapping w.r.t. UrlSearchParamsView objects.
    const GURL url_from_serialized(MakeUrlWithQuery(serialized));
    const UrlSearchParamsView recreated_params(url_from_serialized);
    EXPECT_EQ(recreated_params.SerializeAsUtf8(), serialized);
  }
}

}  // namespace
}  // namespace net
