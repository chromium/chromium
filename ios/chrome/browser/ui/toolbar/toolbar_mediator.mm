// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_mediator.h"

#include "base/memory/ptr_util.h"
#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/search_engines/search_engine_observer_bridge.h"
#include "ios/chrome/browser/ui/bookmarks/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_consumer.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#import "ios/public/provider/chrome/browser/voice/voice_search_provider.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarMediator ()<BookmarkModelBridgeObserver,
                              CRWWebStateObserver,
                              SearchEngineObserving,
                              WebStateListObserving>

// The current web state associated with the toolbar.
@property(nonatomic, assign) web::WebState* webState;

// The icon for the search button.
@property(nonatomic, strong) UIImage* searchIcon;

// Whether the associated toolbar is in dark mode.
@property(nonatomic, assign) BOOL toolbarDarkMode;

@end

@implementation ToolbarMediator {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  // Bridge to register for bookmark changes.
  std::unique_ptr<bookmarks::BookmarkModelBridge> _bookmarkModelBridge;
  // Listen for default search engine changes.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
}

@synthesize bookmarkModel = _bookmarkModel;
@synthesize consumer = _consumer;
@synthesize webState = _webState;
@synthesize webStateList = _webStateList;
@synthesize searchIcon = _searchIcon;

- (instancetype)init {
  self = [super init];
  if (self) {
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

#pragma mark - Public

- (void)updateConsumerForWebState:(web::WebState*)webState {
  [self updateNavigationBackAndForwardStateForWebState:webState];
  [self updateShareMenuForWebState:webState];
  [self updateBookmarksForWebState:webState];
}

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }

  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }
  _bookmarkModelBridge.reset();
  _searchEngineObserver.reset();
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webState:(web::WebState*)webState
    didPruneNavigationItemsWithCount:(size_t)pruned_item_count {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self.consumer setLoadingState:self.webState->IsLoading()];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  DCHECK_EQ(_webState, webState);
  [self.consumer setLoadingProgressFraction:progress];
}

- (void)webStateDidChangeBackForwardState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateConsumer];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _webState->RemoveObserver(_webStateObserver.get());
  _webState = nullptr;
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  DCHECK_EQ(_webStateList, webStateList);
  [self.consumer setTabCount:_webStateList->count()
           addedInBackground:!activating];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  DCHECK_EQ(_webStateList, webStateList);
  [self.consumer setTabCount:_webStateList->count() addedInBackground:NO];
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  DCHECK_EQ(_webStateList, webStateList);
  self.webState = newWebState;
}

#pragma mark - Setters

- (void)setIncognito:(BOOL)incognito {
  if (incognito == _incognito)
    return;

  _incognito = incognito;
  if (self.searchIcon) {
    // If the searchEngine was already initialized, ask for the new image.
    [self updateSearchIcon];
  }
}

- (void)setTemplateURLService:(TemplateURLService*)templateURLService {
  _templateURLService = templateURLService;
  if (templateURLService) {
    // Listen for default search engine changes.
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, templateURLService);
    templateURLService->Load();
  }
}

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
  }

  _webState = webState;

  if (_webState) {
    _webState->AddObserver(_webStateObserver.get());

    if (self.consumer) {
      [self updateConsumer];
    }
  }
}

