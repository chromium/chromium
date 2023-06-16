// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/spotlight_manager.h"

#import "base/check.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/app/spotlight/actions_spotlight_manager.h"
#import "ios/chrome/app/spotlight/bookmarks_spotlight_manager.h"
#import "ios/chrome/app/spotlight/reading_list_spotlight_manager.h"
#import "ios/chrome/app/spotlight/topsites_spotlight_manager.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Called from the BrowserBookmarkModelBridge from C++ -> ObjC.
@interface SpotlightManager ()<BookmarkUpdatedDelegate> {
  BookmarksSpotlightManager* _bookmarkManager;
  TopSitesSpotlightManager* _topSitesManager;
  ActionsSpotlightManager* _actionsManager;
  TemplateURLService* _templateURLService;
}

@property(nonatomic, strong) ReadingListSpotlightManager* readingListManager;

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

@end

@implementation SpotlightManager

+ (SpotlightManager*)spotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState {
  if (spotlight::IsSpotlightAvailable()) {
    return [[SpotlightManager alloc] initWithBrowserState:browserState];
  }
  return nil;
}

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  DCHECK(browserState);
  DCHECK(spotlight::IsSpotlightAvailable());
  self = [super init];
  if (self) {
    _templateURLService =
        ios::TemplateURLServiceFactory::GetForBrowserState(browserState);
    _topSitesManager = [TopSitesSpotlightManager
        topSitesSpotlightManagerWithBrowserState:browserState];
    _bookmarkManager = [BookmarksSpotlightManager
        bookmarksSpotlightManagerWithBrowserState:browserState];
    [_bookmarkManager setDelegate:self];
    _actionsManager = [ActionsSpotlightManager actionsSpotlightManager];
    if (base::FeatureList::IsEnabled(kSpotlightReadingListSource)) {
      _readingListManager = [ReadingListSpotlightManager
          readingListSpotlightManagerWithBrowserState:browserState];
    }
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_bookmarkManager);
  DCHECK(!_topSitesManager);
  DCHECK(!_actionsManager);
  DCHECK(!_readingListManager);
  DCHECK(!_templateURLService);
}

- (void)resyncIndex {
  [_bookmarkManager reindexBookmarksIfNeeded];
  [_actionsManager indexActionsWithIsGoogleDefaultSearchEngine:
                       [self isGoogleDefaultSearchEngine]];
  [self.readingListManager clearAndReindexReadingList];
}

- (void)bookmarkUpdated {
  [_topSitesManager reindexTopSites];
}

- (void)shutdown {
  [_bookmarkManager shutdown];
  [_topSitesManager shutdown];
  [_actionsManager shutdown];
  [_readingListManager shutdown];

  _bookmarkManager = nil;
  _topSitesManager = nil;
  _actionsManager = nil;
  _readingListManager = nil;
  _templateURLService = nullptr;
}

#pragma mark - Private

- (BOOL)isGoogleDefaultSearchEngine {
  if (!_templateURLService) {
    return NO;
  }

  const TemplateURL* defaultURL =
      _templateURLService->GetDefaultSearchProvider();
  BOOL isGoogleDefaultSearchProvider =
      defaultURL &&
      defaultURL->GetEngineType(_templateURLService->search_terms_data()) ==
          SEARCH_ENGINE_GOOGLE;
  return isGoogleDefaultSearchProvider;
}

@end
