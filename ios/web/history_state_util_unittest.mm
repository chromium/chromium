// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/history_state_util.h"

#import <stddef.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace web {
namespace {
struct TestEntry {
  std::string fromUrl;
  std::string toUrl;
  std::string expectedUrl;
};

class HistoryStateUtilTest : public PlatformTest {
 protected:
  static const struct TestEntry tests_[];
};

const struct TestEntry HistoryStateUtilTest::tests_[] = {
    // Valid absolute changes.
    { "http://foo.com", "http://foo.com/bar", "http://foo.com/bar" },
    { "https://foo.com", "https://foo.com/bar", "https://foo.com/bar" },
    { "http://foo.com/", "http://foo.com#bar", "http://foo.com#bar" },
    { "http://foo.com:80", "http://foo.com:80/b",  "http://foo.com:80/b"},
    { "http://foo.com:888", "http://foo.com:888/b",  "http://foo.com:888/b"},
    // Valid relative changes.
    { "http://foo.com", "#bar", "http://foo.com#bar" },
    { "http://foo.com/", "#bar", "http://foo.com/#bar" },
    { "https://foo.com/", "bar", "https://foo.com/bar" },
    { "http://foo.com/foo/1", "/bar", "http://foo.com/bar" },
    { "http://foo.com/foo/1", "bar", "http://foo.com/foo/bar" },
    { "http://foo.com/", "bar.com", "http://foo.com/bar.com" },
    { "http://foo.com", "bar.com", "http://foo.com/bar.com" },
    { "http://foo.com:888", "bar.com", "http://foo.com:888/bar.com" },
    // Invalid scheme changes.
    { "http://foo.com", "https://foo.com#bar", "" },
    { "https://foo.com", "http://foo.com#bar", "" },
    // Invalid domain changes.
    { "http://foo.com/bar", "http://bar.com", "" },
    { "http://foo.com/bar", "http://www.foo.com/bar2", "" },
    // Valid port change.
    { "http://foo.com", "http://foo.com:80/bar", "http://foo.com/bar" },
    { "http://foo.com:80", "http://foo.com/bar", "http://foo.com/bar" },
    // Invalid port change.
    { "http://foo.com", "http://foo.com:42/bar", "" },
    { "http://foo.com:42", "http://foo.com/bar", "" },
    // Invalid URL.
    { "http://foo.com", "http://fo o.c om/ba r", "" },
    { "http://foo.com:80", "bar", "http://foo.com:80/bar" }
};

TEST_F(HistoryStateUtilTest, TestIsHistoryStateChangeValid) {
  for (size_t i = 0; i < std::size(tests_); ++i) {
    GURL fromUrl(tests_[i].fromUrl);
    GURL toUrl = history_state_util::GetHistoryStateChangeUrl(fromUrl, fromUrl,
                                                              tests_[i].toUrl);
    bool expected_result = tests_[i].expectedUrl.size() > 0;
    bool actual_result = toUrl.is_valid();
    if (actual_result) {
      actual_result = history_state_util::IsHistoryStateChangeValid(fromUrl,
                                                                    toUrl);
    }
    EXPECT_EQ(expected_result, actual_result) << tests_[i].fromUrl << " "
                                              << tests_[i].toUrl;
  }
}

TEST_F(HistoryStateUtilTest, TestGetHistoryStateChangeUrl) {
  for (size_t i = 0; i < std::size(tests_); ++i) {
    GURL fromUrl(tests_[i].fromUrl);
    GURL expectedResult(tests_[i].expectedUrl);
    GURL actualResult = history_state_util::GetHistoryStateChangeUrl(
        fromUrl, fromUrl, tests_[i].toUrl);
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
