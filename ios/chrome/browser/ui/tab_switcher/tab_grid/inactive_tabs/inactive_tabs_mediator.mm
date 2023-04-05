// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_mediator.h"

#import "base/notreached.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_observer.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_info_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/url/url_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ScopedWebStateListObservation =
    base::ScopedObservation<WebStateList, WebStateListObserver>;
using ScopedWebStateObservation =
    base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>;

namespace {

// Constructs an array of TabSwitcherItems from a `web_state_list` sorted by
// recency, with the most recent first.
NSArray* CreateItemsOrderedByRecency(WebStateList* web_state_list) {
  NSMutableArray* items = [[NSMutableArray alloc] init];
  std::vector<web::WebState*> web_states;

  for (int i = 0; i < web_state_list->count(); i++) {
    web_states.push_back(web_state_list->GetWebStateAt(i));
  }
  std::sort(web_states.begin(), web_states.end(),
            [](web::WebState* a, web::WebState* b) -> bool {
              return a->GetLastActiveTime() > b->GetLastActiveTime();
            });

  for (web::WebState* web_state : web_states) {
    [items addObject:GetTabSwitcherItem(web_state)];
  }
  return items;
}

// Observes all web states from the list with the scoped web state observer.
void AddWebStateObservations(
    ScopedWebStateObservation* scoped_web_state_observation,
    WebStateList* web_state_list) {
  for (int i = 0; i < web_state_list->count(); i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    scoped_web_state_observation->AddObservation(web_state);
  }
}

// Pushes tab items created from the web state list to the consumer. They are
// sorted by recency (see `CreateItemsOrderedByRecency`).
void PopulateConsumerItems(id<TabCollectionConsumer> consumer,
                           WebStateList* web_state_list) {
  [consumer populateItems:CreateItemsOrderedByRecency(web_state_list)
           selectedItemID:nil];
}

}  // namespace

@interface InactiveTabsMediator () <CRWWebStateObserver,
                                    PrefObserverDelegate,
                                    SnapshotCacheObserver,
                                    WebStateListObserving> {
  // The UI consumer to which updates are made.
  __weak id<TabCollectionConsumer, InactiveTabsInfoConsumer> _consumer;
  // The handler for commands related to Inactive Tabs.
  __weak id<InactiveTabsCommands> _commandHandler;
  // The list of inactive tabs.
  WebStateList* _webStateList;
  // The snapshot cache of _webStateList.
  __weak SnapshotCache* _snapshotCache;
  // The observers of _webStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<ScopedWebStateListObservation> _scopedWebStateListObservation;
  // The observers of web states from _webStateList.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<ScopedWebStateObservation> _scopedWebStateObservation;
  // Preference service from the application context.
  PrefService* _prefService;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // The short-term cache for grid thumbnails.
  NSMutableDictionary<NSString*, UIImage*>* _appearanceCache;
}

@end

@implementation InactiveTabsMediator

