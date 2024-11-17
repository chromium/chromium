// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/open_tabs_spotlight_manager.h"

#import <CoreSpotlight/CoreSpotlight.h>
#import <memory>
#import <queue>

#import "base/apple/foundation_util.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/elapsed_timer.h"
#import "ios/chrome/app/spotlight/searchable_item_factory.h"
#import "ios/chrome/app/spotlight/spotlight_interface.h"
#import "ios/chrome/app/spotlight/spotlight_logger.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"

using web::WebState;

namespace {

// At initial indexing, the # of web states to index per batch before releasing
// the main queue.
const int kBatchSize = 100;

}  // namespace

@interface OpenTabsSpotlightManager () <BrowserListObserver,
                                        WebStateListObserving,
                                        CRWWebStateObserver>

/// Tracks if a clear and reindex operation is pending e.g. while the app is
/// backgrounded.
@property(nonatomic, assign) BOOL needsClearAndReindex;
/// Tracks if a clear and reindex operation is pending e.g. while the app is
/// backgrounded.
@property(nonatomic, assign) BOOL needsFullIndex;
/// Prevents reentry into clearAndReindexIfNeeded method.
@property(nonatomic, assign) BOOL deletionInProgress;

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

  // Tracks the last committed URL for each tab.
  std::map<web::WebStateID, GURL> _lastCommittedURLs;
  // Tracks the number of open tabs with this URL.
  std::map<GURL, NSUInteger> _knownURLCounts;
  // Bridge that observes all web state lists in all non-incognito browsers.
  // Used to keep track of closing tabs.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;

  // At full reindex of a WebStateList, this queue is used for WebStates that
  // require indexing.
  std::queue<base::WeakPtr<WebState>> _indexingQueue;

  // Timer that's counts how long it takes to index all tabs.
  std::unique_ptr<base::ElapsedTimer> _initialIndexTimer;

  // Tracks the number of times reindexing had to restart because of the model
  int _reindexInterruptionCount;
}

#pragma mark - public

+ (OpenTabsSpotlightManager*)openTabsSpotlightManagerWithProfile:
    (ProfileIOS*)profile {
  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForProfile(profile);
  SearchableItemFactory* searchableItemFactory = [[SearchableItemFactory alloc]
      initWithLargeIconService:largeIconService
                        domain:spotlight::DOMAIN_OPEN_TABS
         useTitleInIdentifiers:NO];

  return [[OpenTabsSpotlightManager alloc]
      initWithLargeIconService:largeIconService
                   browserList:BrowserListFactory::GetForProfile(profile)
            spotlightInterface:[SpotlightInterface defaultInterface]
         searchableItemFactory:searchableItemFactory];
}

- (instancetype)
    initWithLargeIconService:(favicon::LargeIconService*)largeIconService
                 browserList:(BrowserList*)browserList
          spotlightInterface:(SpotlightInterface*)spotlightInterface
       searchableItemFactory:(SearchableItemFactory*)searchableItemFactory {
  self = [super initWithSpotlightInterface:spotlightInterface
                     searchableItemFactory:searchableItemFactory];

  if (self) {
    _browserList = browserList;
    _indexingQueue = std::queue<base::WeakPtr<WebState>>();
    _browserListObserverBridge =
        std::make_unique<BrowserListObserverBridge>(self);
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _browserList->AddObserver(_browserListObserverBridge.get());
    [self startObservingAllWebStates];
  }

  return self;
}

- (void)clearAndReindexOpenTabs {
  if (!_browserList) {
    [SpotlightLogger logSpotlightError:[OpenTabsSpotlightManager
                                           browserListNotAvailableError]];
    return;
  }

  // If there is an ongoing indexing, recreate the queue to clear it.
  _indexingQueue = std::queue<base::WeakPtr<WebState>>();

  [self stopObservingAllWebStates];

  self.needsClearAndReindex = YES;
  [self clearAndReindexIfNeeded];
}
- (void)clearAndReindexIfNeeded {
  // If already waiting for Spotlight DB to clear all, don't do anything.
  if (self.deletionInProgress) {
    return;
  }

  if (!self.needsClearAndReindex || self.isAppInBackground) {
    return;
  }

  self.needsFullIndex = NO;
  self.deletionInProgress = YES;
  __weak OpenTabsSpotlightManager* weakSelf = self;
  [self.spotlightInterface
      deleteSearchableItemsWithDomainIdentifiers:@[
        StringFromSpotlightDomain(spotlight::DOMAIN_OPEN_TABS)
      ]
                               completionHandler:^(NSError*) {
                                 weakSelf.deletionInProgress = NO;
                                 if (weakSelf.isShuttingDown) {
                                   return;
                                 }
                                 weakSelf.needsClearAndReindex = NO;
                                 [weakSelf indexAllOpenTabs];
                               }];
}

