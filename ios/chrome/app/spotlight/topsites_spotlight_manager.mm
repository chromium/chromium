// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/topsites_spotlight_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/suggestions/suggestions_service.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/history/top_sites_factory.h"
#include "ios/chrome/browser/suggestions/suggestions_service_factory.h"
#include "ios/chrome/browser/sync/sync_observer_bridge.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class SpotlightTopSitesBridge;
class SpotlightTopSitesCallbackBridge;
class SpotlightSuggestionsBridge;

@interface TopSitesSpotlightManager ()<SyncObserverModelBridge> {
  // Bridge to register for top sites changes. It's important that this instance
  // variable is released before the _topSite one.
  std::unique_ptr<SpotlightTopSitesBridge> _topSitesBridge;

  // Bridge to register for top sites callbacks.
  std::unique_ptr<SpotlightTopSitesCallbackBridge> _topSitesCallbackBridge;

  // Bridge to register for sync changes.
  std::unique_ptr<SyncObserverBridge> _syncObserverBridge;

  // Bridge to register for suggestion changes.
  std::unique_ptr<SpotlightSuggestionsBridge> _suggestionsBridge;

  bookmarks::BookmarkModel* _bookmarkModel;             // weak
  suggestions::SuggestionsService* _suggestionService;  // weak
  syncer::SyncService* _syncService;                    // weak

  scoped_refptr<history::TopSites> _topSites;
  base::CallbackListSubscription _suggestionsServiceResponseSubscription;

  // Indicates if a reindex is pending. Reindexes made by calling the external
  // reindexTopSites method are executed at most every second.
  BOOL _isReindexPending;
}
@property(nonatomic, readonly) scoped_refptr<history::TopSites> topSites;

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                    topSites:(scoped_refptr<history::TopSites>)topSites
               bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                 syncService:(syncer::SyncService*)syncService
          suggestionsService:
              (suggestions::SuggestionsService*)suggestionsService;

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
- (void)addAllSuggestionsTopSitesItems;
// Callback for topsites mostvisited, adds sites to spotlight.
- (void)onMostVisitedURLsAvailable:
    (const history::MostVisitedURLList&)top_sites;
// Callback for suggestions, adds sites to spotlight.
- (void)onSuggestionsProfileAvailable:
    (const suggestions::SuggestionsProfile&)suggestions_profile;

@end

class SpotlightTopSitesCallbackBridge
    : public base::SupportsWeakPtr<SpotlightTopSitesCallbackBridge> {
 public:
  explicit SpotlightTopSitesCallbackBridge(TopSitesSpotlightManager* owner)
      : owner_(owner) {}

  ~SpotlightTopSitesCallbackBridge() {}

  void OnMostVisitedURLsAvailable(const history::MostVisitedURLList& data) {
    [owner_ onMostVisitedURLsAvailable:data];
  }

 private:
  __weak TopSitesSpotlightManager* owner_;
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
  history::TopSites* top_sites_;
};

class SpotlightSuggestionsBridge
    : public base::SupportsWeakPtr<SpotlightSuggestionsBridge> {
 public:
  explicit SpotlightSuggestionsBridge(TopSitesSpotlightManager* owner)
      : owner_(owner) {}

  ~SpotlightSuggestionsBridge() {}

  void OnSuggestionsProfileAvailable(
      const suggestions::SuggestionsProfile& suggestions_profile) {
    [owner_ onSuggestionsProfileAvailable:suggestions_profile];
  }

 private:
  __weak TopSitesSpotlightManager* owner_;
};

@implementation TopSitesSpotlightManager
@synthesize topSites = _topSites;

