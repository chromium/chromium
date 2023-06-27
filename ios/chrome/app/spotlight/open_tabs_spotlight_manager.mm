// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/open_tabs_spotlight_manager.h"

#import <CoreSpotlight/CoreSpotlight.h>
#import <memory>

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/spotlight/spotlight_interface.h"
#import "ios/chrome/app/spotlight/spotlight_logger.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::WebState;

@interface OpenTabsSpotlightManager () <BrowserListObserver,
                                        WebStateListObserving,
                                        CRWWebStateObserver>

@end

/// @discussion Keeps a list of currently opened non-incognito tabs in the
/// spotlight index. For this, this class observes all navigations in all opened
/// tabs (by observing all WebStateLists in all non-incognito browsers).
/// Whenever a URL might have changed for a given tab, the `_lastCommittedURLs`
/// is updated, and the counts in `_knownURLCounts` are changed. When a known
/// URL count reaches 0, the spotlight entry is removed, and vice versa.
/// Non-HTTP(S) and invalid URLs are ignored for the purpose of this class.
@implementation OpenTabsSpotlightManager {
  // Bridges browser list events
  std::unique_ptr<BrowserListObserverBridge> _browserListObserverBridge;

  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Tracks the last committed URL for each session (tab).
  std::map<SessionID, GURL> _lastCommittedURLs;
  // Tracks the number of open tabs with this URL.
  std::map<GURL, NSUInteger> _knownURLCounts;
  // Bridges that observe all web state lists in all non-incognito browsers.
  // Used to keep track of closing tabs.
  std::map<WebStateList*, std::unique_ptr<WebStateListObserverBridge>>
      _webStateListObserverBridges;
}

#pragma mark - public

+ (OpenTabsSpotlightManager*)openTabsSpotlightManagerWithBrowserState:
    (ChromeBrowserState*)browserState {
  return [[OpenTabsSpotlightManager alloc]
      initWithLargeIconService:IOSChromeLargeIconServiceFactory::
                                   GetForBrowserState(browserState)
                   browserList:BrowserListFactory::GetForBrowserState(
                                   browserState)
            spotlightInterface:[SpotlightInterface defaultInterface]];
}

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                 browserList:(BrowserList*)browserList
          spotlightInterface:(SpotlightInterface*)spotlightInterface {
  self = [super initWithLargeIconService:largeIconService
                                  domain:spotlight::DOMAIN_OPEN_TABS
                      spotlightInterface:spotlightInterface];
  if (self) {
    _browserList = browserList;
    _browserListObserverBridge =
        std::make_unique<BrowserListObserverBridge>(self);
    _browserList->AddObserver(_browserListObserverBridge.get());
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
  }
  return self;
}

- (void)clearAndReindexOpenTabs {
  if (!_browserList) {
    [SpotlightLogger logSpotlightError:[OpenTabsSpotlightManager
                                           browserListNotAvailableError]];
    return;
  }
  [self.spotlightInterface
      deleteSearchableItemsWithDomainIdentifiers:@[
        StringFromSpotlightDomain(spotlight::DOMAIN_OPEN_TABS)
      ]
                               completionHandler:^(NSError*) {
                                 [self indexAllOpenTabs];
                               }];
  __weak OpenTabsSpotlightManager* weakSelf = self;
  [self clearAllSpotlightItems:^(NSError* error) {
    if (error) {
      [SpotlightLogger logSpotlightError:error];
      return;
    }
    [weakSelf indexAllOpenTabs];
  }];
}

#pragma mark - BaseSpotlightManager

// In Open Tabs model, URLs are unique keys. Titles are ignored.
- (NSString*)spotlightIDForURL:(const GURL&)URL title:(NSString*)title {
  return [super spotlightIDForURL:URL title:@""];
}

- (void)shutdown {
  [self shutdownAllObservation];
  [super shutdown];
}

#pragma mark - BrowserListObserver

- (void)browserList:(const BrowserList*)browserList
       browserAdded:(Browser*)browser {
  WebStateList* webStateList = browser->GetWebStateList();
  [self addAllURLsFromWebStateList:webStateList];

  _webStateListObserverBridges[webStateList] =
      std::make_unique<WebStateListObserverBridge>(self);
  webStateList->AddObserver(_webStateListObserverBridges[webStateList].get());
}

- (void)browserList:(const BrowserList*)browserList
     browserRemoved:(Browser*)browser {
  WebStateList* webStateList = browser->GetWebStateList();

  [self removeAllURLsFromWebStateList:webStateList];

  if (_webStateListObserverBridges[webStateList]) {
    webStateList->RemoveObserver(
        _webStateListObserverBridges[webStateList].get());
    _webStateListObserverBridges.erase(webStateList);
  }
}

- (void)browserListWillShutdown:(const BrowserList*)browserList {
  [self shutdownAllObservation];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                    selection:(const WebStateSelection&)selection {
  if (change.type() == WebStateListChange::Type::kInsert) {
    const WebStateListChangeInsert& insertChange =
        change.As<WebStateListChangeInsert>();
    insertChange.inserted_web_state()->AddObserver(
        _webStateObserverBridge.get());
  }
}

- (void)webStateList:(WebStateList*)webStateList
    willCloseWebState:(web::WebState*)webState
              atIndex:(int)atIndex
           userAction:(BOOL)userAction {
  webState->RemoveObserver(_webStateObserverBridge.get());

  [self removeLatestCommittedURLForWebState:webState];
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  [self removeAllURLsFromWebStateList:webStateList];

  webStateList->RemoveObserver(
      _webStateListObserverBridges[webStateList].get());
  _webStateListObserverBridges.erase(webStateList);
}

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self updateLatestCommittedURLForWebState:webState];
  webState->AddObserver(_webStateObserverBridge.get());
}

