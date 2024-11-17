// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/spotlight_manager.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/app/spotlight/actions_spotlight_manager.h"
#import "ios/chrome/app/spotlight/bookmarks_spotlight_manager.h"
#import "ios/chrome/app/spotlight/open_tabs_spotlight_manager.h"
#import "ios/chrome/app/spotlight/reading_list_spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#import "ios/chrome/app/spotlight/topsites_spotlight_manager.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

// Called from the BrowserBookmarkModelBridge from C++ -> ObjC.
@interface SpotlightManager () {
  BookmarksSpotlightManager* _bookmarkManager;
  TopSitesSpotlightManager* _topSitesManager;
  ActionsSpotlightManager* _actionsManager;
  raw_ptr<TemplateURLService> _templateURLService;
}

@property(nonatomic, strong) ReadingListSpotlightManager* readingListManager;
@property(nonatomic, strong) OpenTabsSpotlightManager* openTabsManager;

- (instancetype)initWithProfile:(ProfileIOS*)profile NS_DESIGNATED_INITIALIZER;

@end

@implementation SpotlightManager

+ (SpotlightManager*)spotlightManagerWithProfile:(ProfileIOS*)profile {
  if (spotlight::IsSpotlightAvailable()) {
    return [[SpotlightManager alloc] initWithProfile:profile];
  }
  return nil;
}

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  DCHECK(profile);
  DCHECK(spotlight::IsSpotlightAvailable());
  self = [super init];
  if (self) {
    _templateURLService =
        ios::TemplateURLServiceFactory::GetForProfile(profile);
    _topSitesManager =
        [TopSitesSpotlightManager topSitesSpotlightManagerWithProfile:profile];
    _bookmarkManager = [BookmarksSpotlightManager
        bookmarksSpotlightManagerWithProfile:profile];
    _actionsManager = [ActionsSpotlightManager actionsSpotlightManager];
    _readingListManager = [ReadingListSpotlightManager
        readingListSpotlightManagerWithProfile:profile];
    _openTabsManager =
        [OpenTabsSpotlightManager openTabsSpotlightManagerWithProfile:profile];
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
  [_topSitesManager reindexTopSites];
  [self.readingListManager clearAndReindexReadingList];
  [self.openTabsManager clearAndReindexOpenTabs];
}

- (void)shutdown {
  [_bookmarkManager shutdown];
  [_topSitesManager shutdown];
  [_actionsManager shutdown];
  [_readingListManager shutdown];
  [_openTabsManager shutdown];

  _bookmarkManager = nil;
  _topSitesManager = nil;
  _actionsManager = nil;
  _readingListManager = nil;
  _openTabsManager = nil;
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
