// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_favicon_internal.h"

#import <UIKit/UIKit.h>
#include <vector>

#include "ios/web/public/favicon/favicon_url.h"
#import "net/base/apple/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ios_web_view {

using CWVFaviconTest = PlatformTest;

// Tests CWVFavicon factory initialization.
TEST_F(CWVFaviconTest, FactoryInitialization) {
  NSURL* url = [NSURL URLWithString:@"https://chromium.org/static/fav.ico"];
  CWVFaviconType type = CWVFaviconTypeFavicon;
  web::FaviconURL::IconType web_type = web::FaviconURL::IconType::kFavicon;
  NSArray<NSValue*>* sizes = @[
    [NSValue valueWithCGSize:CGSizeMake(16, 16)],
    [NSValue valueWithCGSize:CGSizeMake(32, 32)]
  ];
  std::vector<gfx::Size> gfx_sizes = {gfx::Size(16, 16), gfx::Size(32, 32)};
  web::FaviconURL favicon_url(net::GURLWithNSURL(url), web_type, gfx_sizes);

  NSURL* url2 = [NSURL URLWithString:@"https://chromium.org/static/fav.png"];
  CWVFaviconType type2 = CWVFaviconTypeTouchIcon;
  web::FaviconURL::IconType web_type2 = web::FaviconURL::IconType::kTouchIcon;
  NSArray<NSValue*>* sizes2 = @[ [NSValue valueWithCGSize:CGSizeMake(48, 48)] ];
  std::vector<gfx::Size> gfx_sizes2 = {gfx::Size(48, 48)};
  web::FaviconURL favicon_url2(net::GURLWithNSURL(url2), web_type2, gfx_sizes2);

  std::vector<web::FaviconURL> favicon_urls = {favicon_url, favicon_url2};
  NSArray<CWVFavicon*>* favicons =
      [CWVFavicon faviconsFromFaviconURLs:favicon_urls];

  CWVFavicon* favicon = favicons.firstObject;
  EXPECT_NSEQ(url, favicon.URL);
  EXPECT_EQ(type, favicon.type);
  EXPECT_NSEQ(sizes, favicon.sizes);

  CWVFavicon* favicon2 = favicons.lastObject;
  EXPECT_NSEQ(url2, favicon2.URL);
  EXPECT_EQ(type2, favicon2.type);
  EXPECT_NSEQ(sizes2, favicon2.sizes);
}

}  // namespace ios_web_view
