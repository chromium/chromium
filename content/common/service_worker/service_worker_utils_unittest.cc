// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_utils.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "net/base/load_flags.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::FetchCacheMode;

namespace content {

namespace {

bool IsPathRestrictionSatisfied(const GURL& scope, const GURL& script_url) {
  std::string error_message;
  return ServiceWorkerUtils::IsPathRestrictionSatisfied(
      scope, script_url, nullptr, &error_message);
}

bool IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
    const GURL& scope,
    const GURL& script_url,
    const std::string& service_worker_allowed) {
  std::string error_message;
  return ServiceWorkerUtils::IsPathRestrictionSatisfied(
      scope, script_url, &service_worker_allowed, &error_message);
}

}  // namespace

TEST(ServiceWorkerUtilsTest, ScopeMatches) {
  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/"), GURL("http://www.example.com/")));
  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/"),
      GURL("http://www.example.com/page.html")));

  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/"), GURL("https://www.example.com/")));
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/"),
      GURL("https://www.example.com/page.html")));
  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/"), GURL("http://www.example.com/#a")));

  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(GURL("http://www.example.com/"),
                                                GURL("http://www.foo.com/")));
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/"), GURL("https://www.foo.com/page.html")));

  // '*' is not a wildcard.
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*"), GURL("http://www.example.com/x")));
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*"), GURL("http://www.example.com/")));
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*"), GURL("http://www.example.com/xx")));
  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*"), GURL("http://www.example.com/*")));

  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*/x"), GURL("http://www.example.com/*/x")));
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/*/x"), GURL("http://www.example.com/a/x")));
  ASSERT_FALSE(
      ServiceWorkerUtils::ScopeMatches(GURL("http://www.example.com/*/x/*"),
                                       GURL("http://www.example.com/a/x/b")));
  ASSERT_FALSE(
      ServiceWorkerUtils::ScopeMatches(GURL("http://www.example.com/*/x/*"),
                                       GURL("http://www.example.com/*/x/b")));

  // '?' is not a wildcard.
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/?"), GURL("http://www.example.com/x")));
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/?"), GURL("http://www.example.com/")));
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/?"), GURL("http://www.example.com/xx")));
  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/?"), GURL("http://www.example.com/?")));

  // Query string is part of the resource.
  ASSERT_TRUE(
      ServiceWorkerUtils::ScopeMatches(GURL("http://www.example.com/?a=b"),
                                       GURL("http://www.example.com/?a=b")));
  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/?a="), GURL("http://www.example.com/?a=b")));
  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/"), GURL("http://www.example.com/?a=b")));

  // URLs canonicalize \ to / so this is equivalent to "...//x"
  ASSERT_TRUE(ServiceWorkerUtils::ScopeMatches(
      GURL("http://www.example.com/\\x"), GURL("http://www.example.com//x")));

  // URLs that are in different origin shouldn't match.
  ASSERT_FALSE(ServiceWorkerUtils::ScopeMatches(
      GURL("https://evil.com"), GURL("https://evil.com.example.com")));
}

