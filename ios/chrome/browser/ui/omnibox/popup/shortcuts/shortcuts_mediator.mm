// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_mediator.h"

#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/reading_list/core/reading_list_model.h"
#include "ios/chrome/browser/ntp_tiles/most_visited_sites_observer_bridge.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_provider.h"
#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_consumer.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Maximum number of most visited tiles fetched.
const NSInteger kMaxNumMostVisitedTiles = 4;

const CGFloat kFaviconSize = 48;
const CGFloat kFaviconMinimalSize = 32;

}  // namespace

@interface ShortcutsMediator ()<MostVisitedSitesObserving>

// Most visited items from the MostVisitedSites service currently displayed.
@property(nonatomic, strong)
    NSMutableArray<ShortcutsMostVisitedItem*>* mostVisitedItems;

@property(nonatomic, strong)
    FaviconAttributesProvider* faviconAttributesProvider;

@end

@implementation ShortcutsMediator {
  std::unique_ptr<ntp_tiles::MostVisitedSites> _mostVisitedSites;
  std::unique_ptr<ntp_tiles::MostVisitedSitesObserverBridge> _mostVisitedBridge;
}

- (instancetype)
initWithLargeIconService:(favicon::LargeIconService*)largeIconService
          largeIconCache:(LargeIconCache*)largeIconCache
         mostVisitedSite:
             (std::unique_ptr<ntp_tiles::MostVisitedSites>)mostVisitedSites
        readingListModel:(ReadingListModel*)readingListModel {
  self = [super init];
  if (self) {
    _mostVisitedSites = std::move(mostVisitedSites);
    _mostVisitedBridge =
        std::make_unique<ntp_tiles::MostVisitedSitesObserverBridge>(self);
    _mostVisitedSites->SetMostVisitedURLsObserver(_mostVisitedBridge.get(),
                                                  kMaxNumMostVisitedTiles);

    _faviconAttributesProvider = [[FaviconAttributesProvider alloc]
        initWithFaviconSize:kFaviconSize
             minFaviconSize:kFaviconMinimalSize
           largeIconService:largeIconService];
    _faviconAttributesProvider.cache = largeIconCache;
  }
  return self;
}

- (void)setConsumer:(id<ShortcutsConsumer>)consumer {
  _consumer = consumer;
  [self pushData];
}

#pragma mark - private

- (void)pushData {
  [self.consumer mostVisitedShortcutsAvailable:self.mostVisitedItems];
}

- (void)fetchFaviconForItem:(ShortcutsMostVisitedItem*)item {
  __weak ShortcutsMediator* weakSelf = self;
  [self.faviconAttributesProvider
      fetchFaviconAttributesForURL:item.URL
                        completion:^(FaviconAttributes* attributes) {
                          item.attributes = attributes;
                          [weakSelf.consumer faviconChangedForItem:item];
                        }];
}

#pragma mark - ShortcutsViewControllerDelegate

- (void)openMostVisitedItem:(ShortcutsMostVisitedItem*)item {
  web::NavigationManager::WebLoadParams params(item.URL);
  params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  [self.dispatcher loadURLWithParams:params];
  [self.dispatcher cancelOmniboxEdit];
}

- (void)openBookmarks {
  [self.dispatcher showBookmarksManager];
  [self.dispatcher cancelOmniboxEdit];
}

- (void)openReadingList {
  [self.dispatcher showReadingList];
  [self.dispatcher cancelOmniboxEdit];
}
- (void)openRecentTabs {
  [self.dispatcher showRecentTabs];
  [self.dispatcher cancelOmniboxEdit];
}
- (void)openHistory {
  [self.dispatcher showHistory];
  [self.dispatcher cancelOmniboxEdit];
}

#pragma mark - MostVisitedSitesObserving

- (void)onMostVisitedURLsAvailable:
    (const ntp_tiles::NTPTilesVector&)mostVisited {
  if (self.mostVisitedItems.count) {
    // If some content is already displayed to the user, do not update without a
    // user action.
    return;
  }

  NSMutableArray* newMostVisited = [NSMutableArray array];
  for (const ntp_tiles::NTPTile& tile : mostVisited) {
    ShortcutsMostVisitedItem* item =
        [ShortcutsMostVisitedItem itemWithNTPTile:tile];
    [newMostVisited addObject:item];
    [self fetchFaviconForItem:item];
  }

  self.mostVisitedItems = newMostVisited;
  [self pushData];
}

- (void)onIconMadeAvailable:(const GURL&)siteURL {
  for (ShortcutsMostVisitedItem* item in self.mostVisitedItems) {
    if (item.URL == siteURL) {
      [self fetchFaviconForItem:item];
      return;
    }
  }
}

@end
