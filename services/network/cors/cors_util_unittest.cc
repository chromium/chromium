// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace network::cors {

namespace {

using CorsUtilTest = testing::Test;

TEST_F(CorsUtilTest, CorsUnsafeNotForbiddenRequestHeaderNames) {
  // Needed because initializer list is not allowed for a macro argument.
  using List = std::vector<std::string>;

  // Empty => Empty
  EXPECT_EQ(CorsUnsafeNotForbiddenRequestHeaderNames({}), List({}));

  // "user-agent" is NOT forbidden per spec, but forbidden in Chromium.
  EXPECT_EQ(
      CorsUnsafeNotForbiddenRequestHeaderNames({{"content-type", "text/plain"},
                                                {"dpr", "12345"},
                                                {"aCCept", "en,ja"},
                                                {"accept-charset", "utf-8"},
                                                {"uSer-Agent", "foo"},
                                                {"hogE", "fuga"}}),
      List({"hoge"}));

  EXPECT_EQ(
      CorsUnsafeNotForbiddenRequestHeaderNames({{"content-type", "text/html"},
                                                {"dpr", "123-45"},
                                                {"aCCept", "en,ja"},
                                                {"accept-charset", "utf-8"},
                                                {"hogE", "fuga"}}),
      List({"content-type", "dpr", "hoge"}));

  // `safelistValueSize` is 1024.
  EXPECT_EQ(
      CorsUnsafeNotForbiddenRequestHeaderNames(
          {{"content-type", "text/plain; charset=" + std::string(108, '1')},
           {"accept", std::string(128, '1')},
           {"accept-language", std::string(128, '1')},
           {"content-language", std::string(128, '1')},
           {"dpr", std::string(128, '1')},
           {"device-memory", std::string(128, '1')},
           {"save-data", "on"},
           {"viewport-width", std::string(128, '1')},
           {"width", std::string(126, '1')},
           {"accept-charset", "utf-8"},
           {"hogE", "fuga"}}),
      List({"hoge"}));

  // `safelistValueSize` is 1025.
  EXPECT_EQ(
      CorsUnsafeNotForbiddenRequestHeaderNames(
          {{"content-type", "text/plain; charset=" + std::string(108, '1')},
           {"accept", std::string(128, '1')},
           {"accept-language", std::string(128, '1')},
           {"content-language", std::string(128, '1')},
           {"dpr", std::string(128, '1')},
           {"device-memory", std::string(128, '1')},
           {"save-data", "on"},
           {"viewport-width", std::string(128, '1')},
           {"width", std::string(127, '1')},
           {"accept-charset", "utf-8"},
           {"hogE", "fuga"}}),
      List({"hoge", "content-type", "accept", "accept-language",
            "content-language", "dpr", "device-memory", "save-data",
            "viewport-width", "width"}));

  // `safelistValueSize` is 897 because "content-type" is not safelisted.
  EXPECT_EQ(
      CorsUnsafeNotForbiddenRequestHeaderNames(
          {{"content-type", "text/plain; charset=" + std::string(128, '1')},
           {"accept", std::string(128, '1')},
           {"accept-language", std::string(128, '1')},
           {"content-language", std::string(128, '1')},
           {"dpr", std::string(128, '1')},
           {"device-memory", std::string(128, '1')},
           {"save-data", "on"},
           {"viewport-width", std::string(128, '1')},
           {"width", std::string(127, '1')},
           {"accept-charset", "utf-8"},
           {"hogE", "fuga"}}),
      List({"content-type", "hoge"}));
}

TEST_F(CorsUtilTest, ConditionalAndCacheHeadersAreNotSafelisted) {
  using List = std::vector<std::string>;

  EXPECT_EQ(
      CorsUnsafeNotForbiddenRequestHeaderNames({{"If-MODifIED-since", "x"},
                                                {"iF-nONE-MATCh", "y"},
                                                {"CACHE-ContrOl", "z"}}),
      List({"if-modified-since", "if-none-match", "cache-control"}));
}

}  // namespace

}  // namespace network::cors
