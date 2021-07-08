// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_favicon_mediator.h"

#include "base/bind.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_data_sink.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestion_identifier.h"
#import "ios/chrome/browser/ui/content_suggestions/identifier/content_suggestions_section_information.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_provider.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_with_payload.h"
#include "ios/chrome/browser/ui/ntp/metrics.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Size of the favicon returned by the provider for the suggestions items.
const CGFloat kSuggestionsFaviconSize = 16;
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

// The ContentSuggestionsService, serving suggestions.
@property(nonatomic, assign)
    ntp_snippets::ContentSuggestionsService* contentService;

// FaviconAttributesProvider to fetch the favicon for the suggestions.
@property(nonatomic, nullable, strong)
    FaviconAttributesProvider* suggestionsAttributesProvider;

// Redefined as readwrite
@property(nonatomic, nullable, strong, readwrite)
    FaviconAttributesProvider* mostVisitedAttributesProvider;

@end

@implementation ContentSuggestionsFaviconMediator

@synthesize mostVisitedAttributesProvider = _mostVisitedAttributesProvider;
@synthesize suggestionsAttributesProvider = _suggestionsAttributesProvider;
@synthesize contentService = _contentService;
@synthesize dataSink = _dataSink;

#pragma mark - Public.

- (instancetype)
initWithContentService:(ntp_snippets::ContentSuggestionsService*)contentService
      largeIconService:(favicon::LargeIconService*)largeIconService
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

    _suggestionsAttributesProvider = [[FaviconAttributesProvider alloc]
        initWithFaviconSize:kSuggestionsFaviconSize
             minFaviconSize:1
           largeIconService:largeIconService];
    _contentService = contentService;
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
    [strongSelf.dataSink itemHasChanged:strongItem];
  };

  [self.mostVisitedAttributesProvider fetchFaviconAttributesForURL:item.URL
                                                        completion:completion];
}

- (void)fetchFaviconForSuggestions:(ContentSuggestionsItem*)item
                        inCategory:(ntp_snippets::Category)category {
  __weak ContentSuggestionsFaviconMediator* weakSelf = self;
  __weak ContentSuggestionsItem* weakItem = item;

  void (^completion)(FaviconAttributes*) = ^(FaviconAttributes* attributes) {
    ContentSuggestionsFaviconMediator* strongSelf = weakSelf;
    ContentSuggestionsItem* strongItem = weakItem;
    if (!strongSelf || !strongItem)
      return;

    strongItem.attributes = attributes;
    [strongSelf.dataSink itemHasChanged:strongItem];
    [strongSelf fetchFaviconImageForSuggestions:strongItem inCategory:category];
  };

  GURL URL = item.URL;
  if (category.IsKnownCategory(ntp_snippets::KnownCategories::READING_LIST)) {
    URL = item.faviconURL;
  }

  [self.suggestionsAttributesProvider fetchFaviconAttributesForURL:URL
                                                        completion:completion];
}

#pragma mark - Private.

// Fetches the favicon image for the |item|, based on the
// ContentSuggestionsService.
- (void)fetchFaviconImageForSuggestions:(ContentSuggestionsItem*)item
                             inCategory:(ntp_snippets::Category)category {
  if (!category.IsKnownCategory(ntp_snippets::KnownCategories::ARTICLES)) {
    // TODO(crbug.com/721266): remove this guard once the choice to download the
    // favicon from the google server is done in the provider.
    return;
  }

  __weak ContentSuggestionsFaviconMediator* weakSelf = self;
  __weak ContentSuggestionsItem* weakItem = item;
  void (^imageCallback)(const gfx::Image&) = ^(const gfx::Image& image) {
    ContentSuggestionsFaviconMediator* strongSelf = weakSelf;
    ContentSuggestionsItem* strongItem = weakItem;
    if (!strongSelf || !strongItem || image.IsEmpty())
      return;

    strongItem.attributes = [FaviconAttributesWithPayload
        attributesWithImage:[image.ToUIImage() copy]];
    [strongSelf.dataSink itemHasChanged:strongItem];
  };

  ntp_snippets::ContentSuggestion::ID identifier =
      ntp_snippets::ContentSuggestion::ID(
          category, item.suggestionIdentifier.IDInSection);
  self.contentService->FetchSuggestionFavicon(
      identifier, /* minimum_size_in_pixel = */ 1, kSuggestionsFaviconSize,
      base::BindOnce(imageCallback));
}

// If it is the first time the favicon corresponding to |URL| has its favicon
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