- (instancetype)initWithConsumer:
                    (id<TabCollectionConsumer, InactiveTabsInfoConsumer>)
                        consumer
                  commandHandler:(id<InactiveTabsCommands>)commandHandler
                    webStateList:(WebStateList*)webStateList
                     prefService:(PrefService*)prefService
                   snapshotCache:(SnapshotCache*)snapshotCache {
  DCHECK(IsInactiveTabsEnabled());
  DCHECK(consumer);
  DCHECK(commandHandler);
  DCHECK(webStateList);
  DCHECK(prefService);
  self = [super init];
  if (self) {
    _consumer = consumer;
    _commandHandler = commandHandler;
    _webStateList = webStateList;

    // Observe the web state list.
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _scopedWebStateListObservation =
        std::make_unique<ScopedWebStateListObservation>(
            _webStateListObserverBridge.get());
    _scopedWebStateListObservation->Observe(_webStateList);

    // Observe all web states from the list.
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _scopedWebStateObservation = std::make_unique<ScopedWebStateObservation>(
        _webStateObserverBridge.get());
    AddWebStateObservations(_scopedWebStateObservation.get(), _webStateList);

    // Observe the preferences for changes to Inactive Tabs settings.
    _prefService = prefService;
    _prefChangeRegistrar.Init(_prefService);
    _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
    // Register to observe any changes on pref backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kInactiveTabsTimeThreshold, &_prefChangeRegistrar);

    // Push the tabs to the consumer.
    PopulateConsumerItems(_consumer, _webStateList);
    // Push the info to the consumer.
    NSInteger daysThreshold =
        _prefService->GetInteger(prefs::kInactiveTabsTimeThreshold);
    [_consumer updateInactiveTabsDaysThreshold:daysThreshold];

    _snapshotCache = snapshotCache;
    [_snapshotCache addObserver:self];

    _appearanceCache = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (void)dealloc {
  [_snapshotCache removeObserver:self];
}

- (NSInteger)numberOfItems {
  return _webStateList->count();
}

- (void)closeItemWithID:(NSString*)itemID {
  // TODO(crbug.com/1418021): Add metrics when the user closes an inactive tab.
  int index = GetTabIndex(_webStateList, WebStateSearchCriteria{
                                             .identifier = itemID,
                                         });
  if (index != WebStateList::kInvalidIndex) {
    _webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
  }
}

- (void)closeAllItems {
  // TODO(crbug.com/1418021): Add metrics when the user closes all inactive
  // tabs.
  _webStateList->CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);
}

- (void)disconnect {
  _consumer = nil;
  _scopedWebStateObservation.reset();
  _webStateObserverBridge.reset();
  _scopedWebStateListObservation.reset();
  _webStateListObserverBridge.reset();
  _webStateList = nullptr;
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefService = nullptr;
  [_snapshotCache removeObserver:self];
  _appearanceCache = nil;
}

#pragma mark - CRWWebStateObserver

- (void)webStateDidStartLoading:(web::WebState*)webState {
  [self updateConsumerItemForWebState:webState];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self updateConsumerItemForWebState:webState];
}

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  [self updateConsumerItemForWebState:webState];
}

- (void)updateConsumerItemForWebState:(web::WebState*)webState {
  [_consumer replaceItemID:webState->GetStableIdentifier()
                  withItem:GetTabSwitcherItem(webState)];
}

#pragma mark - GridImageDataSource

- (void)snapshotForIdentifier:(NSString*)identifier
                   completion:(void (^)(UIImage*))completion {
  if (_appearanceCache[identifier]) {
    completion(_appearanceCache[identifier]);
    return;
  }
  web::WebState* webState =
      GetWebState(_webStateList, WebStateSearchCriteria{
                                     .identifier = identifier,
                                 });
  if (webState) {
    SnapshotTabHelper::FromWebState(webState)->RetrieveColorSnapshot(
        ^(UIImage* image) {
          completion(image);
        });
  }
}

- (void)faviconForIdentifier:(NSString*)identifier
                  completion:(void (^)(UIImage*))completion {
  web::WebState* webState =
      GetWebState(_webStateList, WebStateSearchCriteria{
                                     .identifier = identifier,
                                 });
  if (!webState) {
    return;
  }
  // NTP tabs get no favicon.
  if (IsUrlNtp(webState->GetVisibleURL())) {
    return;
  }
  completion([UIImage imageNamed:@"default_world_favicon_regular"]);

  favicon::FaviconDriver* faviconDriver =
      favicon::WebFaviconDriver::FromWebState(webState);
  if (faviconDriver) {
    gfx::Image favicon = faviconDriver->GetFavicon();
    if (!favicon.IsEmpty()) {
      completion(favicon.ToUIImage());
    }
  }
}

