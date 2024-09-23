// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/ui_bundled/favicon_attributes_provider.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/cancelable_task_tracker.h"
#import "components/favicon/core/fallback_url_util.h"
#import "components/favicon/core/large_icon_service.h"
#import "components/favicon_base/fallback_icon_style.h"
#import "components/favicon_base/favicon_types.h"
#import "ios/chrome/browser/favicon/model/large_icon_cache.h"
#import "ios/chrome/browser/favicon/ui_bundled/favicon_attributes_with_payload.h"
#import "skia/ext/skia_utils_ios.h"
#import "url/gurl.h"

@interface FaviconAttributesProvider () {
  // Used to cancel tasks for the LargeIconService.
  base::CancelableTaskTracker _faviconTaskTracker;
}

@end

@implementation FaviconAttributesProvider
@synthesize largeIconService = _largeIconService;
@synthesize minSize = _minSize;
@synthesize faviconSize = _faviconSize;
@synthesize cache = _cache;

- (instancetype)initWithFaviconSize:(CGFloat)faviconSize
                     minFaviconSize:(CGFloat)minFaviconSize
                   largeIconService:
                       (favicon::LargeIconService*)largeIconService {
  DCHECK(largeIconService);
  self = [super init];
  if (self) {
    _faviconSize = faviconSize;
    _minSize = minFaviconSize;
    _largeIconService = largeIconService;
  }

  return self;
}

- (void)fetchFaviconAttributesForURL:(const GURL&)URL
                          completion:(void (^)(FaviconAttributes*))completion {
  GURL blockURL(URL);
  void (^faviconBlock)(const favicon_base::LargeIconResult&) =
      ^(const favicon_base::LargeIconResult& result) {
        FaviconAttributesWithPayload* attributes = nil;
        if (result.bitmap.is_valid()) {
          scoped_refptr<base::RefCountedMemory> data =
              result.bitmap.bitmap_data.get();
          UIImage* favicon =
              [UIImage imageWithData:[NSData dataWithBytes:data->front()
                                                    length:data->size()]];
          attributes =
              [FaviconAttributesWithPayload attributesWithImage:favicon];
          attributes.iconType = result.bitmap.icon_type;
        } else if (result.fallback_icon_style) {
          UIColor* backgroundColor = skia::UIColorFromSkColor(
              result.fallback_icon_style->background_color);
          UIColor* textColor =
              skia::UIColorFromSkColor(result.fallback_icon_style->text_color);
          NSString* monogram =
              base::SysUTF16ToNSString(favicon::GetFallbackIconText(blockURL));
          attributes = [FaviconAttributesWithPayload
              attributesWithMonogram:monogram
                           textColor:textColor
                     backgroundColor:backgroundColor
              defaultBackgroundColor:result.fallback_icon_style->
                                     is_default_background_color];
        }
        DCHECK(attributes);
        completion(attributes);
      };

  __weak FaviconAttributesProvider* weakSelf = self;
  void (^faviconBlockSaveToCache)(const favicon_base::LargeIconResult&) =
      ^(const favicon_base::LargeIconResult& result) {
        faviconBlock(result);

        FaviconAttributesProvider* strongSelf = weakSelf;
        if (strongSelf.cache &&
            (result.bitmap.is_valid() || result.fallback_icon_style)) {
          strongSelf.cache->SetCachedResult(blockURL, result);
        }
      };

  if (self.cache) {
    std::unique_ptr<favicon_base::LargeIconResult> cached_result =
        self.cache->GetCachedResult(URL);
    if (cached_result) {
      faviconBlock(*cached_result);
    }
  }

  // Always call LargeIconService in case the favicon was updated.
  CGFloat faviconSize = [UIScreen mainScreen].scale * self.faviconSize;
  CGFloat minFaviconSize = [UIScreen mainScreen].scale * self.minSize;
  self.largeIconService->GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      URL, minFaviconSize, faviconSize,
      base::BindRepeating(faviconBlockSaveToCache), &_faviconTaskTracker);
}
@end