#pragma mark - CRWWebStateObserver

// Invoked by WebStateObserverBridge::DidStartNavigation.
- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext {
  [self removeLatestCommittedURLForWebState:webState];
}

// Invoked by WebStateObserverBridge::DidFinishNavigation.
- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  [self updateLatestCommittedURLForWebState:webState];
}

- (void)webState:(web::WebState*)webState
    didRedirectNavigation:(web::NavigationContext*)navigationContext {
  [self updateLatestCommittedURLForWebState:webState];
}

#pragma mark - private

/// Removes whatever the previously remembered URL was for a given webstate.
- (void)removeLatestCommittedURLForWebState:(web::WebState*)webState {
  [self indexURL:nullptr
                     title:nil
      forSessionIdentifier:webState->GetUniqueIdentifier()];
}

/// Updates the remembered URL of the webstate.
- (void)updateLatestCommittedURLForWebState:(web::WebState*)webState {
  GURL URL = webState->GetLastCommittedURL();
  if (![OpenTabsSpotlightManager shouldIndexURL:URL]) {
    return;
  }

  NSString* title = base::SysUTF16ToNSString(webState->GetTitle());
  [self indexURL:&URL
                     title:title
      forSessionIdentifier:webState->GetUniqueIdentifier()];
}

/// Iterates through all webstates in `webStateList` add adds them to the index.
- (void)addAllURLsFromWebStateList:(WebStateList*)webStateList {
  for (int i = 0; i < webStateList->count(); i++) {
    web::WebState* webState = webStateList->GetWebStateAt(i);
    [self updateLatestCommittedURLForWebState:webState];
  }
}

/// Iterates through all webstates in `webStateList` add removes them from the
/// index.
- (void)removeAllURLsFromWebStateList:(WebStateList*)webStateList {
  for (int i = 0; i < webStateList->count(); i++) {
    web::WebState* webState = webStateList->GetWebStateAt(i);
    [self removeLatestCommittedURLForWebState:webState];
  }
}

/// Iterate over all non-incognito web states and adds them to the index
/// immediately.
- (void)indexAllOpenTabs {
  for (Browser* browser : self.browserList->AllRegularBrowsers()) {
    WebStateList* webStateList = browser->GetWebStateList();
    [self addAllURLsFromWebStateList:webStateList];
  }
}

/// NSError to throw when the browser list isn't available.
+ (NSError*)browserListNotAvailableError {
  return [NSError
      errorWithDomain:@"chrome"
                 code:0
             userInfo:@{
               NSLocalizedDescriptionKey :
                   @"BrowserList isn't initialized or already shut down"
             }];
}

- (NSString*)spotlightIDForURL:(const GURL&)URL {
  return [self spotlightIDForURL:URL title:nil];
}

/// Only index valid HTTP(S) URLs.
+ (BOOL)shouldIndexURL:(GURL)URL {
  return URL.is_valid() && URL.SchemeIsHTTPOrHTTPS();
}

/// Pass nullptr for `URL` to remove the previously indexed URL without
/// replacing it. In this case, `title` is ignored so `nil` is accepted.
- (void)indexURL:(GURL*)URL
                   title:(NSString*)title
    forSessionIdentifier:(SessionID)sessionID {
  if (_lastCommittedURLs.contains(sessionID)) {
    GURL lastKnownURL = _lastCommittedURLs[sessionID];
    DCHECK(_knownURLCounts[lastKnownURL] > 0);
    _knownURLCounts[lastKnownURL]--;
    if (_knownURLCounts[lastKnownURL] == 0) {
      // The URL doesn't correspond to any open tab anymore, remove it from the
      // index.
      [self.spotlightInterface deleteSearchableItemsWithIdentifiers:@[
        [self spotlightIDForURL:lastKnownURL]
      ]
                                                  completionHandler:nil];
    }
  }

  if (URL) {
    _lastCommittedURLs[sessionID] = *URL;
    _knownURLCounts[*URL]++;
    if (_knownURLCounts[*URL] == 1) {
      // The URL is newly added, update Spotlight index.
      [self refreshItemsWithURL:*URL title:title];
    }
  } else {
    _lastCommittedURLs.erase(sessionID);
  }
}

/// Stops observing all objects and resets bridges.
- (void)shutdownAllObservation {
  if (!self.browserList) {
    return;
  }

  // Stop observing all webstates.
  for (Browser* browser : self.browserList->AllRegularBrowsers()) {
    WebStateList* webStateList = browser->GetWebStateList();
    for (int i = 0; i < webStateList->count(); i++) {
      WebState* webState = webStateList->GetWebStateAt(i);
      webState->RemoveObserver(_webStateObserverBridge.get());
    }
  }
  _webStateObserverBridge.reset();

  // Stop observing all web state lists
  for (auto it = _webStateListObserverBridges.begin();
       it != _webStateListObserverBridges.end(); ++it) {
    it->first->RemoveObserver(it->second.get());
  }
  _webStateListObserverBridges.clear();

  // Stop observing brower list.
  _browserList->RemoveObserver(_browserListObserverBridge.get());
  _browserListObserverBridge.reset();
}

@end
