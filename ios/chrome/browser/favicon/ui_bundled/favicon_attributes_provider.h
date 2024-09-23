// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_UI_BUNDLED_FAVICON_ATTRIBUTES_PROVIDER_H_
#define IOS_CHROME_BROWSER_FAVICON_UI_BUNDLED_FAVICON_ATTRIBUTES_PROVIDER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

namespace favicon {
class LargeIconService;
}  // namespace favicon

@class FaviconViewProvider;
class LargeIconCache;
class GURL;

// Object to fetch favicon attributes by URL - an image or a fallback icon if
// there is no favicon image available with large enough resolution.
@interface FaviconAttributesProvider : NSObject
// Favicon attributes associated with `URL` will be fetched using
// `largeIconService`. The favicon will be rendered with height and width equal
// to `faviconSize`, and the image will be fetched if the source size is greater
// than or equal to `minFaviconSize`.
- (instancetype)initWithFaviconSize:(CGFloat)faviconSize
                     minFaviconSize:(CGFloat)minFaviconSize
                   largeIconService:(favicon::LargeIconService*)largeIconService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Fetches favicon attributes and calls the completion block.
- (void)fetchFaviconAttributesForURL:(const GURL&)URL
                          completion:(void (^)(FaviconAttributes*))completion;

// LargeIconService used to fetch favicons.
@property(nonatomic, readonly) favicon::LargeIconService* largeIconService;
// Minimal acceptable favicon size. Below that, will fall back to a monogram.
@property(nonatomic, readonly) CGFloat minSize;
// Expected favicon size (in points). Will downscale favicon to this.
@property(nonatomic, readonly) CGFloat faviconSize;
// Cache for the favicon. Using a cache makes the `completion` block to be
// called synchronously and potentially multiple times. If this is null, no
// cache is used.
@property(nonatomic, assign) LargeIconCache* cache;

@end

#endif  // IOS_CHROME_BROWSER_FAVICON_UI_BUNDLED_FAVICON_ATTRIBUTES_PROVIDER_H_
