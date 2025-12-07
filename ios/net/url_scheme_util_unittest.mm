// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/url_scheme_util.h"

#import <Foundation/Foundation.h>

#import <array>
#import <string_view>

#import "base/strings/sys_string_conversions.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace net {
namespace {
constexpr auto kSchemeTestData = std::to_array<std::string_view>({
    "http://foo.com",
    "https://foo.com",
    "data:text/html;charset=utf-8,Hello",
    "about:blank",
    "chrome://settings",
});
}  // anonymous namespace

using URLSchemeUtilTest = PlatformTest;

TEST_F(URLSchemeUtilTest, NSURLHasDataScheme) {
  for (std::string_view url : kSchemeTestData) {
    bool nsurl_result =
        UrlHasDataScheme([NSURL URLWithString:base::SysUTF8ToNSString(url)]);
    bool gurl_result = GURL(url).SchemeIs(url::kDataScheme);
    EXPECT_EQ(gurl_result, nsurl_result) << "Scheme check failed for " << url;
  }
}

}  // namespace net
