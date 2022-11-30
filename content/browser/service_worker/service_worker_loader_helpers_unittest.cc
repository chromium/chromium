// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_loader_helpers.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace service_worker_loader_helpers {

namespace {

bool IsPathRestrictionSatisfied(const GURL& scope, const GURL& script_url) {
  std::string error_message;
  return service_worker_loader_helpers::IsPathRestrictionSatisfied(
      scope, script_url, nullptr, &error_message);
}

bool IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
    const GURL& scope,
    const GURL& script_url,
    const std::string& service_worker_allowed) {
  std::string error_message;
  return service_worker_loader_helpers::IsPathRestrictionSatisfied(
      scope, script_url, &service_worker_allowed, &error_message);
}

}  // namespace

TEST(ServiceWorkerLoaderHelpersTest, PathRestriction_Basic) {
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

TEST(ServiceWorkerLoaderHelpersTest, PathRestriction_SelfReference) {
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

TEST(ServiceWorkerLoaderHelpersTest, PathRestriction_ParentReference) {
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

TEST(ServiceWorkerLoaderHelpersTest, PathRestriction_ConsecutiveSlashes) {
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

TEST(ServiceWorkerLoaderHelpersTest, PathRestriction_BackSlash) {
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

TEST(ServiceWorkerLoaderHelpersTest, PathRestriction_DisallowedCharacter) {
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

TEST(ServiceWorkerLoaderHelpersTest, PathRestriction_ServiceWorkerAllowed) {
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

  // Cross origin header value is not accepted.
  EXPECT_FALSE(IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
      GURL("http://example.com/"), GURL("http://example.com/sw.js"),
      "http://other.com/"));
  EXPECT_FALSE(IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
      GURL("http://example.com/foo/"), GURL("http://example.com/bar/sw.js"),
      "http://other.com/foo/"));
}

}  // namespace service_worker_loader_helpers

}  // namespace content
