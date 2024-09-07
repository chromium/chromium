// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id_wrapper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage_wrapper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/tabs/model/tabs_closer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_info_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/device_form_factor.h"

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
    [items addObject:[GridItemIdentifier tabIdentifier:web_state]];
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
      selectedItemIdentifier:nil];
}

}  // namespace

@interface InactiveTabsMediator () <CRWWebStateObserver,
                                    PrefObserverDelegate,
                                    SnapshotStorageObserver,
                                    WebStateListObserving> {
  // The list of inactive tabs.
  raw_ptr<WebStateList> _webStateList;
  // The snapshot storage of _webStateList.
  __weak SnapshotStorageWrapper* _snapshotStorage;
  // The observers of _webStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<ScopedWebStateListObservation> _scopedWebStateListObservation;
  // The observers of web states from _webStateList.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<ScopedWebStateObservation> _scopedWebStateObservation;
  // Preference service from the application context.
  raw_ptr<PrefService> _prefService;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // TabsClosed used to implement the "close all tabs" operation with support
  // for undoing the operation.
  std::unique_ptr<TabsCloser> _tabsCloser;
}

@end

@implementation InactiveTabsMediator

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                         prefService:(PrefService*)prefService
                     snapshotStorage:(SnapshotStorageWrapper*)snapshotStorage
                          tabsCloser:(std::unique_ptr<TabsCloser>)tabsCloser {
  CHECK(IsInactiveTabsAvailable());
  CHECK(webStateList);
  CHECK(prefService);
  CHECK(snapshotStorage);
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

    _snapshotStorage = snapshotStorage;
    [_snapshotStorage addObserver:self];

    _tabsCloser = std::move(tabsCloser);
  }
  return self;
}

- (void)dealloc {
  [_snapshotStorage removeObserver:self];
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
  [_snapshotStorage removeObserver:self];
  _snapshotStorage = nil;
  _tabsCloser.reset();
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
  GridItemIdentifier* item = [GridItemIdentifier tabIdentifier:webState];
  [_consumer replaceItem:item withReplacementItem:item];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kInactiveTabsTimeThreshold) {
    NSInteger daysThreshold =
        _prefService->GetInteger(prefs::kInactiveTabsTimeThreshold);
    [_consumer updateInactiveTabsDaysThreshold:daysThreshold];
  }
}

#pragma mark - SnapshotStorageObserver

- (void)didUpdateSnapshotStorageWithSnapshotID:(SnapshotIDWrapper*)snapshotID {
  web::WebState* webState = nullptr;
  for (int i = 0; i < _webStateList->count(); i++) {
    SnapshotTabHelper* snapshotTabHelper =
        SnapshotTabHelper::FromWebState(_webStateList->GetWebStateAt(i));
    if (snapshotID.snapshot_id == snapshotTabHelper->GetSnapshotID()) {
      webState = _webStateList->GetWebStateAt(i);
      break;
    }
  }
  if (webState) {
    // It is possible to observe an updated snapshot for a WebState before
    // observing that the WebState has been added to the WebStateList. It is the
    // consumer's responsibility to ignore any updates before inserts.
    GridItemIdentifier* item = [GridItemIdentifier tabIdentifier:webState];
    [_consumer replaceItem:item withReplacementItem:item];
  }
}

#pragma mark - WebStateListObserving

- (void)willChangeWebStateList:(WebStateList*)webStateList
                        change:(const WebStateListChangeDetach&)detachChange
                        status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (_webStateList->IsBatchInProgress()) {
    // Updates are handled in the batch operation observer methods.
    return;
  }

  web::WebState* detachedWebState = detachChange.detached_web_state();
  [_consumer removeItemWithIdentifier:[GridItemIdentifier
                                          tabIdentifier:detachedWebState]
               selectedItemIdentifier:nil];

  _scopedWebStateObservation->RemoveObservation(detachedWebState);
}

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (_webStateList->IsBatchInProgress()) {
    // Updates are handled in the batch operation observer methods.
    return;
  }

  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when the status in WebStateList is updated.
      break;
    case WebStateListChange::Type::kDetach:
      // Do nothing when a WebState is detached.
      break;
    case WebStateListChange::Type::kMove:
    case WebStateListChange::Type::kReplace:
    case WebStateListChange::Type::kGroupCreate:
    case WebStateListChange::Type::kGroupVisualDataUpdate:
    case WebStateListChange::Type::kGroupMove:
    case WebStateListChange::Type::kGroupDelete:
      NOTREACHED();
    case WebStateListChange::Type::kInsert: {
      // Insertions are only supported for iPad multiwindow support when
      // changing the user settings for Inactive Tabs (i.e. when picking a
      // longer inactivity threshold).
      DCHECK_EQ(ui::GetDeviceFormFactor(), ui::DEVICE_FORM_FACTOR_TABLET);

      const WebStateListChangeInsert& insertChange =
          change.As<WebStateListChangeInsert>();
      web::WebState* insertedWebState = insertChange.inserted_web_state();
      int nextItemIndex = insertChange.index() + 1;
      GridItemIdentifier* nextItemIdentifier;
      if (webStateList->ContainsIndex(nextItemIndex)) {
        nextItemIdentifier = [GridItemIdentifier
            tabIdentifier:webStateList->GetWebStateAt(nextItemIndex)];
      }
      [_consumer insertItem:[GridItemIdentifier tabIdentifier:insertedWebState]
                    beforeItemID:nextItemIdentifier
          selectedItemIdentifier:nil];

      _scopedWebStateObservation->AddObservation(insertedWebState);
      break;
    }
  }
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

