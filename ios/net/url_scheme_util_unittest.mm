// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/url_scheme_util.h"

#import <Foundation/Foundation.h>

#include "base/stl_util.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace net {

const char* kSchemeTestData[] = {
    "http://foo.com",
    "https://foo.com",
    "data:text/html;charset=utf-8,Hello",
    "about:blank",
    "chrome://settings",
};

using URLSchemeUtilTest = PlatformTest;

TEST_F(URLSchemeUtilTest, NSURLHasDataScheme) {
  for (unsigned int i = 0; i < base::size(kSchemeTestData); ++i) {
    const char* url = kSchemeTestData[i];
    bool nsurl_result = UrlHasDataScheme(
        [NSURL URLWithString:[NSString stringWithUTF8String:url]]);
    bool gurl_result = GURL(url).SchemeIs(url::kDataScheme);
    EXPECT_EQ(gurl_result, nsurl_result) << "Scheme check failed for " << url;
  }
}

}  // namespace net
