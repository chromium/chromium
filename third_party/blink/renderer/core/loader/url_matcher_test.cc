// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/url_matcher.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(UrlMatcherTest, SingleDomain) {
  UrlMatcher matcher("https://test.com");
  EXPECT_TRUE(matcher.Match(KURL("https://test.com/script.js")));
  EXPECT_FALSE(matcher.Match(KURL("http://test.com/script.js")));
  EXPECT_FALSE(matcher.Match(KURL("http://another.test.com/script.js")));
}

TEST(UrlMatcherTest, MultipleDomains) {
  UrlMatcher matcher("https://test.com,https://another.test.com");
  KURL url = KURL("https://test.com/script.js");
  EXPECT_TRUE(matcher.Match(url));
}

TEST(UrlMatcherTest, WithSeparatorForPathStrings) {
  UrlMatcher matcher("https://test.com|/foo");
  EXPECT_TRUE(matcher.Match(KURL("https://test.com/foo")));
  EXPECT_FALSE(matcher.Match(KURL("https://test.com/bar")));
  EXPECT_FALSE(matcher.Match(KURL("https://test.com?foo")));
}

TEST(UrlMatcherTest, WithSeparatorForQueryParams) {
  UrlMatcher matcher("https://test.com|foo=bar");
  EXPECT_FALSE(matcher.Match(KURL("https://test.com/foo")));
  EXPECT_FALSE(matcher.Match(KURL("https://test.com/foo/bar")));
  EXPECT_TRUE(matcher.Match(KURL("https://test.com?foo=bar")));
  EXPECT_TRUE(matcher.Match(KURL("https://test.com?a=b&foo=bar")));
}
}  // namespace blink
