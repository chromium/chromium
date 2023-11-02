// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_favicon_mediator.h"

#import "base/bind.h"
#import "components/favicon/core/large_icon_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_provider.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_with_payload.h"
#import "ios/chrome/browser/ui/ntp/metrics/metrics.h"
#import "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Size of the favicon returned by the provider for the most visited items.
const CGFloat kMostVisitedFaviconSize = 48;
// Size below which the provider returns a colored tile instead of an image.
const CGFloat kMostVisitedFaviconMinimalSize = 32;

}  // namespace

@interface ContentSuggestionsFaviconMediator () {
  // Most visited data used for logging the tiles impression. The data is
  // copied when receiving the first non-empty data. This copy is used to make
  // sure only the data received the first time is logged, and only once.
  ntp_tiles::NTPTilesVector _mostVisitedDataForLogging;
}

// Redefined as readwrite
@property(nonatomic, nullable, strong, readwrite)
    FaviconAttributesProvider* mostVisitedAttributesProvider;

@end

@implementation ContentSuggestionsFaviconMediator

#pragma mark - Public.

- (instancetype)initWithLargeIconService:
                    (favicon::LargeIconService*)largeIconService
                          largeIconCache:(LargeIconCache*)largeIconCache {
  self = [super init];
  if (self) {
    _mostVisitedAttributesProvider = [[FaviconAttributesProvider alloc]
        initWithFaviconSize:kMostVisitedFaviconSize
             minFaviconSize:kMostVisitedFaviconMinimalSize
           largeIconService:largeIconService];
    // Set a cache only for the Most Visited provider, as the cache is
    // overwritten for every new results and the size of the favicon fetched for
    // the suggestions is much smaller.
    _mostVisitedAttributesProvider.cache = largeIconCache;
  }
  return self;
}

- (void)setMostVisitedDataForLogging:
    (const ntp_tiles::NTPTilesVector&)mostVisitedData {
  DCHECK(_mostVisitedDataForLogging.empty());
  _mostVisitedDataForLogging = mostVisitedData;
}

- (void)fetchFaviconForMostVisited:(ContentSuggestionsMostVisitedItem*)item {
  __weak ContentSuggestionsFaviconMediator* weakSelf = self;
  __weak ContentSuggestionsMostVisitedItem* weakItem = item;

  void (^completion)(FaviconAttributes*) = ^(FaviconAttributes* attributes) {
    ContentSuggestionsFaviconMediator* strongSelf = weakSelf;
    ContentSuggestionsMostVisitedItem* strongItem = weakItem;
    if (!strongSelf || !strongItem)
      return;

    strongItem.attributes = attributes;
    [strongSelf logFaviconFetchedForItem:strongItem];
    [strongSelf.consumer updateMostVisitedTileConfig:strongItem];
  };

  [self.mostVisitedAttributesProvider fetchFaviconAttributesForURL:item.URL
                                                        completion:completion];
}


#pragma mark - Private.

// If it is the first time the favicon corresponding to `URL` has its favicon
// fetched, its impression is logged.
// This is called when the favicon is fetched and might not represent a tile
// impression (for example, if some tiles are not displayed on screen because
// the screen is too narrow, their favicons are still fetched, and this function
// is called).
- (void)logFaviconFetchedForItem:(ContentSuggestionsMostVisitedItem*)item {
  for (size_t i = 0; i < _mostVisitedDataForLogging.size(); ++i) {
    ntp_tiles::NTPTile& ntpTile = _mostVisitedDataForLogging[i];
    if (ntpTile.url == item.URL) {
      RecordNTPTileImpression(i, ntpTile.source, ntpTile.title_source,
                              item.attributes, ntpTile.url);
      // Reset the URL to be sure to log the impression only once.
      ntpTile.url = GURL();
      break;
    }
  }
}

@end
