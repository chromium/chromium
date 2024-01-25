// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_favicon_internal.h"

#import <UIKit/UIKit.h>

#import "net/base/apple/url_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ios_web_view {
// Converts web::FaviconURL::IconType to CWVFaviconType.
CWVFaviconType ConvertIconTypeFromWebToCWV(web::FaviconURL::IconType iconType) {
  switch (iconType) {
    case web::FaviconURL::IconType::kInvalid:
      return CWVFaviconTypeInvalid;
    case web::FaviconURL::IconType::kFavicon:
      return CWVFaviconTypeFavicon;
    case web::FaviconURL::IconType::kTouchIcon:
      return CWVFaviconTypeTouchIcon;
    case web::FaviconURL::IconType::kTouchPrecomposedIcon:
      return CWVFaviconTypeTouchPrecomposedIcon;
    case web::FaviconURL::IconType::kWebManifestIcon:
      return CWVFaviconTypeWebManifestIcon;
  }
}
}  // namespace ios_web_view

@implementation CWVFavicon

@synthesize URL = _URL;
@synthesize type = _type;
@synthesize sizes = _sizes;

+ (NSArray<CWVFavicon*>*)faviconsFromFaviconURLs:
    (const std::vector<web::FaviconURL>&)faviconURLs {
  NSMutableArray<CWVFavicon*>* favicons = [NSMutableArray array];
  for (const auto& faviconURL : faviconURLs) {
    CWVFavicon* favicon = [[CWVFavicon alloc] initWithFaviconURL:faviconURL];
    [favicons addObject:favicon];
  }
  return [favicons copy];
}

- (instancetype)initWithFaviconURL:(web::FaviconURL)faviconURL {
  self = [super init];
  if (self) {
    _URL = net::NSURLWithGURL(faviconURL.icon_url);

    _type = ios_web_view::ConvertIconTypeFromWebToCWV(faviconURL.icon_type);

    NSMutableArray<NSValue*>* sizes = [NSMutableArray array];
    for (const auto& size : faviconURL.icon_sizes) {
      NSValue* value =
          [NSValue valueWithCGSize:CGSizeMake(size.width(), size.height())];
      [sizes addObject:value];
    }
    _sizes = [sizes copy];
  }
  return self;
}

@end