- (void)setConsumer:(id<ToolbarConsumer>)consumer {
  _consumer = consumer;
  [_consumer setVoiceSearchEnabled:ios::GetChromeBrowserProvider()
                                       ->GetVoiceSearchProvider()
                                       ->IsVoiceSearchEnabled()];
  if (self.webState) {
    [self updateConsumer];
  }
  if (self.webStateList) {
    [self.consumer setTabCount:_webStateList->count() addedInBackground:NO];
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  // TODO(crbug.com/727427):Add support for DCHECK(webStateList).
  _webStateList = webStateList;
  self.webState = nil;

  if (_webStateList) {
    self.webState = self.webStateList->GetActiveWebState();
    _webStateList->AddObserver(_webStateListObserver.get());

    if (self.consumer) {
      [self.consumer setTabCount:_webStateList->count() addedInBackground:NO];
    }
  }
}

- (void)setBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel {
  _bookmarkModel = bookmarkModel;
  if (self.webState && self.consumer) {
    [self updateConsumer];
  }
  _bookmarkModelBridge.reset();
  if (bookmarkModel) {
    _bookmarkModelBridge =
        std::make_unique<bookmarks::BookmarkModelBridge>(self, bookmarkModel);
  }
}

#pragma mark - Helper methods

// Updates the consumer to match the current WebState.
- (void)updateConsumer {
  DCHECK(self.webState);
  DCHECK(self.consumer);
  [self updateConsumerForWebState:self.webState];

  BOOL isNTP = IsVisibleURLNewTabPage(self.webState);
  [self.consumer setIsNTP:isNTP];
  // Never show the loading UI for an NTP.
  BOOL isLoading = self.webState->IsLoading() && !isNTP;
  [self.consumer setLoadingState:isLoading];
  [self updateBookmarksForWebState:self.webState];
  [self updateShareMenuForWebState:self.webState];
}

// Updates the consumer with the new forward and back states.
- (void)updateNavigationBackAndForwardStateForWebState:
    (web::WebState*)webState {
  DCHECK(webState);
  [self.consumer
      setCanGoForward:webState->GetNavigationManager()->CanGoForward()];
  [self.consumer setCanGoBack:webState->GetNavigationManager()->CanGoBack()];
}

// Updates the bookmark state of the consumer.
- (void)updateBookmarksForWebState:(web::WebState*)webState {
  if (self.webState) {
    GURL URL = webState->GetVisibleURL();
    [self.consumer setPageBookmarked:self.bookmarkModel &&
                                     self.bookmarkModel->IsBookmarked(URL)];
  }
}

// Updates the Share Menu button of the consumer.
- (void)updateShareMenuForWebState:(web::WebState*)webState {
  const GURL& URL = webState->GetLastCommittedURL();
  BOOL shareMenuEnabled =
      URL.is_valid() && !web::GetWebClient()->IsAppSpecificURL(URL);
  [self.consumer setShareMenuEnabled:shareMenuEnabled];
}

// Updates the search icon in the toolbar. This depends on both the current
// search engine as well as the dark mode status of the associated toolbar.
- (void)updateSearchIcon {
  SearchEngineIcon searchEngineIcon = SEARCH_ENGINE_ICON_OTHER;
  if (self.templateURLService &&
      self.templateURLService->GetDefaultSearchProvider() &&
      self.templateURLService->GetDefaultSearchProvider()->GetEngineType(
          self.templateURLService->search_terms_data()) ==
          SEARCH_ENGINE_GOOGLE) {
    searchEngineIcon = SEARCH_ENGINE_ICON_GOOGLE_SEARCH;
  }
  BOOL useDarkIcon = self.incognito || self.toolbarDarkMode;
  UIImage* searchIcon =
      ios::GetChromeBrowserProvider()
          ->GetBrandedImageProvider()
          ->GetToolbarSearchIcon(searchEngineIcon, useDarkIcon);
  DCHECK(searchIcon);
  [self.consumer setSearchIcon:searchIcon];
}

#pragma mark - BookmarkModelBridgeObserver

// If an added or removed bookmark is the same as the current url, update the
// toolbar so the star highlight is kept in sync.
- (void)bookmarkNodeChildrenChanged:
    (const bookmarks::BookmarkNode*)bookmarkNode {
  [self updateBookmarksForWebState:self.webState];
}

// If all bookmarks are removed, update the toolbar so the star highlight is
// kept in sync.
- (void)bookmarkModelRemovedAllNodes {
  [self updateBookmarksForWebState:self.webState];
}

// In case we are on a bookmarked page before the model is loaded.
- (void)bookmarkModelLoaded {
  [self updateBookmarksForWebState:self.webState];
}

- (void)bookmarkNodeChanged:(const bookmarks::BookmarkNode*)bookmarkNode {
  // No-op -- required by BookmarkModelBridgeObserver but not used.
}
- (void)bookmarkNode:(const bookmarks::BookmarkNode*)bookmarkNode
     movedFromParent:(const bookmarks::BookmarkNode*)oldParent
            toParent:(const bookmarks::BookmarkNode*)newParent {
  // No-op -- required by BookmarkModelBridgeObserver but not used.
}
- (void)bookmarkNodeDeleted:(const bookmarks::BookmarkNode*)node
                 fromFolder:(const bookmarks::BookmarkNode*)folder {
  // No-op -- required by BookmarkModelBridgeObserver but not used.
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  [self updateSearchIcon];
}

#pragma mark - AdaptiveToolbarViewControllerDelegate

- (void)userInterfaceStyleChangedForViewController:
    (AdaptiveToolbarViewController*)viewController {
  self.toolbarDarkMode = viewController.traitCollection.userInterfaceStyle ==
                         UIUserInterfaceStyleDark;
  [self updateSearchIcon];
}

@end
