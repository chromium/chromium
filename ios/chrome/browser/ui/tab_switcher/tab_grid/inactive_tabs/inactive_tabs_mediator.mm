// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_mediator.h"

#import "base/notreached.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/sessions/core/tab_restore_service.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_observer.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_info_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
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
    [items
        addObject:[[WebStateTabSwitcherItem alloc] initWithWebState:web_state]];
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
  // The saved session window just before close all tabs from regular tab grid
  // is called.
  SessionWindowIOS* _closedSessionWindow;
  // The number of tabs in `_closedSessionWindow` that are synced by
  // TabRestoreService.
  int _syncedClosedTabsCount;
  // Session restoration agent.
  SessionRestorationBrowserAgent* _sessionRestorationAgent;
  // Snapshot agent.
  SnapshotBrowserAgent* _snapshotAgent;
  // TabRestoreService holds the recently closed tabs.
  sessions::TabRestoreService* _tabRestoreService;
}

@end

@implementation InactiveTabsMediator

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                         prefService:(PrefService*)prefService
             sessionRestorationAgent:
                 (SessionRestorationBrowserAgent*)sessionRestorationAgent
                       snapshotAgent:(SnapshotBrowserAgent*)snapshotAgent
                   tabRestoreService:
                       (sessions::TabRestoreService*)tabRestoreService {
  CHECK(IsInactiveTabsAvailable());
  CHECK(webStateList);
  CHECK(prefService);
  CHECK(sessionRestorationAgent);
  CHECK(snapshotAgent);
  CHECK(snapshotAgent->snapshot_cache());
  CHECK(tabRestoreService);
  self = [super init];
  if (self) {
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

    _snapshotCache = snapshotAgent->snapshot_cache();
    [_snapshotCache addObserver:self];

    _sessionRestorationAgent = sessionRestorationAgent;
    _snapshotAgent = snapshotAgent;
    _tabRestoreService = tabRestoreService;
  }
  return self;
}

- (void)dealloc {
  [_snapshotCache removeObserver:self];
}

- (void)setConsumer:
    (id<TabCollectionConsumer, InactiveTabsInfoConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;

  // Push the tabs to the consumer.
  PopulateConsumerItems(_consumer, _webStateList);
  // Push the info to the consumer.
  NSInteger daysThreshold = InactiveTabsTimeThreshold().InDays();
  [_consumer updateInactiveTabsDaysThreshold:daysThreshold];
}

- (NSInteger)numberOfItems {
  return _webStateList->count();
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
  _snapshotCache = nil;
  _sessionRestorationAgent = nullptr;
  [self discardSavedClosedItems];
  _snapshotAgent = nullptr;
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
  TabSwitcherItem* item =
      [[WebStateTabSwitcherItem alloc] initWithWebState:webState];
  [_consumer replaceItemID:webState->GetStableIdentifier() withItem:item];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kInactiveTabsTimeThreshold) {
    NSInteger daysThreshold =
        _prefService->GetInteger(prefs::kInactiveTabsTimeThreshold);
    [_consumer updateInactiveTabsDaysThreshold:daysThreshold];
  }
}

#pragma mark - SnapshotCacheObserver

- (void)snapshotCache:(SnapshotCache*)snapshotCache
    didUpdateSnapshotForIdentifier:(NSString*)identifier {
  web::WebState* webState =
      GetWebState(_webStateList, WebStateSearchCriteria{
                                     .identifier = identifier,
                                 });
  if (webState) {
    // It is possible to observe an updated snapshot for a WebState before
    // observing that the WebState has been added to the WebStateList. It is the
    // consumer's responsibility to ignore any updates before inserts.
    TabSwitcherItem* item =
        [[WebStateTabSwitcherItem alloc] initWithWebState:webState];
    [_consumer replaceItemID:identifier withItem:item];
  }
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  DCHECK_EQ(_webStateList, webStateList);
  if (_webStateList->IsBatchInProgress()) {
    // Updates are handled in the batch operation observer methods.
    return;
  }
  // Insertions are only supported for iPad multiwindow support when changing
  // the user settings for Inactive Tabs (i.e. when picking a longer inactivity
  // threshold).
  DCHECK_EQ(ui::GetDeviceFormFactor(), ui::DEVICE_FORM_FACTOR_TABLET);

  TabSwitcherItem* item =
      [[WebStateTabSwitcherItem alloc] initWithWebState:webState];
  [_consumer insertItem:item atIndex:index selectedItemID:nil];

  _scopedWebStateObservation->AddObservation(webState);
}

- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex {
  NOTREACHED_NORETURN();
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  NOTREACHED_NORETURN();
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
  NOTREACHED_NORETURN();
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

#pragma mark - GridCommands

- (void)addNewItem {
  NOTREACHED_NORETURN();
}

- (void)insertNewItemAtIndex:(NSUInteger)index {
  NOTREACHED_NORETURN();
}

- (BOOL)isItemWithIDSelected:(NSString*)itemID {
  NOTREACHED_NORETURN();
}

- (void)moveItemWithID:(NSString*)itemID toIndex:(NSUInteger)index {
  NOTREACHED_NORETURN();
}

- (void)closeItemsWithIDs:(NSArray<NSString*>*)itemIDs {
  NOTREACHED_NORETURN();
}

- (void)closeAllItems {
  // TODO(crbug.com/1418021): Add metrics when the user closes all inactive
  // tabs.
  _webStateList->CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);
  _snapshotAgent->RemoveAllSnapshots();
}

- (void)saveAndCloseAllItems {
  if (_webStateList->empty()) {
    return;
  }
  // TODO(crbug.com/1418021): Add metrics when the user closes all inactive
  // tabs from regular tab grid.
  _closedSessionWindow = SerializeWebStateList(_webStateList);
  int oldSize = _tabRestoreService ? _tabRestoreService->entries().size() : 0;
  _webStateList->CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);
  _syncedClosedTabsCount =
      _tabRestoreService ? _tabRestoreService->entries().size() - oldSize : 0;
}

- (void)undoCloseAllItems {
  if (!_closedSessionWindow) {
    return;
  }
  // TODO(crbug.com/1418021): Add metrics when the user restores all inactive
  // tabs from regular tab grid.
  _sessionRestorationAgent->RestoreSessionWindow(
      _closedSessionWindow, SessionRestorationScope::kRegularOnly);

  _closedSessionWindow = nil;
  [self removeEntriesFromTabRestoreService];
  _syncedClosedTabsCount = 0;
}

- (void)discardSavedClosedItems {
  if (!_closedSessionWindow) {
    return;
  }
  _syncedClosedTabsCount = 0;
  _closedSessionWindow = nil;
  _snapshotAgent->RemoveAllSnapshots();
}

- (void)
    showCloseItemsConfirmationActionSheetWithItems:(NSArray<NSString*>*)items
                                            anchor:
                                                (UIBarButtonItem*)buttonAnchor {
  NOTREACHED_NORETURN();
}

- (void)shareItems:(NSArray<NSString*>*)items
            anchor:(UIBarButtonItem*)buttonAnchor {
  NOTREACHED_NORETURN();
}

- (NSArray<UIMenuElement*>*)addToButtonMenuElementsForItems:
    (NSArray<NSString*>*)items {
  NOTREACHED_NORETURN();
}

- (void)searchItemsWithText:(NSString*)searchText {
  NOTREACHED_NORETURN();
}

- (void)resetToAllItems {
  NOTREACHED_NORETURN();
}

- (void)fetchSearchHistoryResultsCountForText:(NSString*)searchText
                                   completion:(void (^)(size_t))completion {
  NOTREACHED_NORETURN();
}

#pragma mark - TabCollectionCommands

- (void)selectItemWithID:(NSString*)itemID {
  NOTREACHED_NORETURN();
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

- (void)setPinState:(BOOL)pinState forItemWithIdentifier:(NSString*)identifier {
  NOTREACHED_NORETURN();
}

#pragma mark - Private

// Removes `_syncedClosedTabsCount` most recent entries from the
// TabRestoreService.
- (void)removeEntriesFromTabRestoreService {
  if (!_tabRestoreService) {
    return;
  }
  std::vector<SessionID> identifiers;
  auto iter = _tabRestoreService->entries().begin();
  auto end = _tabRestoreService->entries().end();
  for (int i = 0; i < _syncedClosedTabsCount && iter != end; i++) {
    identifiers.push_back(iter->get()->id);
    iter++;
  }
  for (const SessionID sessionID : identifiers) {
    _tabRestoreService->RemoveTabEntryById(sessionID);
  }
}

@end