+ (TopSitesSpotlightManager*)topSitesSpotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState {
  return [[TopSitesSpotlightManager alloc]
      initWithLargeIconService:IOSChromeLargeIconServiceFactory::
                                   GetForBrowserState(browserState)
                      topSites:ios::TopSitesFactory::GetForBrowserState(
                                   browserState)
                 bookmarkModel:ios::BookmarkModelFactory::GetForBrowserState(
                                   browserState)
                   syncService:SyncServiceFactory::GetForBrowserState(
                                   browserState)
            suggestionsService:suggestions::SuggestionsServiceFactory::
                                   GetForBrowserState(browserState)];
}

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                    topSites:(scoped_refptr<history::TopSites>)topSites
               bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                 syncService:(syncer::SyncService*)syncService
          suggestionsService:
              (suggestions::SuggestionsService*)suggestionsService {
  self = [super initWithLargeIconService:largeIconService
                                  domain:spotlight::DOMAIN_TOPSITES];
  if (self) {
    DCHECK(topSites);
    DCHECK(bookmarkModel);
    DCHECK(syncService);
    DCHECK(suggestionsService);
    _topSites = topSites;
    _topSitesBridge.reset(new SpotlightTopSitesBridge(self, _topSites.get()));
    _topSitesCallbackBridge.reset(new SpotlightTopSitesCallbackBridge(self));
    _bookmarkModel = bookmarkModel;
    _isReindexPending = false;
    if (syncService && suggestionsService) {
      _suggestionsBridge.reset(new SpotlightSuggestionsBridge(self));
      _syncService = syncService;
      _suggestionService = suggestionsService;
      _suggestionsServiceResponseSubscription =
          _suggestionService->AddCallback(base::BindRepeating(
              &SpotlightSuggestionsBridge::OnSuggestionsProfileAvailable,
              _suggestionsBridge->AsWeakPtr()));
      _syncObserverBridge.reset(new SyncObserverBridge(self, syncService));
    }
  }
  return self;
}

- (void)updateAllTopSitesSpotlightItems {
  __weak TopSitesSpotlightManager* weakSelf = self;
  [self clearAllSpotlightItems:^(NSError* error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf addAllTopSitesSpotlightItems];
    });
  }];
}

- (void)addAllTopSitesSpotlightItems {
  if (!_topSites)
    return;

  if (_suggestionService) {
    [self addAllSuggestionsTopSitesItems];
  } else {
    [self addAllLocalTopSitesItems];
  }
}

- (void)addAllLocalTopSitesItems {
  _topSites->GetMostVisitedURLs(base::BindOnce(
      &SpotlightTopSitesCallbackBridge::OnMostVisitedURLsAvailable,
      _topSitesCallbackBridge->AsWeakPtr()));
}

- (void)addAllSuggestionsTopSitesItems {
  if (!_suggestionService->FetchSuggestionsData()) {
    [self addAllLocalTopSitesItems];
  }
}

- (BOOL)isURLBookmarked:(const GURL&)URL {
  if (!_bookmarkModel->loaded())
    return NO;

  std::vector<const bookmarks::BookmarkNode*> nodes;
  _bookmarkModel->GetNodesByURL(URL, &nodes);
  return nodes.size() > 0;
}

- (void)onMostVisitedURLsAvailable:
    (const history::MostVisitedURLList&)top_sites {
  NSUInteger sitesToIndex =
      MIN(top_sites.size(), [ContentSuggestionsMediator maxSitesShown]);
  for (size_t i = 0; i < sitesToIndex; i++) {
    const GURL& URL = top_sites[i].url;

    // Check if the item is bookmarked, in which case it is already indexed.
    if ([self isURLBookmarked:URL]) {
      continue;
    }

    [self refreshItemsWithURL:URL
                        title:base::SysUTF16ToNSString(top_sites[i].title)];
  }
}

- (void)onSuggestionsProfileAvailable:
    (const suggestions::SuggestionsProfile&)suggestionsProfile {
  size_t size = suggestionsProfile.suggestions_size();
  if (size) {
    NSUInteger sitesToIndex =
        MIN(size, [ContentSuggestionsMediator maxSitesShown]);
    for (size_t i = 0; i < sitesToIndex; i++) {
      const suggestions::ChromeSuggestion& suggestion =
          suggestionsProfile.suggestions(i);
      GURL URL = GURL(suggestion.url());
      // Check if the item is bookmarked, in which case it is already indexed.
      if ([self isURLBookmarked:URL]) {
        continue;
      }

      std::string title = suggestion.title();
      [self refreshItemsWithURL:URL title:base::SysUTF8ToNSString(title)];
    }
  } else {
    [self addAllLocalTopSitesItems];
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
  _topSitesBridge.reset();
  _topSitesCallbackBridge.reset();
  _syncObserverBridge.reset();
  _suggestionsBridge.reset();

  _topSites = nullptr;
  _bookmarkModel = nullptr;
  _syncService = nullptr;
  _suggestionService = nullptr;

  [super shutdown];
}

#pragma mark -
#pragma mark SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self updateAllTopSitesSpotlightItems];
}

@end