- (BOOL)addNewItem {
  NOTREACHED();
}

- (BOOL)isItemWithIDSelected:(web::WebStateID)itemID {
  NOTREACHED();
}

- (void)closeItemsWithTabIDs:(const std::set<web::WebStateID>&)tabIDs
                    groupIDs:(const std::set<tab_groups::TabGroupId>&)groupIDs
                    tabCount:(int)tabCount {
  NOTREACHED();
}

- (void)closeAllItems {
  // TODO(crbug.com/40257500): Add metrics when the user closes all inactive
  // tabs.
  CloseAllWebStates(*_webStateList, WebStateList::CLOSE_USER_ACTION);
  [_snapshotStorage removeAllImages];
}

- (void)saveAndCloseAllItems {
  if (![self canCloseTabs]) {
    return;
  }

  // TODO(crbug.com/40257500): Add metrics when the user closes all inactive
  // tabs from regular tab grid.
  _tabsCloser->CloseTabs();
}

- (void)undoCloseAllItems {
  if (![self canUndoCloseAllTabs]) {
    return;
  }
  // TODO(crbug.com/40257500): Add metrics when the user restores all inactive
  // tabs from regular tab grid.
  _tabsCloser->UndoCloseTabs();
}

- (void)discardSavedClosedItems {
  if (![self canUndoCloseAllTabs]) {
    return;
  }
  _tabsCloser->ConfirmDeletion();
}

- (void)showCloseItemsConfirmationActionSheetWithItems:
            (const std::set<web::WebStateID>&)itemIDs
                                                anchor:(UIBarButtonItem*)
                                                           buttonAnchor {
  NOTREACHED();
}

- (void)shareItems:(const std::set<web::WebStateID>&)itemIDs
            anchor:(UIBarButtonItem*)buttonAnchor {
  NOTREACHED();
}

- (void)searchItemsWithText:(NSString*)searchText {
  NOTREACHED();
}

- (void)resetToAllItems {
  NOTREACHED();
}

- (void)selectItemWithID:(web::WebStateID)itemID
                    pinned:(BOOL)pinned
    isFirstActionOnTabGrid:(BOOL)isFirstActionOnTabGrid {
  NOTREACHED();
}

- (void)selectTabGroup:(const TabGroup*)tabGroup {
  NOTREACHED();
}

- (void)closeItemWithID:(web::WebStateID)itemID {
  // TODO(crbug.com/40257500): Add metrics when the user closes an inactive tab.
  int index = GetWebStateIndex(_webStateList, WebStateSearchCriteria{
                                                  .identifier = itemID,
                                              });
  if (index != WebStateList::kInvalidIndex) {
    _webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
  }
}

- (void)setPinState:(BOOL)pinState forItemWithID:(web::WebStateID)itemID {
  NOTREACHED();
}

- (void)deleteTabGroup:(base::WeakPtr<const TabGroup>)group
            sourceView:(UIView*)sourceView {
  NOTREACHED_NORETURN();
}

- (void)closeTabGroup:(base::WeakPtr<const TabGroup>)group {
  NOTREACHED_NORETURN();
}

- (void)ungroupTabGroup:(base::WeakPtr<const TabGroup>)group
             sourceView:(UIView*)sourceView {
  NOTREACHED_NORETURN();
}

#pragma mark - GridToolbarsConfigurationProvider

- (TabGridToolbarsConfiguration*)toolbarsConfiguration {
  TabGridToolbarsConfiguration* toolbarsConfiguration =
      [[TabGridToolbarsConfiguration alloc]
          initWithPage:TabGridPageRegularTabs];
  toolbarsConfiguration.closeAllButton = [self canCloseTabs];
  toolbarsConfiguration.searchButton = YES;
  toolbarsConfiguration.undoButton = [self canUndoCloseAllTabs];
  return toolbarsConfiguration;
}

- (BOOL)didSavedClosedTabs {
  return [self canUndoCloseAllTabs];
}

#pragma mark - Internal

- (BOOL)canCloseTabs {
  return _tabsCloser && _tabsCloser->CanCloseTabs();
}

- (BOOL)canUndoCloseAllTabs {
  return _tabsCloser && _tabsCloser->CanUndoCloseTabs();
}

#pragma mark - GridViewControllerMutator

- (void)userTappedOnItemID:(GridItemIdentifier*)itemID {
  // No-op
}

- (void)addToSelectionItemID:(GridItemIdentifier*)itemID {
  NOTREACHED();
}

- (void)removeFromSelectionItemID:(GridItemIdentifier*)itemID {
  // No-op
}

- (void)closeItemWithIdentifier:(GridItemIdentifier*)identifier {
  CHECK(identifier.type == GridItemType::kTab);
  [self closeItemWithID:identifier.tabSwitcherItem.identifier];
}

@end