TEST(ServiceWorkerUtilsTest, FindLongestScopeMatch) {
  LongestScopeMatcher matcher(GURL("http://www.example.com/xxx"));

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

TEST(ServiceWorkerUtilsTest, PathRestriction_Basic) {
  EXPECT_TRUE(IsPathRestrictionSatisfied(GURL("http://example.com/"),
                                         GURL("http://example.com/sw.js")));
  EXPECT_TRUE(IsPathRestrictionSatisfied(GURL("http://example.com/foo/"),
                                         GURL("http://example.com/sw.js")));
  EXPECT_TRUE(IsPathRestrictionSatisfied(GURL("http://example.com/foo/"),
                                         GURL("http://example.com/foo/sw.js")));
  EXPECT_TRUE(IsPathRestrictionSatisfied(GURL("http://example.com/foo/bar"),
                                         GURL("http://example.com/foo/sw.js")));
  EXPECT_FALSE(IsPathRestrictionSatisfied(
      GURL("http://example.com/"), GURL("http://example.com/foo/sw.js")));
  EXPECT_FALSE(IsPathRestrictionSatisfied(
      GURL("http://example.com/bar/"), GURL("http://example.com/foo/sw.js")));

  // The scope is under the script directory, but that doesn't have the trailing
  // slash. In this case, the check should be failed.
  EXPECT_FALSE(IsPathRestrictionSatisfied(
      GURL("http://example.com/foo"), GURL("http://example.com/foo/sw.js")));

  // Query parameters should not affect the path restriction.
  EXPECT_TRUE(IsPathRestrictionSatisfied(GURL("http://example.com/?query"),
                                         GURL("http://example.com/sw.js")));
  EXPECT_TRUE(IsPathRestrictionSatisfied(GURL("http://example.com/foo/?query"),
                                         GURL("http://example.com/sw.js")));
  EXPECT_TRUE(IsPathRestrictionSatisfied(GURL("http://example.com/foo/?query"),
                                         GURL("http://example.com/foo/sw.js")));
  EXPECT_FALSE(IsPathRestrictionSatisfied(
      GURL("http://example.com/?query"), GURL("http://example.com/foo/sw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/bar/query?"),
                                 GURL("http://example.com/foo/sw.js")));

  EXPECT_TRUE(IsPathRestrictionSatisfied(
      GURL("http://example.com/"), GURL("http://example.com/sw.js?query")));
  EXPECT_TRUE(IsPathRestrictionSatisfied(
      GURL("http://example.com/foo/"), GURL("http://example.com/sw.js?query")));
  EXPECT_TRUE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/"),
                                 GURL("http://example.com/foo/sw.js?query")));
  EXPECT_FALSE(IsPathRestrictionSatisfied(
      GURL("http://example.com/"), GURL("http://example.com/foo/sw.js?query")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/bar/"),
                                 GURL("http://example.com/foo/sw.js?query")));

  // Query parameter including a slash should not affect.
  EXPECT_TRUE(IsPathRestrictionSatisfied(
      GURL("http://example.com/foo/bar?key/value"),
      GURL("http://example.com/foo/sw.js?key=value")));
  EXPECT_TRUE(IsPathRestrictionSatisfied(
      GURL("http://example.com/foo/bar?key=value"),
      GURL("http://example.com/foo/sw.js?key/value")));
}

TEST(ServiceWorkerUtilsTest, PathRestriction_SelfReference) {
  // Self reference is canonicalized.
  ASSERT_EQ(GURL("http://example.com/foo/bar"),
            GURL("http://example.com/././foo/bar"));
  EXPECT_TRUE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/"),
                                 GURL("http://example.com/././foo/sw.js")));
  EXPECT_TRUE(IsPathRestrictionSatisfied(GURL("http://example.com/././foo/"),
                                         GURL("http://example.com/foo/sw.js")));
  EXPECT_TRUE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/./"),
                                 GURL("http://example.com/./foo/sw.js")));

  // URL-encoded dot ('%2e') is also canonicalized.
  ASSERT_EQ(GURL("http://example.com/foo/././bar"),
            GURL("http://example.com/foo/%2e/%2e/bar"));
  EXPECT_TRUE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/%2e/%2e/bar"),
                                 GURL("http://example.com/foo/%2e/%2e/sw.js")));
  EXPECT_TRUE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/bar"),
                                 GURL("http://example.com/foo/%2e/%2e/sw.js")));
  EXPECT_TRUE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/%2e/%2e/bar"),
                                 GURL("http://example.com/foo/sw.js")));
  EXPECT_TRUE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/%2e/bar"),
                                 GURL("http://example.com/%2e/foo/sw.js")));

  // URL-encoded dot ('%2E') is also canonicalized.
  ASSERT_EQ(GURL("http://example.com/foo/././bar"),
            GURL("http://example.com/foo/%2E/%2e/bar"));
}

