// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cookie_indices.h"

#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

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

}  // namespace
}  // namespace net
