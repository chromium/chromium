// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/url_scheme_util.h"

#import <Foundation/Foundation.h>

#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace web {

using URLSchemeUtilTest = PlatformTest;

TEST_F(URLSchemeUtilTest, UrlHasWebScheme) {
  EXPECT_TRUE(UrlHasWebScheme(GURL("http://foo.com")));
  EXPECT_TRUE(UrlHasWebScheme(GURL("https://foo.com")));
  EXPECT_TRUE(UrlHasWebScheme(GURL("data:text/html;charset=utf-8,Hello")));
  EXPECT_FALSE(UrlHasWebScheme(GURL("about:blank")));
  EXPECT_FALSE(UrlHasWebScheme(GURL("chrome://settings")));
}

TEST_F(URLSchemeUtilTest, NSURLHasWebScheme) {
  EXPECT_TRUE(UrlHasWebScheme([NSURL URLWithString:@"http://foo.com"]));
  EXPECT_TRUE(UrlHasWebScheme([NSURL URLWithString:@"https://foo.com"]));
  EXPECT_TRUE(UrlHasWebScheme(
      [NSURL URLWithString:@"data:text/html;charset=utf-8,Hello"]));
  EXPECT_FALSE(UrlHasWebScheme([NSURL URLWithString:@"about:blank"]));
  EXPECT_FALSE(UrlHasWebScheme([NSURL URLWithString:@"chrome://settings"]));
}

}  // namespace web
