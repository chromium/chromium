// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_IMAGE_FETCHER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_IMAGE_FETCHER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/favicon/model/favicon_loader.h"

namespace image_fetcher {
class ImageDataFetcher;
}  // namespace image_fetcher

/// A utility class for fetching images specifically for the Omnibox.
/// Fetched images are cached in memory for the duration of the Omnibox session.
@interface OmniboxImageFetcher : NSObject

- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader
                         imageFetcher:
                             (std::unique_ptr<image_fetcher::ImageDataFetcher>)
                                 imageFetcher;

/// Asynchronously fetches the image for the given image URL and executes the
/// completion block with the resulting image.
- (void)fetchImage:(GURL)imageURL completion:(void (^)(UIImage*))completion;

/// Asynchronously fetches the favicon for the given page URL and executes the
/// completion block with the resulting image.
- (void)fetchFavicon:(GURL)pageURL completion:(void (^)(UIImage*))completion;

/// Clears the fetched cached images.
- (void)clearCache;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_IMAGE_FETCHER_H_