- (void)preloadSnapshotsForVisibleGridItems:
    (NSSet<NSString*>*)visibleGridItems {
  for (int i = 0; i <= _webStateList->count() - 1; i++) {
    web::WebState* web_state = _webStateList->GetWebStateAt(i);
    NSString* identifier = web_state->GetStableIdentifier();

    BOOL isWebStateHidden = ![visibleGridItems containsObject:identifier];
    if (isWebStateHidden) {
      continue;
    }

    __weak __typeof(_appearanceCache) weakAppearanceCache = _appearanceCache;
    auto cacheImage = ^(UIImage* image) {
      weakAppearanceCache[identifier] = image;
    };

    [self snapshotForIdentifier:identifier completion:cacheImage];
  }
}

- (void)clearPreloadedSnapshots {
  [_appearanceCache removeAllObjects];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kInactiveTabsTimeThreshold) {
    NSInteger daysThreshold =
        _prefService->GetInteger(prefs::kInactiveTabsTimeThreshold);
    [_consumer updateInactiveTabsDaysThreshold:daysThreshold];

    if (daysThreshold == kInactiveTabsDisabledByUser) {
      [_commandHandler inactiveTabsExplicitlyDisabledByUser];
    }
  }
}

#pragma mark - SnapshotCacheObserver

- (void)snapshotCache:(SnapshotCache*)snapshotCache
    didUpdateSnapshotForIdentifier:(NSString*)identifier {
  [_appearanceCache removeObjectForKey:identifier];
  web::WebState* webState =
      GetWebState(_webStateList, WebStateSearchCriteria{
                                     .identifier = identifier,
                                 });
  if (webState) {
    // It is possible to observe an updated snapshot for a WebState before
    // observing that the WebState has been added to the WebStateList. It is the
    // consumer's responsibility to ignore any updates before inserts.
    [_consumer replaceItemID:identifier withItem:GetTabSwitcherItem(webState)];
  }
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  // Insertions are only supported for iPad multiwindow support when changing
  // the user settings for Inactive Tabs (i.e. when picking a longer inactivity
  // threshold).
  DCHECK_EQ(ui::GetDeviceFormFactor(), ui::DEVICE_FORM_FACTOR_TABLET);
  DCHECK_EQ(_webStateList, webStateList);
  if (_webStateList->IsBatchInProgress()) {
    // Updates are handled in the batch operation observer methods.
    return;
  }

  [_consumer insertItem:GetTabSwitcherItem(webState)
                atIndex:index
         selectedItemID:nil];

  _scopedWebStateObservation->AddObservation(webState);
}

- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex {
  NOTREACHED();
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  NOTREACHED();
}

- (void)webStateList:(WebStateList*)webStateList
    willDetachWebState:(web::WebState*)webState
               atIndex:(int)index {
  DCHECK_EQ(_webStateList, webStateList);
  if (_webStateList->IsBatchInProgress()) {
    // Updates are handled in the batch operation observer methods.
    return;
  }

  [_consumer removeItemWithID:webState->GetStableIdentifier()
               selectedItemID:nil];

  _scopedWebStateObservation->RemoveObservation(webState);
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  // No-op.
}

- (void)webStateList:(WebStateList*)webStateList
    willCloseWebState:(web::WebState*)webState
              atIndex:(int)atIndex
           userAction:(BOOL)userAction {
  // No-op.
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  // No-op.
}

- (void)webStateList:(WebStateList*)webStateList
    didChangePinnedStateForWebState:(web::WebState*)webState
                            atIndex:(int)index {
  NOTREACHED();
}

- (void)webStateListWillBeginBatchOperation:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);
  _scopedWebStateObservation->RemoveAllObservations();
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);

  AddWebStateObservations(_scopedWebStateObservation.get(), _webStateList);
  PopulateConsumerItems(_consumer, _webStateList);
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  DCHECK_EQ(webStateList, _webStateList);
  _scopedWebStateListObservation.reset();
  _webStateList = nullptr;
}

@end
