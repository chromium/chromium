// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cookie_indices.h"

#include "net/cookies/cookie_util.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

using cookie_util::ParsedRequestCookies;
using ::testing::ElementsAre;
using ::testing::Optional;

constexpr std::string_view kCookieIndicesHeader = "Cookie-Indices";

TEST(CookieIndicesTest, Absent) {
  auto headers =
      HttpResponseHeaders::Builder(HttpVersion(1, 1), "200 OK").Build();
  auto result = ParseCookieIndices(*headers);
  EXPECT_FALSE(result.has_value());
}

TEST(CookieIndicesTest, PresentButEmpty) {
  auto headers = HttpResponseHeaders::Builder(HttpVersion(1, 1), "200 OK")
                     .AddHeader(kCookieIndicesHeader, "")
                     .Build();
  auto result = ParseCookieIndices(*headers);
  EXPECT_THAT(result, Optional(ElementsAre()));
}

TEST(CookieIndicesTest, OneCookie) {
  auto headers = HttpResponseHeaders::Builder(HttpVersion(1, 1), "200 OK")
                     .AddHeader(kCookieIndicesHeader, R"("alpha")")
                     .Build();
  auto result = ParseCookieIndices(*headers);
  EXPECT_THAT(result, Optional(ElementsAre("alpha")));
}

TEST(CookieIndicesTest, SeveralCookies) {
  auto headers =
      HttpResponseHeaders::Builder(HttpVersion(1, 1), "200 OK")
          .AddHeader(kCookieIndicesHeader, R"("alpha", "bravo")")
          .AddHeader(kCookieIndicesHeader, R"("charlie", "delta", "echo")")
          .Build();
  auto result = ParseCookieIndices(*headers);
  EXPECT_THAT(result, Optional(ElementsAre("alpha", "bravo", "charlie", "delta",
                                           "echo")));
}

TEST(CookieIndicesTest, NonRfc6265Cookie) {
  auto headers = HttpResponseHeaders::Builder(HttpVersion(1, 1), "200 OK")
                     .AddHeader(kCookieIndicesHeader, R"("text/html")")
                     .Build();
  auto result = ParseCookieIndices(*headers);
  EXPECT_THAT(result, Optional(ElementsAre()));
}

TEST(CookieIndicesTest, NotAList) {
  auto headers = HttpResponseHeaders::Builder(HttpVersion(1, 1), "200 OK")
                     .AddHeader(kCookieIndicesHeader, ",,,")
                     .Build();
  auto result = ParseCookieIndices(*headers);
  EXPECT_FALSE(result.has_value());
}

TEST(CookieIndicesTest, InnerList) {
  auto headers = HttpResponseHeaders::Builder(HttpVersion(1, 1), "200 OK")
                     .AddHeader(kCookieIndicesHeader, R"(("foo"))")
                     .Build();
  auto result = ParseCookieIndices(*headers);
  EXPECT_FALSE(result.has_value());
}

TEST(CookieIndicesTest, Token) {
  auto headers = HttpResponseHeaders::Builder(HttpVersion(1, 1), "200 OK")
                     .AddHeader(kCookieIndicesHeader, R"(alpha)")
                     .Build();
  auto result = ParseCookieIndices(*headers);
  EXPECT_FALSE(result.has_value());
}

TEST(CookieIndicesTest, StringWithUnrecognizedParam) {
  auto headers = HttpResponseHeaders::Builder(HttpVersion(1, 1), "200 OK")
                     .AddHeader(kCookieIndicesHeader, R"("session"; secure)")
                     .Build();
  auto result = ParseCookieIndices(*headers);
  EXPECT_THAT(result, Optional(ElementsAre("session")));
}

TEST(CookieIndicesTest, HashIgnoresCookieOrder) {
  const std::string cookie_indices[] = {"fruit", "vegetable"};
  EXPECT_EQ(HashCookieIndices(cookie_indices,
                              ParsedRequestCookies{{"fruit", "apple"},
                                                   {"vegetable", "tomato"}}),
            HashCookieIndices(cookie_indices,
                              ParsedRequestCookies{{"vegetable", "tomato"},
                                                   {"fruit", "apple"}}));
}

TEST(CookieIndicesTest, HashCaseSensitive) {
  const std::string cookie_indices[] = {"fruit", "vegetable"};
  EXPECT_NE(HashCookieIndices(cookie_indices,
                              ParsedRequestCookies{{"fruit", "apple"},
                                                   {"vegetable", "tomato"}}),
            HashCookieIndices(cookie_indices,
                              ParsedRequestCookies{{"Fruit", "apple"},
                                                   {"vegetable", "tomato"}}));
  EXPECT_NE(HashCookieIndices(cookie_indices,
                              ParsedRequestCookies{{"fruit", "apple"},
                                                   {"vegetable", "tomato"}}),
            HashCookieIndices(cookie_indices,
                              ParsedRequestCookies{{"fruit", "Apple"},
                                                   {"vegetable", "tomato"}}));
}

TEST(CookieIndicesTest, HashNotJustConcatenated) {
  // Any other simple delimiter would also be bad, but this is the most likely
  // case to result by accident.
  const std::string cookie_indices[] = {"fruit", "vegetable"};
  EXPECT_NE(HashCookieIndices(cookie_indices,
                              ParsedRequestCookies{{"fruit", "apple"},
                                                   {"vegetable", "tomato"}}),
            HashCookieIndices(cookie_indices,
                              ParsedRequestCookies{{"fruit", "app"},
                                                   {"vegetable", "letomato"}}));
}

TEST(CookieIndicesTest, HashDisregardsOtherCookies) {
  const std::string cookie_indices[] = {"fruit"};
  EXPECT_EQ(HashCookieIndices(cookie_indices,
                              ParsedRequestCookies{{"fruit", "apple"},
                                                   {"vegetable", "tomato"}}),
            HashCookieIndices(cookie_indices,
                              ParsedRequestCookies{{"bread", "pumpernickel"},
                                                   {"fruit", "apple"}}));
}

TEST(CookieIndicesTest, HashDistinguishesEmptyAndAbsentCookies) {
  const std::string cookie_indices[] = {"fruit"};
  EXPECT_NE(
      HashCookieIndices(cookie_indices, ParsedRequestCookies{{"fruit", ""}}),
      HashCookieIndices(cookie_indices, ParsedRequestCookies{}));
}

TEST(CookieIndicesTest, IgnoresOrderOfDuplicateCookies) {
  const std::string cookie_indices[] = {"fruit"};
  EXPECT_EQ(HashCookieIndices(
                cookie_indices,
                ParsedRequestCookies{{"fruit", "lime"}, {"fruit", "pear"}}),
            HashCookieIndices(
                cookie_indices,
                ParsedRequestCookies{{"fruit", "pear"}, {"fruit", "lime"}}));
}

}  // namespace
}  // namespace net
