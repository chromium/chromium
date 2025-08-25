// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_loader_helpers.h"

#include "content/public/common/content_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace service_worker_loader_helpers {

namespace {

bool IsPathRestrictionSatisfied(const GURL& scope, const GURL& script_url) {
  std::string error_message;
  return service_worker_loader_helpers::IsPathRestrictionSatisfied(
      scope, script_url, std::nullopt, &error_message);
}

bool IsPathRestrictionSatisfiedWithServiceWorkerAllowedHeader(
    const GURL& scope,
    const GURL& script_url,
    std::string_view service_worker_allowed) {
  std::string error_message;
  return service_worker_loader_helpers::IsPathRestrictionSatisfied(
      scope, script_url, service_worker_allowed, &error_message);
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

class ServiceWorkerLoaderHelpersSyntheticResponseTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  ServiceWorkerLoaderHelpersSyntheticResponseTest() = default;
  ServiceWorkerLoaderHelpersSyntheticResponseTest(
      const ServiceWorkerLoaderHelpersSyntheticResponseTest&) = delete;
  ServiceWorkerLoaderHelpersSyntheticResponseTest& operator=(
      const ServiceWorkerLoaderHelpersSyntheticResponseTest&) = delete;
  ~ServiceWorkerLoaderHelpersSyntheticResponseTest() override = default;

  void SetUp() override {
    browser_client_ = std::make_unique<SyntheticResponseTestBrowserClient>(
        IsAllowedInContentBrowserClient());
    browser_context_ = std::make_unique<TestBrowserContext>();
    SetBrowserClientForTesting(browser_client_.get());
  }

 protected:
  bool IsEligibleForSyntheticResponse(
      const GURL& client_url,
      const std::string& allowed_url,
      const std::string& denied_url_params = "") {
    return service_worker_loader_helpers::
        IsEligibleForSyntheticResponseForTesting(
            browser_context_.get(), client_url, allowed_url, denied_url_params);
  }
  bool IsAllowedInContentBrowserClient() { return GetParam(); }

 private:
  class SyntheticResponseTestBrowserClient : public TestContentBrowserClient {
   public:
    explicit SyntheticResponseTestBrowserClient(bool is_allowed)
        : is_allowed_(is_allowed) {}

    bool IsServiceWorkerSyntheticResponseAllowed(
        content::BrowserContext* browser_context,
        const GURL& url) override {
      return is_allowed_;
    }

   private:
    bool is_allowed_ = false;
  };

  BrowserTaskEnvironment task_environment_{BrowserTaskEnvironment::IO_MAINLOOP};
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<SyntheticResponseTestBrowserClient> browser_client_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ServiceWorkerLoaderHelpersSyntheticResponseTest,
                         testing::Bool());

TEST_P(ServiceWorkerLoaderHelpersSyntheticResponseTest,
       IsEligibleForSyntheticResponse_AllowList) {
  if (IsAllowedInContentBrowserClient()) {
    // The mock content browser client is used and it always returns true for
    // the eligibility. This test ensure the behavior when the eligibility is
    // checked with the given allowlist.
  } else {
    EXPECT_TRUE(IsEligibleForSyntheticResponse(GURL("http://example.com/"),
                                               "http://example.com/"));
    EXPECT_TRUE(IsEligibleForSyntheticResponse(GURL("http://example.com/foo/"),
                                               "http://example.com/foo/"));
    // Currently trailing slash is not accepted. Will consider accepting once we
    // have a use case.
    EXPECT_FALSE(IsEligibleForSyntheticResponse(GURL("http://example.com/foo/"),
                                                "http://example.com/foo"));
    EXPECT_FALSE(IsEligibleForSyntheticResponse(GURL("http://example.com/"),
                                                "http://example.com/foo"));
    EXPECT_FALSE(IsEligibleForSyntheticResponse(GURL("http://example.com/foo"),
                                                "http://example.com/bar"));
    // With query params.
    EXPECT_TRUE(
        IsEligibleForSyntheticResponse(GURL("http://example.com/foo?param=bar"),
                                       "http://example.com/foo?param="));
    EXPECT_FALSE(
        IsEligibleForSyntheticResponse(GURL("http://example.com/foo?param="),
                                       "http://example.com/foo?param=bar"));
    EXPECT_FALSE(
        IsEligibleForSyntheticResponse(GURL("http://example.com/foo?param=bar"),
                                       "http://example.com/foo?param=baz"));
    EXPECT_FALSE(IsEligibleForSyntheticResponse(
        GURL("http://example.com/foo?param=test"),
        "http://example.com/bar?param=test"));
    // Empty string is not allowed.
    EXPECT_FALSE(
        IsEligibleForSyntheticResponse(GURL("http://example.com/"), ""));
  }
}

TEST_P(ServiceWorkerLoaderHelpersSyntheticResponseTest,
       IsEligibleForSyntheticResponse_DeniedUrlParams) {
  // Test with a simple URL.
  EXPECT_FALSE(IsEligibleForSyntheticResponse(
      GURL("http://example.com/?foo=bar"), "http://example.com/", "foo"));
  EXPECT_TRUE(IsEligibleForSyntheticResponse(
      GURL("http://example.com/?foo=bar"), "http://example.com/", "bar"));
  EXPECT_FALSE(IsEligibleForSyntheticResponse(
      GURL("http://example.com/?foo=bar&bar=baz"), "http://example.com/",
      "bar"));
  EXPECT_FALSE(IsEligibleForSyntheticResponse(
      GURL("http://example.com/?foo=bar&bar=baz"), "http://example.com/",
      "foo,bar"));
  EXPECT_TRUE(IsEligibleForSyntheticResponse(
      GURL("http://example.com/?foo=bar&bar=baz"), "http://example.com/",
      "baz"));

  // Test with a more specific URL and query.
  const char kAllowedUrl[] = "https://example.com/search?q=";
  const char kDeniedUrlParams[] = "foo,bar";

  // Allowed URL, but denied param "foo" is present.
  EXPECT_FALSE(IsEligibleForSyntheticResponse(
      GURL("https://example.com/search?q=test&foo=1"), kAllowedUrl,
      kDeniedUrlParams));

  // Allowed URL, but denied param "bar" is present.
  EXPECT_FALSE(IsEligibleForSyntheticResponse(
      GURL("https://example.com/search?q=test&bar=2"), kAllowedUrl,
      kDeniedUrlParams));

  // Allowed URL, and no denied params are present.
  EXPECT_TRUE(IsEligibleForSyntheticResponse(
      GURL("https://example.com/search?q=test&baz=3"), kAllowedUrl,
      kDeniedUrlParams));

  // Allowed URL, with a query parameter that is a substring of a denied
  // parameter.
  EXPECT_TRUE(IsEligibleForSyntheticResponse(
      GURL("https://example.com/search?q=test&fo=4"), kAllowedUrl,
      kDeniedUrlParams));

  // The URL not in the allowlist. No denied params are present.
  bool url_not_in_allowlist = IsEligibleForSyntheticResponse(
      GURL("https://example.com/other?q=test&baz=3"), kAllowedUrl,
      kDeniedUrlParams);
  if (IsAllowedInContentBrowserClient()) {
    // If the URL is allowed by the content browser client, eligible.
    EXPECT_TRUE(url_not_in_allowlist);
  } else {
    EXPECT_FALSE(url_not_in_allowlist);
  }

  // Denied string is in the value of "q". This should be allowed.
  EXPECT_TRUE(IsEligibleForSyntheticResponse(
      GURL("https://example.com/search?q=foo"), kAllowedUrl, kDeniedUrlParams));
}

}  // namespace service_worker_loader_helpers

}  // namespace content