- (void)shutdown {
  [super shutdown];
  [self shutdownAllObservation];
}

- (void)appWillEnterForeground {
  [super appWillEnterForeground];

  if (self.needsClearAndReindex || self.needsFullIndex) {
    [self clearAndReindexOpenTabs];
    return;
  }

  if (!_indexingQueue.empty()) {
    __weak OpenTabsSpotlightManager* weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
      [weakSelf indexNextBatchFromQueue];
    });
  }
}

#pragma mark - BrowserListObserver

- (void)browserList:(const BrowserList*)browserList
       browserAdded:(Browser*)browser {
  if (browser->type() == Browser::Type::kIncognito) {
    return;
  }
  // If the initial indexing is still in progress, cancel it and restart.
  if (!_indexingQueue.empty()) {
    [self logReindexInterruption];
    [self clearAndReindexOpenTabs];
    return;
  }

  WebStateList* webStateList = browser->GetWebStateList();
  webStateList->AddObserver(_webStateListObserverBridge.get());

  if (self.isAppInBackground) {
    // Normally, no model updates should happen in background.
    // In case they do, process them on foreground.
    self.needsClearAndReindex = YES;
    return;
  }

  [self addAllURLsFromWebStateList:webStateList];

}

- (void)browserList:(const BrowserList*)browserList
     browserRemoved:(Browser*)browser {
  if (browser->type() == Browser::Type::kIncognito) {
    return;
  }
  WebStateList* webStateList = browser->GetWebStateList();
  webStateList->RemoveObserver(_webStateListObserverBridge.get());

  if (self.isAppInBackground) {
    // Normally, no model updates should happen in background.
    // In case they do, process them on foreground.
    self.needsClearAndReindex = YES;
    return;
  }

  [self removeAllURLsFromWebStateList:webStateList];
}

- (void)browserListWillShutdown:(const BrowserList*)browserList {
  [self shutdownAllObservation];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  // If the initial indexing is still in progress, cancel it and restart.
  if (!_indexingQueue.empty() && !self.isAppInBackground) {
    [self logReindexInterruption];
    [self clearAndReindexOpenTabs];
    return;
  }

  if (change.type() == WebStateListChange::Type::kInsert) {
    const WebStateListChangeInsert& insertChange =
        change.As<WebStateListChangeInsert>();
    insertChange.inserted_web_state()->AddObserver(
        _webStateObserverBridge.get());
  } else if (change.type() == WebStateListChange::Type::kDetach) {
    const WebStateListChangeDetach& detachChange =
        change.As<WebStateListChangeDetach>();
    raw_ptr<web::WebState> webState = detachChange.detached_web_state();
    webState->RemoveObserver(_webStateObserverBridge.get());

    if (self.isAppInBackground) {
      // Normally, no model updates should happen in background.
      // In case they do, process them on foreground.
      self.needsClearAndReindex = YES;
      return;
    }

    [self removeLatestCommittedURLForWebState:webState];
  }
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  webStateList->RemoveObserver(_webStateListObserverBridge.get());

  if (self.isAppInBackground) {
    // Normally, no model updates should happen in background.
    // In case they do, process them on foreground.
    self.needsClearAndReindex = YES;
    return;
  }

  [self removeAllURLsFromWebStateList:webStateList];
}

#pragma mark - CRWWebStateObserver

// Invoked by WebStateObserverBridge::DidStartNavigation.
- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigationContext {
  if (self.isAppInBackground) {
    // Normally, no model updates should happen in background.
    // In case they do, process them on foreground.
    self.needsClearAndReindex = YES;
    return;
  }

  [self removeLatestCommittedURLForWebState:webState];
}

// Invoked by WebStateObserverBridge::DidFinishNavigation.
- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigationContext {
  if (self.isAppInBackground) {
    // Normally, no model updates should happen in background.
    // In case they do, process them on foreground.
    self.needsClearAndReindex = YES;
    return;
  }

  [self updateLatestCommittedURLForWebState:webState];
}

