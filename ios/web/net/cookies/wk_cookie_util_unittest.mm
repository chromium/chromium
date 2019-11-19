// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookies/wk_cookie_util.h"

#import <WebKit/WebKit.h>

#import "ios/web/public/test/web_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

using WKCookieUtilTest = WebTest;

// Tests that web::WKCookieStoreForBrowserState returns valid WKHTTPCookieStore.
TEST_F(WKCookieUtilTest, WKCookieStoreForBrowserState) {
  WKHTTPCookieStore* store = WKCookieStoreForBrowserState(GetBrowserState());
  EXPECT_TRUE([store isKindOfClass:[WKHTTPCookieStore class]]);
}

}  // namespace web
