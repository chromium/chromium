// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/host_patterns.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net::device_bound_sessions {

namespace {

TEST(HostPatterns, IsValidHostPattern) {
  EXPECT_FALSE(IsValidHostPattern(""));
  EXPECT_FALSE(IsValidHostPattern("a.*.test"));
  EXPECT_FALSE(IsValidHostPattern("*a.test"));

  EXPECT_TRUE(IsValidHostPattern("*"));
  EXPECT_TRUE(IsValidHostPattern("example.test"));
  EXPECT_TRUE(IsValidHostPattern("*.example.test"));
  EXPECT_TRUE(IsValidHostPattern("[1:abcd::3:4:ff]"));
}

TEST(HostPatterns, MatchesHostPattern) {
  EXPECT_TRUE(MatchesHostPattern("*", GURL("https://example.test").GetHost()));
  EXPECT_TRUE(MatchesHostPattern("example.test",
                                 GURL("https://example.test").GetHost()));
  EXPECT_FALSE(MatchesHostPattern("*.example.test",
                                  GURL("https://example.test").GetHost()));
  EXPECT_TRUE(MatchesHostPattern(
      "*.example.test", GURL("https://subdomain.example.test").GetHost()));
  EXPECT_FALSE(MatchesHostPattern("notexample.test",
                                  GURL("https://example.test/").GetHost()));
  EXPECT_TRUE(MatchesHostPattern("[1:abcd::3:4:ff]",
                                 GURL("https://[1:abcd::3:4:ff]/").GetHost()));
}

}  // namespace

}  // namespace net::device_bound_sessions
