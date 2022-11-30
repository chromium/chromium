// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(ServiceWorkerScopeMatchTest, ScopeMatches) {
  ASSERT_TRUE(ServiceWorkerScopeMatches(GURL("http://www.example.com/"),
                                        GURL("http://www.example.com/")));
  ASSERT_TRUE(
      ServiceWorkerScopeMatches(GURL("http://www.example.com/"),
                                GURL("http://www.example.com/page.html")));

  ASSERT_FALSE(ServiceWorkerScopeMatches(GURL("http://www.example.com/"),
                                         GURL("https://www.example.com/")));
  ASSERT_FALSE(
      ServiceWorkerScopeMatches(GURL("http://www.example.com/"),
                                GURL("https://www.example.com/page.html")));
  ASSERT_TRUE(ServiceWorkerScopeMatches(GURL("http://www.example.com/"),
                                        GURL("http://www.example.com/#a")));

  ASSERT_FALSE(ServiceWorkerScopeMatches(GURL("http://www.example.com/"),
                                         GURL("http://www.foo.com/")));
  ASSERT_FALSE(ServiceWorkerScopeMatches(
      GURL("http://www.example.com/"), GURL("https://www.foo.com/page.html")));

  // '*' is not a wildcard.
  ASSERT_FALSE(ServiceWorkerScopeMatches(GURL("http://www.example.com/*"),
                                         GURL("http://www.example.com/x")));
  ASSERT_FALSE(ServiceWorkerScopeMatches(GURL("http://www.example.com/*"),
                                         GURL("http://www.example.com/")));
  ASSERT_FALSE(ServiceWorkerScopeMatches(GURL("http://www.example.com/*"),
                                         GURL("http://www.example.com/xx")));
  ASSERT_TRUE(ServiceWorkerScopeMatches(GURL("http://www.example.com/*"),
                                        GURL("http://www.example.com/*")));

  ASSERT_TRUE(ServiceWorkerScopeMatches(GURL("http://www.example.com/*/x"),
                                        GURL("http://www.example.com/*/x")));
  ASSERT_FALSE(ServiceWorkerScopeMatches(GURL("http://www.example.com/*/x"),
                                         GURL("http://www.example.com/a/x")));
  ASSERT_FALSE(ServiceWorkerScopeMatches(GURL("http://www.example.com/*/x/*"),
                                         GURL("http://www.example.com/a/x/b")));
  ASSERT_FALSE(ServiceWorkerScopeMatches(GURL("http://www.example.com/*/x/*"),
                                         GURL("http://www.example.com/*/x/b")));

  // '?' is not a wildcard.
  ASSERT_FALSE(ServiceWorkerScopeMatches(GURL("http://www.example.com/?"),
                                         GURL("http://www.example.com/x")));
  ASSERT_FALSE(ServiceWorkerScopeMatches(GURL("http://www.example.com/?"),
                                         GURL("http://www.example.com/")));
  ASSERT_FALSE(ServiceWorkerScopeMatches(GURL("http://www.example.com/?"),
                                         GURL("http://www.example.com/xx")));
  ASSERT_TRUE(ServiceWorkerScopeMatches(GURL("http://www.example.com/?"),
                                        GURL("http://www.example.com/?")));

  // Query string is part of the resource.
  ASSERT_TRUE(ServiceWorkerScopeMatches(GURL("http://www.example.com/?a=b"),
                                        GURL("http://www.example.com/?a=b")));
  ASSERT_TRUE(ServiceWorkerScopeMatches(GURL("http://www.example.com/?a="),
                                        GURL("http://www.example.com/?a=b")));
  ASSERT_TRUE(ServiceWorkerScopeMatches(GURL("http://www.example.com/"),
                                        GURL("http://www.example.com/?a=b")));

  // URLs canonicalize \ to / so this is equivalent to "...//x"
  ASSERT_TRUE(ServiceWorkerScopeMatches(GURL("http://www.example.com/\\x"),
                                        GURL("http://www.example.com//x")));

  // URLs that are in different origin shouldn't match.
  ASSERT_FALSE(ServiceWorkerScopeMatches(GURL("https://evil.com"),
                                         GURL("https://evil.com.example.com")));
}

TEST(ServiceWorkerScopeMatchTest, FindLongestScopeMatch) {
  ServiceWorkerLongestScopeMatcher matcher(GURL("http://www.example.com/xxx"));

  // "/xx" should be matched longest.
  ASSERT_TRUE(matcher.MatchLongest(GURL("http://www.example.com/x")));
  ASSERT_FALSE(matcher.MatchLongest(GURL("http://www.example.com/")));
  ASSERT_TRUE(matcher.MatchLongest(GURL("http://www.example.com/xx")));

  // "/xxx" should be matched longer than "/xx".
  ASSERT_TRUE(matcher.MatchLongest(GURL("http://www.example.com/xxx")));

  // The second call with the same URL should return false.
  ASSERT_FALSE(matcher.MatchLongest(GURL("http://www.example.com/xxx")));

  ASSERT_FALSE(matcher.MatchLongest(GURL("http://www.example.com/xxxx")));
}

}  // namespace blink