TEST(ServiceWorkerUtilsTest, PathRestriction_ParentReference) {
  // Parent reference is canonicalized.
  ASSERT_EQ(GURL("http://example.com/foo/bar"),
            GURL("http://example.com/foo/../foo/bar"));
  EXPECT_TRUE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/bar"),
                                 GURL("http://example.com/foo/../foo/sw.js")));
  EXPECT_TRUE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/../foo/bar"),
                                 GURL("http://example.com/foo/sw.js")));
  EXPECT_TRUE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/bar/../bar"),
                                 GURL("http://example.com/../foo/sw.js")));
  EXPECT_TRUE(IsPathRestrictionSatisfied(
      GURL("http://example.com/foo/../../../foo/bar"),
      GURL("http://example.com/foo/sw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/../bar"),
                                 GURL("http://example.com/foo/sw.js")));

  // URL-encoded dot ('%2e') is also canonicalized.
  ASSERT_EQ(GURL("http://example.com/foo/../foo/bar"),
            GURL("http://example.com/foo/%2e%2e/foo/bar"));
  EXPECT_TRUE(IsPathRestrictionSatisfied(
      GURL("http://example.com/foo/bar"),
      GURL("http://example.com/foo/%2e%2e/foo/sw.js")));
  EXPECT_TRUE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/%2e%2e/foo/bar"),
                                 GURL("http://example.com/foo/sw.js")));
  EXPECT_TRUE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/%2e%2e/foo/bar"),
                                 GURL("http://example.com/%2e%2e/foo/sw.js")));
  EXPECT_TRUE(IsPathRestrictionSatisfied(
      GURL("http://example.com/foo/%2e%2e/%2e%2e/%2e%2e/foo/bar"),
      GURL("http://example.com/foo/sw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/%2e%2e/bar"),
                                 GURL("http://example.com/foo/sw.js")));

  // URL-encoded dot ('%2E') is also canonicalized.
  ASSERT_EQ(GURL("http://example.com/foo/../foo/bar"),
            GURL("http://example.com/foo/%2E%2E/foo/bar"));
}

TEST(ServiceWorkerUtilsTest, PathRestriction_ConsecutiveSlashes) {
  // Consecutive slashes are not unified.
  ASSERT_NE(GURL("http://example.com/foo/bar"),
            GURL("http://example.com/foo///bar"));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/bar"),
                                 GURL("http://example.com/foo///sw.js")));
  EXPECT_TRUE(IsPathRestrictionSatisfied(GURL("http://example.com/foo///bar"),
                                         GURL("http://example.com/foo/sw.js")));
  EXPECT_TRUE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo///bar"),
                                 GURL("http://example.com/foo///sw.js")));
}

TEST(ServiceWorkerUtilsTest, PathRestriction_BackSlash) {
  // A backslash is converted to a slash.
  ASSERT_EQ(GURL("http://example.com/foo/bar"),
            GURL("http://example.com/foo\\bar"));
  EXPECT_TRUE(IsPathRestrictionSatisfied(GURL("http://example.com/foo\\bar"),
                                         GURL("http://example.com/foo/sw.js")));

  // Consecutive back slashes should not unified.
  ASSERT_NE(GURL("http://example.com/foo\\\\\\bar"),
            GURL("http://example.com/foo\\bar"));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo\\bar"),
                                 GURL("http://example.com/foo\\\\\\sw.js")));
  EXPECT_TRUE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo\\\\\\bar"),
                                 GURL("http://example.com/foo\\sw.js")));
}

