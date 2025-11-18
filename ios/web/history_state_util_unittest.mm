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

}  // anonymous namespace
}  // namespace web
