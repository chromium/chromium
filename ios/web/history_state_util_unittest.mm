// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/history_state_util.h"

#import <array>
#import <string_view>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace web {
namespace {
using HistoryStateUtilTest = PlatformTest;

struct TestEntry {
  std::string_view fromUrl;
  std::string_view toUrl;
  std::string_view expectedUrl;
};

constexpr auto kTests = std::to_array<TestEntry>({
    // Valid absolute changes.
    {"http://foo.com", "http://foo.com/bar", "http://foo.com/bar"},
    {"https://foo.com", "https://foo.com/bar", "https://foo.com/bar"},
    {"http://foo.com/", "http://foo.com#bar", "http://foo.com#bar"},
    {"http://foo.com:80", "http://foo.com:80/b", "http://foo.com:80/b"},
    {"http://foo.com:888", "http://foo.com:888/b", "http://foo.com:888/b"},
    // Valid relative changes.
    {"http://foo.com", "#bar", "http://foo.com#bar"},
    {"http://foo.com/", "#bar", "http://foo.com/#bar"},
    {"https://foo.com/", "bar", "https://foo.com/bar"},
    {"http://foo.com/foo/1", "/bar", "http://foo.com/bar"},
    {"http://foo.com/foo/1", "bar", "http://foo.com/foo/bar"},
    {"http://foo.com/", "bar.com", "http://foo.com/bar.com"},
    {"http://foo.com", "bar.com", "http://foo.com/bar.com"},
    {"http://foo.com:888", "bar.com", "http://foo.com:888/bar.com"},
    // Invalid scheme changes.
    {"http://foo.com", "https://foo.com#bar", ""},
    {"https://foo.com", "http://foo.com#bar", ""},
    // Invalid domain changes.
    {"http://foo.com/bar", "http://bar.com", ""},
    {"http://foo.com/bar", "http://www.foo.com/bar2", ""},
    // Valid port change.
    {"http://foo.com", "http://foo.com:80/bar", "http://foo.com/bar"},
    {"http://foo.com:80", "http://foo.com/bar", "http://foo.com/bar"},
    // Invalid port change.
    {"http://foo.com", "http://foo.com:42/bar", ""},
    {"http://foo.com:42", "http://foo.com/bar", ""},
    // Invalid URL.
    {"http://foo.com", "http://fo o.c om/ba r", ""},
    {"http://foo.com:80", "bar", "http://foo.com:80/bar"},
    // Valid blob URL changes (same inner origin and scheme).
    {"blob:http://foo.com/1", "blob:http://foo.com/2", "blob:http://foo.com/2"},
    {"blob:https://foo.com/1", "blob:https://foo.com/2",
     "blob:https://foo.com/2"},
    // Invalid cross-scheme transitions with blob and filesystem URLs.
    {"http://foo.com/bar", "blob:http://foo.com/1", ""},
    {"blob:http://foo.com/1", "http://foo.com/bar", ""},
    {"https://foo.com/bar", "blob:https://foo.com/1", ""},
    {"blob:https://foo.com/1", "https://foo.com/bar", ""},
    {"http://foo.com/bar", "filesystem:http://foo.com/temporary/1", ""},
    {"filesystem:http://foo.com/temporary/1", "http://foo.com/bar", ""},
    // Invalid blob URL changes (cross inner origin or scheme).
    {"blob:http://foo.com/1", "blob:http://bar.com/2", ""},
    {"blob:http://foo.com/1", "blob:https://foo.com/2", ""},
    {"blob:http://foo.com:8080/1", "blob:https://foo.com/signin", ""},
    {"blob:http://foo.com:8080/1", "blob:http://foo.com:8081/2", ""},
    // Invalid non-standard / opaque origin URL changes.
    {"data:text/html,foo", "data:text/html,bar", ""},
    {"about:blank", "about:blank", ""},
    {"http://foo.com", "data:text/html,foo", ""},
    {"data:text/html,foo", "http://foo.com", ""},
    {"blob:null/1", "blob:null/2", ""},
});

TEST_F(HistoryStateUtilTest, TestIsHistoryStateChangeValid) {
  for (const TestEntry& test : kTests) {
    GURL fromUrl(test.fromUrl);
    GURL toUrl = history_state_util::GetHistoryStateChangeUrl(fromUrl, fromUrl,
                                                              test.toUrl);
    bool expected_result = test.expectedUrl.size() > 0;
    bool actual_result = toUrl.is_valid();
    if (actual_result) {
      actual_result =
          history_state_util::IsHistoryStateChangeValid(fromUrl, toUrl);
    }
    EXPECT_EQ(expected_result, actual_result)
        << test.fromUrl << " " << test.toUrl;
  }
}

TEST_F(HistoryStateUtilTest, TestGetHistoryStateChangeUrl) {
  for (const TestEntry& test : kTests) {
    GURL fromUrl(test.fromUrl);
    GURL expectedResult(test.expectedUrl);
    GURL actualResult = history_state_util::GetHistoryStateChangeUrl(
        fromUrl, fromUrl, test.toUrl);
    EXPECT_EQ(expectedResult, actualResult);
  }
}

// Ensures that the baseUrl is used to resolve the destination, not currentUrl.
TEST_F(HistoryStateUtilTest, TestGetHistoryStateChangeUrlWithBase) {
  GURL fromUrl("http://foo.com/relative/path");
  GURL baseUrl("http://foo.com");
  std::string destination = "bar";

  GURL result = history_state_util::GetHistoryStateChangeUrl(fromUrl, baseUrl,
                                                             destination);
  EXPECT_TRUE(result.is_valid());
  EXPECT_EQ(GURL("http://foo.com/bar"), result);
}

// Ensures that an invalid baseUrl gracefully returns an invalid destination.
TEST_F(HistoryStateUtilTest, TestGetHistoryStateChangeUrlWithInvalidBase) {
  GURL fromUrl("http://foo.com");
  GURL baseUrl("http://not a url");
  std::string destination = "baz";

  GURL result = history_state_util::GetHistoryStateChangeUrl(fromUrl, baseUrl,
                                                             destination);
  EXPECT_FALSE(result.is_valid());
}

// Ensures that an invalid currentURL gracefully returns an invalid destination.
TEST_F(HistoryStateUtilTest, TestGetHistoryStateChangeUrlWithInvalidUrl) {
  GURL fromUrl("http://not a url");
  GURL baseUrl("http://foo.com");
  std::string destination = "baz";

  GURL result = history_state_util::GetHistoryStateChangeUrl(fromUrl, baseUrl,
                                                             destination);
  EXPECT_FALSE(result.is_valid());
}

// Tests that blob URLs with the same inner origin are allowed, while blob URLs
// with different inner origins (different host, scheme, or port) are rejected.
TEST_F(HistoryStateUtilTest, TestBlobUrlHistoryStateChange) {
  // Same-origin blob URLs.
  GURL http_blob1("blob:http://foo.com/1");
  GURL http_blob2("blob:http://foo.com/2");
  EXPECT_TRUE(
      history_state_util::IsHistoryStateChangeValid(http_blob1, http_blob2));

  GURL https_blob1("blob:https://foo.com/1");
  GURL https_blob2("blob:https://foo.com/2");
  EXPECT_TRUE(
      history_state_util::IsHistoryStateChangeValid(https_blob1, https_blob2));

  // Cross-origin blob URLs (different host, scheme, or port).
  GURL diff_host_blob("blob:http://bar.com/2");
  EXPECT_FALSE(history_state_util::IsHistoryStateChangeValid(http_blob1,
                                                             diff_host_blob));
  EXPECT_FALSE(
      history_state_util::IsHistoryStateChangeValid(http_blob1, https_blob1));

  GURL diff_port_blob("blob:http://foo.com:8080/1");
  EXPECT_FALSE(history_state_util::IsHistoryStateChangeValid(http_blob1,
                                                             diff_port_blob));

  GURL other_target_blob("blob:https://foo.com/signin");
  EXPECT_FALSE(history_state_util::IsHistoryStateChangeValid(
      diff_port_blob, other_target_blob));

  // Standard URL to blob and vice versa are rejected because schemes differ.
  GURL http_standard("http://foo.com/bar");
  EXPECT_FALSE(
      history_state_util::IsHistoryStateChangeValid(http_standard, http_blob1));
  EXPECT_FALSE(
      history_state_util::IsHistoryStateChangeValid(http_blob1, http_standard));

  GURL https_standard("https://foo.com/bar");
  EXPECT_FALSE(history_state_util::IsHistoryStateChangeValid(https_standard,
                                                             https_blob1));
  EXPECT_FALSE(history_state_util::IsHistoryStateChangeValid(https_blob1,
                                                             https_standard));

  EXPECT_FALSE(history_state_util::IsHistoryStateChangeValid(
      http_standard, other_target_blob));
  EXPECT_FALSE(history_state_util::IsHistoryStateChangeValid(http_blob1,
                                                             https_standard));
}

// Tests that URLs with opaque origins (data:, about:blank, blob:null/...) are
// rejected as valid history state changes.
TEST_F(HistoryStateUtilTest, TestOpaqueOriginHistoryStateChange) {
  GURL data_url1("data:text/html,<html>one</html>");
  GURL data_url2("data:text/html,<html>two</html>");
  EXPECT_FALSE(
      history_state_util::IsHistoryStateChangeValid(data_url1, data_url2));

  GURL about_blank("about:blank");
  EXPECT_FALSE(
      history_state_util::IsHistoryStateChangeValid(about_blank, about_blank));

  GURL opaque_blob1("blob:null/1");
  GURL opaque_blob2("blob:null/2");
  EXPECT_FALSE(history_state_util::IsHistoryStateChangeValid(opaque_blob1,
                                                             opaque_blob2));

  GURL http_url("http://foo.com/bar");
  EXPECT_FALSE(
      history_state_util::IsHistoryStateChangeValid(http_url, data_url1));
  EXPECT_FALSE(
      history_state_util::IsHistoryStateChangeValid(data_url1, http_url));
}

// Tests that filesystem URLs with the same inner origin are allowed, while
// cross-origin filesystem URLs are rejected.
TEST_F(HistoryStateUtilTest, TestFileSystemUrlHistoryStateChange) {
  GURL fs_url1("filesystem:http://foo.com/temporary/1");
  GURL fs_url2("filesystem:http://foo.com/temporary/2");
  EXPECT_TRUE(history_state_util::IsHistoryStateChangeValid(fs_url1, fs_url2));

  // Cross-scheme transitions between standard http and filesystem are rejected.
  GURL http_url("http://foo.com/bar");
  EXPECT_FALSE(
      history_state_util::IsHistoryStateChangeValid(http_url, fs_url1));
  EXPECT_FALSE(
      history_state_util::IsHistoryStateChangeValid(fs_url1, http_url));

  GURL diff_host_fs("filesystem:http://bar.com/temporary/1");
  EXPECT_FALSE(
      history_state_util::IsHistoryStateChangeValid(fs_url1, diff_host_fs));

  GURL diff_scheme_fs("filesystem:https://foo.com/temporary/1");
  EXPECT_FALSE(
      history_state_util::IsHistoryStateChangeValid(fs_url1, diff_scheme_fs));
}

}  // anonymous namespace
}  // namespace web