- (void)webState:(web::WebState*)webState
    didRedirectNavigation:(web::NavigationContext*)navigationContext {
  if (self.isAppInBackground) {
    // Normally, no model updates should happen in background.
    // In case they do, process them on foreground.
    self.needsClearAndReindex = YES;
    return;
  }

  [self updateLatestCommittedURLForWebState:webState];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  webState->RemoveObserver(_webStateObserverBridge.get());
}

#pragma mark - private

/// Removes whatever the previously remembered URL was for a given webstate.
- (void)removeLatestCommittedURLForWebState:(web::WebState*)webState {
  if (self.isShuttingDown) {
    return;
  }
  [self indexURL:nullptr
              title:nil
      forWebStateID:webState->GetUniqueIdentifier()];
}

/// Updates the remembered URL of the webstate.
- (void)updateLatestCommittedURLForWebState:(web::WebState*)webState {
  if (self.isShuttingDown) {
    return;
  }
  GURL URL = webState->GetLastCommittedURL();
  if (![OpenTabsSpotlightManager shouldIndexURL:URL]) {
    return;
  }

  NSString* title = base::SysUTF16ToNSString(webState->GetTitle());
  [self indexURL:&URL
              title:title
      forWebStateID:webState->GetUniqueIdentifier()];
}

/// Iterates through all webstates in `webStateList` add adds them to the index.
- (void)addAllURLsFromWebStateList:(WebStateList*)webStateList {
  if (self.isShuttingDown) {
    return;
  }

  BOOL wasQueueEmpty = _indexingQueue.empty();

  for (int i = 0; i < webStateList->count(); i++) {
    web::WebState* webState = webStateList->GetWebStateAt(i);
    _indexingQueue.push(webState->GetWeakPtr());
  }

  // When the queue wasn't empty, the indexing is already in progress.
  if (!wasQueueEmpty) {
    return;
  }

  __weak OpenTabsSpotlightManager* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf indexNextBatchFromQueue];
  });
}

- (void)indexNextBatchFromQueue {
  if (self.isShuttingDown) {
    if (!_indexingQueue.empty()) {
      [self logReindexInterruption];
    }
    return;
  }

  if (self.isAppInBackground) {
    return;  // Indexing will resume on foreground.
  }

  for (int i = 0; i < kBatchSize; i++) {
    if (_indexingQueue.empty()) {
      if (_initialIndexTimer) {
        UMA_HISTOGRAM_TIMES("IOS.Spotlight.OpenTabsIndexingDuration",
                            _initialIndexTimer->Elapsed());
        _initialIndexTimer.reset();
      }

      return;
    }

    base::WeakPtr<WebState> webState = _indexingQueue.front();
    _indexingQueue.pop();
    if (webState) {
      [self updateLatestCommittedURLForWebState:webState.get()];
    }
  }

  __weak OpenTabsSpotlightManager* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf indexNextBatchFromQueue];
  });
}

/// Iterates through all webstates in `webStateList` add removes them from the
/// index.
- (void)removeAllURLsFromWebStateList:(WebStateList*)webStateList {
  if (self.isShuttingDown) {
    return;
  }
  for (int i = 0; i < webStateList->count(); i++) {
    web::WebState* webState = webStateList->GetWebStateAt(i);
    [self removeLatestCommittedURLForWebState:webState];
  }
}

/// Iterate over all non-incognito web states and adds them to the index
/// immediately.
- (void)indexAllOpenTabs {
  if (self.isShuttingDown) {
    return;
  }

  if (!_indexingQueue.empty()) {
    // Indexing is already happening, nothing to do.
    return;
  }

  self.needsFullIndex = YES;
  [self indexAllOpenTabsIfNeeded];
}

