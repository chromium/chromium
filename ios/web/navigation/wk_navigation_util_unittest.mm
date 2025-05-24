// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_navigation_util.h"

#import <memory>
#import <vector>

#import "base/json/json_reader.h"
#import "base/strings/escape.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/test/test_url_constants.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/scheme_host_port.h"

namespace web {
namespace wk_navigation_util {

typedef PlatformTest WKNavigationUtilTest;

// Tests various inputs for GetSafeItemRange.
TEST_F(WKNavigationUtilTest, GetSafeItemRange) {
  // Session size does not exceed kMaxSessionSize and last_committed_item_index
  // is in range.
  for (int item_count = 0; item_count <= kMaxSessionSize; item_count++) {
    for (int item_index = 0; item_index < item_count; item_index++) {
      int offset = 0;
      int size = 0;
      EXPECT_EQ(item_index,
                GetSafeItemRange(item_index, item_count, &offset, &size))
          << "item_count: " << item_count << " item_index: " << item_index;
      EXPECT_EQ(0, offset) << "item_count: " << item_count
                           << " item_index: " << item_index;
      EXPECT_EQ(item_count, size)
          << "item_count: " << item_count << " item_index: " << item_index;
    }
  }

  // Session size is 1 item longer than kMaxSessionSize.
  int offset = 0;
  int size = 0;
  EXPECT_EQ(0, GetSafeItemRange(0, kMaxSessionSize + 1, &offset, &size));
  EXPECT_EQ(0, offset);
  EXPECT_EQ(kMaxSessionSize, size);

  offset = 0;
  size = 0;
  EXPECT_EQ(
      kMaxSessionSize - 1,
      GetSafeItemRange(kMaxSessionSize, kMaxSessionSize + 1, &offset, &size));
  EXPECT_EQ(1, offset);
  EXPECT_EQ(kMaxSessionSize, size);

  offset = 0;
  size = 0;
  EXPECT_EQ(kMaxSessionSize / 2,
            GetSafeItemRange(kMaxSessionSize / 2, kMaxSessionSize + 1, &offset,
                             &size));
  EXPECT_EQ(0, offset);
  EXPECT_EQ(kMaxSessionSize, size);

  offset = 0;
  size = 0;
  EXPECT_EQ(kMaxSessionSize / 2,
            GetSafeItemRange(kMaxSessionSize / 2 + 1, kMaxSessionSize + 1,
                             &offset, &size));
  EXPECT_EQ(1, offset);
  EXPECT_EQ(kMaxSessionSize, size);

  offset = 0;
  size = 0;
  EXPECT_EQ(kMaxSessionSize / 2 - 1,
            GetSafeItemRange(kMaxSessionSize / 2 - 1, kMaxSessionSize + 1,
                             &offset, &size));
  EXPECT_EQ(0, offset);
  EXPECT_EQ(kMaxSessionSize, size);
}

// Tests that app specific urls and non-placeholder about: urls do not need a
// user agent type, but normal urls and placeholders do.
TEST_F(WKNavigationUtilTest, URLNeedsUserAgentType) {
  // Not app specific or non-placeholder about urls.
  GURL non_user_agent_urls("http://newtab");
  GURL::Replacements scheme_replacements;
  scheme_replacements.SetSchemeStr(kTestAppSpecificScheme);
  EXPECT_FALSE(URLNeedsUserAgentType(
      non_user_agent_urls.ReplaceComponents(scheme_replacements)));
  scheme_replacements.SetSchemeStr(url::kAboutScheme);
  EXPECT_FALSE(URLNeedsUserAgentType(
      non_user_agent_urls.ReplaceComponents(scheme_replacements)));

  // about:blank pages.
  EXPECT_FALSE(URLNeedsUserAgentType(GURL("about:blank")));
  // Normal URL.
  EXPECT_TRUE(URLNeedsUserAgentType(GURL("http://www.0.com")));

  // file:// URL.
  EXPECT_FALSE(URLNeedsUserAgentType(GURL("file://foo.pdf")));

  // App specific URL or a placeholder for an app specific URL.
  GURL app_specific(
      url::SchemeHostPort(kTestAppSpecificScheme, "foo", 0).Serialize());
  EXPECT_FALSE(URLNeedsUserAgentType(app_specific));
}

}  // namespace wk_navigation_util
}  // namespace web
