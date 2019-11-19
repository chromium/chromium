// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_mediator.h"

#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#include "ios/chrome/browser/ntp_tiles/most_visited_sites_observer_bridge.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_provider.h"
#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_consumer.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/url_loading/url_loading_service.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Maximum number of most visited tiles fetched.
const NSInteger kMaxNumMostVisitedTiles = 4;

const CGFloat kFaviconSize = 48;
const CGFloat kFaviconMinimalSize = 32;

}  // namespace

@interface ShortcutsMediator ()<MostVisitedSitesObserving,
                                ReadingListModelBridgeObserver>

// Most visited items from the MostVisitedSites service currently displayed.
@property(nonatomic, strong)
    NSMutableArray<ShortcutsMostVisitedItem*>* mostVisitedItems;

@property(nonatomic, strong)
    FaviconAttributesProvider* faviconAttributesProvider;

@end

@implementation ShortcutsMediator {
  std::unique_ptr<ntp_tiles::MostVisitedSites> _mostVisitedSites;
  std::unique_ptr<ntp_tiles::MostVisitedSitesObserverBridge> _mostVisitedBridge;
  // ShortcutsMediator observes the reading list model to get the reading list
  // badge.
  std::unique_ptr<ReadingListModelBridge> _readingListModelBridge;
  UrlLoadingService* _loadingService;
}

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
              largeIconCache:(LargeIconCache*)largeIconCache
             mostVisitedSite:
                 (std::unique_ptr<ntp_tiles::MostVisitedSites>)mostVisitedSites
            readingListModel:(ReadingListModel*)readingListModel
              loadingService:(UrlLoadingService*)loadingService {
  self = [super init];
  if (self) {
    _faviconAttributesProvider = [[FaviconAttributesProvider alloc]
        initWithFaviconSize:kFaviconSize
             minFaviconSize:kFaviconMinimalSize
           largeIconService:largeIconService];
    _faviconAttributesProvider.cache = largeIconCache;

    _mostVisitedSites = std::move(mostVisitedSites);
    _mostVisitedBridge =
        std::make_unique<ntp_tiles::MostVisitedSitesObserverBridge>(self);
    _mostVisitedSites->SetMostVisitedURLsObserver(_mostVisitedBridge.get(),
                                                  kMaxNumMostVisitedTiles);

    _readingListModelBridge =
        std::make_unique<ReadingListModelBridge>(self, readingListModel);

    _loadingService = loadingService;
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
                          [weakSelf.consumer faviconChangedForURL:item.URL];
                        }];
}

#pragma mark - ShortcutsViewControllerDelegate

- (void)openMostVisitedItem:(ShortcutsMostVisitedItem*)item {
  [self.dispatcher cancelOmniboxEdit];
  UrlLoadParams params = UrlLoadParams::InCurrentTab(item.URL);
  params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  _loadingService->Load(params);
}

- (void)openBookmarks {
  [self.dispatcher cancelOmniboxEdit];
  [self.dispatcher showBookmarksManager];
}

- (void)openReadingList {
  [self.dispatcher cancelOmniboxEdit];
  [self.dispatcher showReadingList];
}
- (void)openRecentTabs {
  [self.dispatcher cancelOmniboxEdit];
  [self.dispatcher showRecentTabs];
}
- (void)openHistory {
  [self.dispatcher cancelOmniboxEdit];
  [self.dispatcher showHistory];
}

#pragma mark - MostVisitedSitesObserving

- (void)onMostVisitedURLsAvailable:
    (const ntp_tiles::NTPTilesVector&)mostVisited {
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

#pragma mark - ReadingListModelBridgeObserver

- (void)readingListModelLoaded:(const ReadingListModel*)model {
  [self.consumer readingListBadgeUpdatedWithCount:model->unread_size()];
}

- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model {
  [self.consumer readingListBadgeUpdatedWithCount:model->unread_size()];
}

@end
