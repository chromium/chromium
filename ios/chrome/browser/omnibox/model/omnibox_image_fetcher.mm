// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_image_fetcher.h"

#import "components/image_fetcher/core/image_data_fetcher.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

namespace {

/// The favicon icon size.
const CGFloat kFaviconIconSize = 16;

}  // namespace

@implementation OmniboxImageFetcher {
  // Fetcher for Answers in Suggest images.
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;

  raw_ptr<FaviconLoader> _faviconLoader;
  /// Holds cached images keyed by their URL. The cache is purged when the popup
  /// is closed.
  NSCache<NSString*, UIImage*>* _cachedImages;
}

- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader
                         imageFetcher:
                             (std::unique_ptr<image_fetcher::ImageDataFetcher>)
                                 imageFetcher {
  self = [super init];

  if (self) {
    _imageFetcher = std::move(imageFetcher);
    _faviconLoader = faviconLoader;
    _cachedImages = [[NSCache alloc] init];
  }

  return self;
}

- (void)clearCache {
  [_cachedImages removeAllObjects];
}

- (void)fetchImage:(GURL)imageURL completion:(void (^)(UIImage*))completion {
  NSString* URL = [NSString cr_fromString:imageURL.spec()];
  UIImage* cachedImage = [_cachedImages objectForKey:URL];
  if (cachedImage) {
    completion(cachedImage);
    return;
  }
  __weak NSCache<NSString*, UIImage*>* weakCachedImages = _cachedImages;
  auto callback =
      base::BindOnce(^(const std::string& image_data,
                       const image_fetcher::RequestMetadata& metadata) {
        NSData* data = [NSData dataWithBytes:image_data.data()
                                      length:image_data.size()];

        UIImage* image = [UIImage imageWithData:data
                                          scale:[UIScreen mainScreen].scale];
        if (image) {
          [weakCachedImages setObject:image forKey:URL];
        }
        completion(image);
      });

  _imageFetcher->FetchImageData(imageURL, std::move(callback),
                                NO_TRAFFIC_ANNOTATION_YET);
}

- (void)fetchFavicon:(GURL)pageURL completion:(void (^)(UIImage*))completion {
  if (!_faviconLoader) {
    return;
  }

  _faviconLoader->FaviconForPageUrl(
      pageURL, kFaviconIconSize, kFaviconIconSize,
      /*fallback_to_google_server=*/false,
      ^(FaviconAttributes* attributes, bool cached) {
        if (attributes.faviconImage) {
          completion(attributes.faviconImage);
        }
      });
}

@end
