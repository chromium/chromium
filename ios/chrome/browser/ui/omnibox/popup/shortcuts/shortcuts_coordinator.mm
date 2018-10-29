// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_coordinator.h"

#include "components/ntp_tiles/most_visited_sites.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_cache_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/ntp_tiles/ios_most_visited_sites_factory.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_mediator.h"
#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ShortcutsCoordinator ()

// Redefined as readwrite and as ShortcutsViewController.
@property(nonatomic, strong, readwrite) ShortcutsViewController* viewController;
// The mediator that pushes the most visited tiles and the reading list badge
// value to the view controller.
@property(nonatomic, strong) ShortcutsMediator* mediator;

@end

@implementation ShortcutsCoordinator

- (void)start {
  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(self.browserState);
  LargeIconCache* cache =
      IOSChromeLargeIconCacheFactory::GetForBrowserState(self.browserState);
  std::unique_ptr<ntp_tiles::MostVisitedSites> mostVisitedSites =
      IOSMostVisitedSitesFactory::NewForBrowserState(self.browserState);
  ReadingListModel* readingListModel =
      ReadingListModelFactory::GetForBrowserState(self.browserState);

  self.mediator = [[ShortcutsMediator alloc]
      initWithLargeIconService:largeIconService
                largeIconCache:cache
               mostVisitedSite:std::move(mostVisitedSites)
              readingListModel:readingListModel];

  ShortcutsViewController* shortcutsViewController =
      [[ShortcutsViewController alloc] init];
  self.viewController = shortcutsViewController;
  self.mediator.consumer = shortcutsViewController;
  self.mediator.dispatcher = self.dispatcher;
  shortcutsViewController.commandHandler = self.mediator;
}

- (void)stop {
  self.viewController = nil;
  self.mediator = nil;
}

@end
