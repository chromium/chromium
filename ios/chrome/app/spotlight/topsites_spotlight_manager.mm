// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/topsites_spotlight_manager.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/ref_counted.h"
#import "base/strings/sys_string_conversions.h"
#import "components/history/core/browser/history_types.h"
#import "components/history/core/browser/top_sites.h"
#import "components/history/core/browser/top_sites_observer.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/app/spotlight/searchable_item_factory.h"
#import "ios/chrome/app/spotlight/spotlight_interface.h"
#import "ios/chrome/app/spotlight/spotlight_logger.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/model/top_sites_factory.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_mediator.h"

class SpotlightTopSitesBridge;
class SpotlightTopSitesCallbackBridge;

@interface TopSitesSpotlightManager ()<SyncObserverModelBridge> {
  // Bridge to register for top sites changes. It's important that this instance
  // variable is released before the _topSite one.
  std::unique_ptr<SpotlightTopSitesBridge> _topSitesBridge;

  // Bridge to register for top sites callbacks.
  std::unique_ptr<SpotlightTopSitesCallbackBridge> _topSitesCallbackBridge;

  scoped_refptr<history::TopSites> _topSites;

  // Indicates if a reindex is pending. Reindexes made by calling the external
  // reindexTopSites method are executed at most every second.
  BOOL _isReindexPending;
}
@property(nonatomic, readonly) scoped_refptr<history::TopSites> topSites;

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                    topSites:(scoped_refptr<history::TopSites>)topSites
          spotlightInterface:(SpotlightInterface*)spotlightInterface
       searchableItemFactory:(SearchableItemFactory*)searchableItemFactory;

// Updates all indexed top sites from appropriate source, within limit of number
// of sites shown on NTP.
- (void)updateAllTopSitesSpotlightItems;
// Adds all top sites from appropriate source, within limit of number of sites
// shown on NTP.
- (void)addAllTopSitesSpotlightItems;
// Adds all top sites from TopSites source (most visited sites on device),
// within limit of number of sites shown on NTP.
- (void)addAllLocalTopSitesItems;
// Adds all top sites from Suggestions source (server-based), within limit of
// number of sites shown on NTP.
- (void)onMostVisitedURLsAvailable:
    (const history::MostVisitedURLList&)top_sites;

@end

class SpotlightTopSitesCallbackBridge final {
 public:
  explicit SpotlightTopSitesCallbackBridge(TopSitesSpotlightManager* owner)
      : owner_(owner) {}

  ~SpotlightTopSitesCallbackBridge() {}

  void OnMostVisitedURLsAvailable(const history::MostVisitedURLList& data) {
    [owner_ onMostVisitedURLsAvailable:data];
  }

  base::WeakPtr<SpotlightTopSitesCallbackBridge> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  __weak TopSitesSpotlightManager* owner_;
  base::WeakPtrFactory<SpotlightTopSitesCallbackBridge> weak_ptr_factory_{this};
};

class SpotlightTopSitesBridge : public history::TopSitesObserver {
 public:
  SpotlightTopSitesBridge(TopSitesSpotlightManager* owner,
                          history::TopSites* top_sites)
      : owner_(owner), top_sites_(top_sites) {
    top_sites->AddObserver(this);
  }

  ~SpotlightTopSitesBridge() override {
    top_sites_->RemoveObserver(this);
    top_sites_ = nullptr;
  }

  void TopSitesLoaded(history::TopSites* top_sites) override {}

  void TopSitesChanged(history::TopSites* top_sites,
                       ChangeReason change_reason) override {
    [owner_ updateAllTopSitesSpotlightItems];
  }

 private:
  __weak TopSitesSpotlightManager* owner_;
  raw_ptr<history::TopSites> top_sites_;
};

@implementation TopSitesSpotlightManager
@synthesize topSites = _topSites;

+ (TopSitesSpotlightManager*)topSitesSpotlightManagerWithProfile:
    (ProfileIOS*)profile {
  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForProfile(profile);
  return [[TopSitesSpotlightManager alloc]
      initWithLargeIconService:largeIconService
                      topSites:ios::TopSitesFactory::GetForProfile(profile)
            spotlightInterface:[SpotlightInterface defaultInterface]
         searchableItemFactory:
             [[SearchableItemFactory alloc]
                 initWithLargeIconService:largeIconService
                                   domain:spotlight::DOMAIN_TOPSITES
                    useTitleInIdentifiers:YES]];
}

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                    topSites:(scoped_refptr<history::TopSites>)topSites
          spotlightInterface:(SpotlightInterface*)spotlightInterface
       searchableItemFactory:(SearchableItemFactory*)searchableItemFactory {
  self = [super initWithSpotlightInterface:spotlightInterface
                     searchableItemFactory:searchableItemFactory];
  if (self) {
    DCHECK(topSites);
    _topSites = topSites;
    _topSitesBridge.reset(new SpotlightTopSitesBridge(self, _topSites.get()));
    _topSitesCallbackBridge.reset(new SpotlightTopSitesCallbackBridge(self));
    _isReindexPending = false;
  }
  return self;
}

- (void)updateAllTopSitesSpotlightItems {
  __weak TopSitesSpotlightManager* weakSelf = self;
  [self.searchableItemFactory cancelItemsGeneration];
  [self.spotlightInterface
      deleteSearchableItemsWithDomainIdentifiers:@[
        spotlight::StringFromSpotlightDomain(spotlight::DOMAIN_TOPSITES)
      ]
                               completionHandler:^(NSError* error) {
                                 if (error) {
                                   [SpotlightLogger logSpotlightError:error];
                                   return;
                                 }
                                 [weakSelf addAllTopSitesSpotlightItems];
                               }];
}

- (void)addAllTopSitesSpotlightItems {
  if (!_topSites)
    return;

  [self addAllLocalTopSitesItems];
}

- (void)addAllLocalTopSitesItems {
  _topSites->GetMostVisitedURLs(base::BindOnce(
      &SpotlightTopSitesCallbackBridge::OnMostVisitedURLsAvailable,
      _topSitesCallbackBridge->AsWeakPtr()));
}

- (void)onMostVisitedURLsAvailable:
    (const history::MostVisitedURLList&)top_sites {
  NSUInteger sitesToIndex =
      MIN(top_sites.size(), [MostVisitedTilesMediator maxSitesShown]);
  for (size_t i = 0; i < sitesToIndex; i++) {
    const GURL& URL = top_sites[i].url;

    __weak TopSitesSpotlightManager* weakSelf = self;
    [self.searchableItemFactory
        generateSearchableItem:URL
                         title:base::SysUTF16ToNSString(top_sites[i].title)
            additionalKeywords:@[]
             completionHandler:^(CSSearchableItem* item) {
               [weakSelf.spotlightInterface indexSearchableItems:@[ item ]];
             }];
  }
}

- (void)reindexTopSites {
  if (_isReindexPending) {
    return;
  }
  _isReindexPending = true;
  __weak TopSitesSpotlightManager* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(1 * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        TopSitesSpotlightManager* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        [strongSelf updateAllTopSitesSpotlightItems];
        strongSelf->_isReindexPending = false;
      });
}

- (void)shutdown {
  [super shutdown];
  _topSitesBridge.reset();
  _topSitesCallbackBridge.reset();

  _topSites = nullptr;
}

#pragma mark -
#pragma mark SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self updateAllTopSitesSpotlightItems];
}

@end