TEST(ServiceWorkerUtilsTest, PathRestriction_DisallowedCharacter) {
  // URL-encoded slash ('%2f') is not canonicalized.
  ASSERT_NE(GURL("http://example.com/foo///bar"),
            GURL("http://example.com/foo/%2f/bar"));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo%2fbar/"),
                                 GURL("http://example.com/foo/bar/sw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/bar%2f"),
                                 GURL("http://example.com/foo/bar/sw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/bar/"),
                                 GURL("http://example.com/foo/bar%2fsw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/"),
                                 GURL("http://example.com/foo/bar%2fsw.js")));

  // URL-encoded slash ('%2F') is also not canonicalized.
  ASSERT_NE(GURL("http://example.com/foo///bar"),
            GURL("http://example.com/foo/%2F/bar"));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo%2Fbar/"),
                                 GURL("http://example.com/foo/bar/sw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/bar%2F"),
                                 GURL("http://example.com/foo/bar/sw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/bar/"),
                                 GURL("http://example.com/foo/bar%2Fsw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/"),
                                 GURL("http://example.com/foo/bar%2Fsw.js")));

  // URL-encoded backslash ('%5c') is not canonicalized.
  ASSERT_NE(GURL("http://example.com/foo/\\/bar"),
            GURL("http://example.com/foo/%5c/bar"));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo%5cbar/"),
                                 GURL("http://example.com/foo/bar/sw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/bar%5c"),
                                 GURL("http://example.com/foo/bar/sw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/bar/"),
                                 GURL("http://example.com/foo/bar%5csw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/"),
                                 GURL("http://example.com/foo/bar%5csw.js")));

  // URL-encoded backslash ('%5C') is also not canonicalized.
  ASSERT_NE(GURL("http://example.com/foo/\\/bar"),
            GURL("http://example.com/foo/%5C/bar"));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo%5Cbar/"),
                                 GURL("http://example.com/foo/bar/sw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/bar%5C"),
                                 GURL("http://example.com/foo/bar/sw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/bar/"),
                                 GURL("http://example.com/foo/bar%5Csw.js")));
  EXPECT_FALSE(
      IsPathRestrictionSatisfied(GURL("http://example.com/foo/"),
                                 GURL("http://example.com/foo/bar%5Csw.js")));

  // Query parameter should be able to have escaped characters.
  EXPECT_TRUE(IsPathRestrictionSatisfied(
      GURL("http://example.com/foo/bar?key%2fvalue"),
      GURL("http://example.com/foo/sw.js?key/value")));
  EXPECT_TRUE(IsPathRestrictionSatisfied(
      GURL("http://example.com/foo/bar?key/value"),
      GURL("http://example.com/foo/sw.js?key%2fvalue")));
  EXPECT_TRUE(IsPathRestrictionSatisfied(
      GURL("http://example.com/foo/bar?key%5cvalue"),
      GURL("http://example.com/foo/sw.js?key\\value")));
  EXPECT_TRUE(IsPathRestrictionSatisfied(
      GURL("http://example.com/foo/bar?key\\value"),
      GURL("http://example.com/foo/sw.js?key%5cvalue")));
}

TEST(ServiceWorkerUtilsTest, PathRestriction_ServiceWorkerAllowed) {
  // Setting header to default max scope changes nothing.
  EXPECT_TRUE(IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
      GURL("http://example.com/"), GURL("http://example.com/sw.js"),
      "http://example.com/"));
  EXPECT_FALSE(IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
      GURL("http://example.com/"), GURL("http://example.com/foo/sw.js"),
      "http://example.com/foo/"));

  // Using the header to widen allowed scope.
  EXPECT_TRUE(IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
      GURL("http://example.com/"), GURL("http://example.com/foo/sw.js"),
      "http://example.com/"));
  EXPECT_TRUE(IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
      GURL("http://example.com/"), GURL("http://example.com/foo/sw.js"), "/"));
  EXPECT_TRUE(IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
      GURL("http://example.com/"), GURL("http://example.com/foo/sw.js"), ".."));
  EXPECT_TRUE(IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
      GURL("http://example.com/bar/"), GURL("http://example.com/foo/sw.js"),
      "../b"));
  EXPECT_FALSE(IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
      GURL("http://example.com/bar/"), GURL("http://example.com/foo/sw.js"),
      "../c"));

  // Using the header to restrict allowed scope.
  EXPECT_FALSE(IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
      GURL("http://example.com/"), GURL("http://example.com/sw.js"),
      "http://example.com/foo/"));
  EXPECT_FALSE(IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
      GURL("http://example.com/"), GURL("http://example.com/sw.js"), "foo"));
  EXPECT_TRUE(IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
      GURL("http://example.com/foo/"), GURL("http://example.com/sw.js"),
      "foo"));

  // Empty string resolves to max scope of "http://www.example.com/sw.js".
  EXPECT_FALSE(IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
      GURL("http://example.com/"), GURL("http://example.com/sw.js"), ""));
  EXPECT_TRUE(IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
      GURL("http://example.com/sw.js/hi"), GURL("http://example.com/sw.js"),
      ""));
}

