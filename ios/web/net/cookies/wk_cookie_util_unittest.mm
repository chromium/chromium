// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookies/wk_cookie_util.h"

#import <WebKit/WebKit.h>

#import "ios/web/public/test/web_test.h"

namespace web {

using WKCookieUtilTest = WebTest;

// Tests that web::WKCookieStoreForBrowserState returns valid WKHTTPCookieStore.
TEST_F(WKCookieUtilTest, WKCookieStoreForBrowserState) {
  WKHTTPCookieStore* store = WKCookieStoreForBrowserState(GetBrowserState());
  EXPECT_TRUE([store isKindOfClass:[WKHTTPCookieStore class]]);
}

}  // namespace web