- (void)indexAllOpenTabsIfNeeded {
  if (self.isAppInBackground || !self.needsFullIndex) {
    return;
  }

  _initialIndexTimer = std::make_unique<base::ElapsedTimer>();

  // Start observing only the web state lists. Individual webstates will be
  // observed as they are batch-indexed.
  [self startObservingAllWebStateLists];

  for (Browser* browser : self.browserList->BrowsersOfType(
           BrowserList::BrowserType::kRegularAndInactive)) {
    WebStateList* webStateList = browser->GetWebStateList();
    [self addAllURLsFromWebStateList:webStateList];
  }

  self.needsFullIndex = NO;

  UMA_HISTOGRAM_COUNTS_1000("IOS.Spotlight.OpenTabsInitialIndexSize",
                            _knownURLCounts.size());
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

/// Only index valid HTTP(S) URLs.
+ (BOOL)shouldIndexURL:(GURL)URL {
  return URL.is_valid() && URL.SchemeIsHTTPOrHTTPS();
}

/// Pass nullptr for `URL` to remove the previously indexed URL without
/// replacing it. In this case, `title` is ignored so `nil` is accepted.
- (void)indexURL:(GURL*)URL
            title:(NSString*)title
    forWebStateID:(web::WebStateID)webStateID {
  if (self.isShuttingDown || self.isAppInBackground) {
    return;
  }
  if (_lastCommittedURLs.contains(webStateID)) {
    GURL lastKnownURL = _lastCommittedURLs[webStateID];
    DCHECK(_knownURLCounts[lastKnownURL] > 0);
    _knownURLCounts[lastKnownURL]--;
    if (_knownURLCounts[lastKnownURL] == 0) {
      // The URL doesn't correspond to any open tab anymore, remove it from the
      // index.
      [self.spotlightInterface deleteSearchableItemsWithIdentifiers:@[
        [self.searchableItemFactory spotlightIDForURL:lastKnownURL]
      ]
                                                  completionHandler:nil];
    }
  }

  if (URL) {
    _lastCommittedURLs[webStateID] = *URL;
    _knownURLCounts[*URL]++;
    if (_knownURLCounts[*URL] == 1) {
      // The URL is newly added, update Spotlight index.
      __weak OpenTabsSpotlightManager* weakSelf = self;
      [self.searchableItemFactory
          generateSearchableItem:*URL
                           title:title
              additionalKeywords:@[]
               completionHandler:^(CSSearchableItem* item) {
                 [weakSelf.spotlightInterface indexSearchableItems:@[ item ]];
               }];
    }
  } else {
    _lastCommittedURLs.erase(webStateID);
  }
}

#pragma mark - observation helpers

/// Stops observing all objects and resets bridges and the browser list.
- (void)shutdownAllObservation {
  if (!_browserList) {
    return;
  }

  [self stopObservingAllWebStates];

  // Stop observing brower list.
  _browserList->RemoveObserver(_browserListObserverBridge.get());
  _browserListObserverBridge.reset();

  // Finally, reset the browser list to make repeated calls safe.
  _browserList = nil;
}

- (void)logReindexInterruption {
  _reindexInterruptionCount++;
  UMA_HISTOGRAM_COUNTS_1000("IOS.Spotlight.OpenTabsReindexRestarted",
                            _reindexInterruptionCount);
}

- (void)stopObservingAllWebStates {
  if (!_browserList) {
    return;
  }

  for (Browser* browser : _browserList->BrowsersOfType(
           BrowserList::BrowserType::kRegularAndInactive)) {
    WebStateList* webStateList = browser->GetWebStateList();
    if (!webStateList) {
      continue;
    }
    for (int i = 0; i < webStateList->count(); i++) {
      WebState* webState = webStateList->GetWebStateAt(i);
      webState->RemoveObserver(_webStateObserverBridge.get());
    }
    webStateList->RemoveObserver(_webStateListObserverBridge.get());
  }

  _webStateObserverBridge = std::make_unique<web::WebStateObserverBridge>(self);
  _webStateListObserverBridge =
      std::make_unique<WebStateListObserverBridge>(self);
}

- (void)startObservingAllWebStateLists {
  if (!_browserList) {
    return;
  }

  [self stopObservingAllWebStates];

  for (Browser* browser : _browserList->BrowsersOfType(
           BrowserList::BrowserType::kRegularAndInactive)) {
    WebStateList* webStateList = browser->GetWebStateList();
    webStateList->AddObserver(_webStateListObserverBridge.get());
  }
}

- (void)startObservingAllWebStates {
  if (!_browserList) {
    return;
  }

  [self startObservingAllWebStateLists];

  for (Browser* browser : _browserList->BrowsersOfType(
           BrowserList::BrowserType::kRegularAndInactive)) {
    WebStateList* webStateList = browser->GetWebStateList();
    for (int i = 0; i < webStateList->count(); i++) {
      web::WebState* webState = webStateList->GetWebStateAt(i);
      webState->AddObserver(_webStateObserverBridge.get());
    }
  }
}

@end