TEST(ServiceWorkerUtilsTest, AllOriginsMatchAndCanAccessServiceWorkers) {
  std::vector<GURL> https_same_origin = {GURL("https://example.com/1"),
                                         GURL("https://example.com/2"),
                                         GURL("https://example.com/3")};
  EXPECT_TRUE(ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
      https_same_origin));

  std::vector<GURL> http_same_origin = {GURL("http://example.com/1"),
                                        GURL("http://example.com/2")};
  EXPECT_FALSE(ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
      http_same_origin));

  std::vector<GURL> localhost_same_origin = {GURL("http://localhost/1"),
                                             GURL("http://localhost/2")};
  EXPECT_TRUE(ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
      localhost_same_origin));

  std::vector<GURL> filesystem_same_origin = {
      GURL("https://example.com/1"), GURL("https://example.com/2"),
      GURL("filesystem:https://example.com/3")};
  EXPECT_FALSE(ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
      filesystem_same_origin));

  std::vector<GURL> https_cross_origin = {GURL("https://example.com/1"),
                                          GURL("https://example.org/2"),
                                          GURL("https://example.com/3")};
  EXPECT_FALSE(ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
      https_cross_origin));

  // Cross-origin access is permitted with --disable-web-security.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kDisableWebSecurity);
  EXPECT_TRUE(ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
      https_cross_origin));

  // Disallowed schemes are not permitted even with --disable-web-security.
  EXPECT_FALSE(ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
      filesystem_same_origin));
}

TEST(ServiceWorkerFetchRequestTest, CacheModeTest) {
  EXPECT_EQ(FetchCacheMode::kDefault,
            ServiceWorkerUtils::GetCacheModeFromLoadFlags(0));
  EXPECT_EQ(
      FetchCacheMode::kNoStore,
      ServiceWorkerUtils::GetCacheModeFromLoadFlags(net::LOAD_DISABLE_CACHE));
  EXPECT_EQ(
      FetchCacheMode::kValidateCache,
      ServiceWorkerUtils::GetCacheModeFromLoadFlags(net::LOAD_VALIDATE_CACHE));
  EXPECT_EQ(
      FetchCacheMode::kBypassCache,
      ServiceWorkerUtils::GetCacheModeFromLoadFlags(net::LOAD_BYPASS_CACHE));
  EXPECT_EQ(FetchCacheMode::kForceCache,
            ServiceWorkerUtils::GetCacheModeFromLoadFlags(
                net::LOAD_SKIP_CACHE_VALIDATION));
  EXPECT_EQ(FetchCacheMode::kOnlyIfCached,
            ServiceWorkerUtils::GetCacheModeFromLoadFlags(
                net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION));
  EXPECT_EQ(
      FetchCacheMode::kUnspecifiedOnlyIfCachedStrict,
      ServiceWorkerUtils::GetCacheModeFromLoadFlags(net::LOAD_ONLY_FROM_CACHE));
  EXPECT_EQ(FetchCacheMode::kUnspecifiedForceCacheMiss,
            ServiceWorkerUtils::GetCacheModeFromLoadFlags(
                net::LOAD_ONLY_FROM_CACHE | net::LOAD_BYPASS_CACHE));
}

}  // namespace content
