// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mediator.h"

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <memory>

#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/scoped_multi_source_observation.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/sessions/core/tab_restore_service.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/commerce/shopping_persisted_data_tab_helper.h"
#import "ios/chrome/browser/drag_and_drop/drag_item_util.h"
#import "ios/chrome/browser/main/browser_util.h"
#import "ios/chrome/browser/reading_list/reading_list_browser_agent.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/snapshots/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_observer.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/tabs_search/tabs_search_service.h"
#import "ios/chrome/browser/tabs_search/tabs_search_service_factory.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/chrome/browser/web_state_list/web_state_list_serialization.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/mac/url_conversions.h"
#import "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using PinnedState = WebStateSearchCriteria::PinnedState;

namespace {

// Constructs an array of TabSwitcherItems from a `web_state_list` sorted by
// last active time.
NSArray<TabSwitcherItem*>* CreateItemsOrderedByLastActiveTime(
    WebStateList* web_state_list) {
  DCHECK(IsTabGridSortedByRecency());
  NSMutableArray<TabSwitcherItem*>* items = [[NSMutableArray alloc] init];
  std::vector<web::WebState*> web_states;

  int first_index = web_state_list->GetIndexOfFirstNonPinnedWebState();
  DCHECK(first_index == 0 || IsPinnedTabsEnabled());

  for (int i = first_index; i < web_state_list->count(); i++) {
    DCHECK(!web_state_list->IsWebStatePinnedAt(i));
    web_states.push_back(web_state_list->GetWebStateAt(i));
  }
  std::sort(web_states.begin(), web_states.end(),
            [](web::WebState* a, web::WebState* b) -> bool {
              return a->GetLastActiveTime() < b->GetLastActiveTime();
            });

  for (web::WebState* web_state : web_states) {
    [items
        addObject:[[WebStateTabSwitcherItem alloc] initWithWebState:web_state]];
  }
  return items;
}

// Constructs an array of TabSwitcherItems from a `web_state_list` sorted by
// index.
NSArray<TabSwitcherItem*>* CreateItemsOrderedByIndex(
    WebStateList* web_state_list) {
  DCHECK(!IsTabGridSortedByRecency());
  NSMutableArray<TabSwitcherItem*>* items = [[NSMutableArray alloc] init];

  int first_index = web_state_list->GetIndexOfFirstNonPinnedWebState();
  DCHECK(first_index == 0 || IsPinnedTabsEnabled());

  for (int i = first_index; i < web_state_list->count(); i++) {
    DCHECK(!web_state_list->IsWebStatePinnedAt(i));
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    [items
        addObject:[[WebStateTabSwitcherItem alloc] initWithWebState:web_state]];
  }
  return items;
}

// Constructs an array of TabSwitcherItems from a `web_state_list`.
NSArray<TabSwitcherItem*>* CreateItems(WebStateList* web_state_list) {
  if (IsTabGridSortedByRecency()) {
    return CreateItemsOrderedByLastActiveTime(web_state_list);
  }
  return CreateItemsOrderedByIndex(web_state_list);
}

void LogPriceDropMetrics(web::WebState* web_state) {
  ShoppingPersistedDataTabHelper* shopping_helper =
      ShoppingPersistedDataTabHelper::FromWebState(web_state);
  if (!shopping_helper)
    return;
  const ShoppingPersistedDataTabHelper::PriceDrop* price_drop =
      shopping_helper->GetPriceDrop();
  BOOL has_price_drop =
      price_drop && price_drop->current_price && price_drop->previous_price;
  base::RecordAction(base::UserMetricsAction(
      base::StringPrintf("Commerce.TabGridSwitched.%s",
                         has_price_drop ? "HasPriceDrop" : "NoPriceDrop")
          .c_str()));
}

// Returns the Browser with `identifier` in its WebStateList. Returns `nullptr`
// if not found.
Browser* GetBrowserForTabWithId(BrowserList* browser_list,
                                NSString* identifier,
                                bool is_otr_tab) {
  std::set<Browser*> browsers = is_otr_tab
                                    ? browser_list->AllIncognitoBrowsers()
                                    : browser_list->AllRegularBrowsers();
  for (Browser* browser : browsers) {
    WebStateList* webStateList = browser->GetWebStateList();
    int index =
        GetTabIndex(webStateList, WebStateSearchCriteria{
                                      .identifier = identifier,
                                      .pinned_state = PinnedState::kNonPinned,
                                  });
    if (index != WebStateList::kInvalidIndex)
      return browser;
  }
  return nullptr;
}

// Records the number of Tabs closed after a bulk or a "Close All" operation.
void RecordTabGridCloseTabsCount(int count) {
  base::UmaHistogramCounts100("IOS.TabGrid.CloseTabs", count);
}

}  // namespace

@interface TabGridMediator () <CRWWebStateObserver,
                               SnapshotCacheObserver,
                               WebStateListObserving>
// The list from the browser.
@property(nonatomic, assign) WebStateList* webStateList;
// The browser state from the browser.
@property(nonatomic, readonly) ChromeBrowserState* browserState;
// The UI consumer to which updates are made.
@property(nonatomic, weak) id<TabCollectionConsumer> consumer;
// The saved session window just before close all tabs is called.
@property(nonatomic, strong) SessionWindowIOS* closedSessionWindow;
// The number of tabs in `closedSessionWindow` that are synced by
// TabRestoreService.
@property(nonatomic, assign) int syncedClosedTabsCount;
@end

@implementation TabGridMediator {
  // Observers for WebStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<
      base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>>
      _scopedWebStateListObservation;
  // Observer for WebStates.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<
      base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>>
      _scopedWebStateObservation;

  // ItemID of the dragged tab. Used to check if the dropped tab is from the
  // same Chrome window.
  NSString* _dragItemID;
}

- (instancetype)initWithConsumer:(id<TabCollectionConsumer>)consumer {
  if (self = [super init]) {
    _consumer = consumer;
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _scopedWebStateListObservation = std::make_unique<
        base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>>(
        _webStateListObserverBridge.get());
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _scopedWebStateObservation =
        std::make_unique<base::ScopedMultiSourceObservation<
            web::WebState, web::WebStateObserver>>(
            _webStateObserverBridge.get());
  }
  return self;
}

- (void)prepareToShowTabGrid {
  DCHECK(IsTabGridSortedByRecency());
  [self resetToAllItems];
}

#pragma mark - Public properties

- (void)setBrowser:(Browser*)browser {
  [self.snapshotCache removeObserver:self];
  _scopedWebStateListObservation->RemoveAllObservations();
  _scopedWebStateObservation->RemoveAllObservations();

  _browser = browser;

  _webStateList = browser ? browser->GetWebStateList() : nullptr;
  _browserState = browser ? browser->GetBrowserState() : nullptr;

  [self.snapshotCache addObserver:self];

  if (_webStateList) {
    _scopedWebStateListObservation->AddObservation(_webStateList);
    [self addWebStateObservations];

    if (self.webStateList->count() > 0) {
      [self populateConsumerItems];
    }
  }
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                    selection:(const WebStateSelection&)selection {
  DCHECK_EQ(_webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    return;
  }

  switch (change.type()) {
    case WebStateListChange::Type::kSelectionOnly:
      // TODO(crbug.com/1442546): Move the implementation from
      // webStateList:didChangeActiveWebState:oldWebState:atIndex:reason and
      // webStateList:didChangePinnedStateForWebState:atIndex to here. Note that
      // here is reachable only when `reason` ==
      // ActiveWebStateChangeReason::Activated in didChangeActiveWebState:.
      break;
    case WebStateListChange::Type::kDetach:
      // Do nothing when a WebState is detached.
      break;
    case WebStateListChange::Type::kMove: {
      if (IsPinnedTabsEnabled() &&
          webStateList->IsWebStatePinnedAt(selection.index)) {
        return;
      }

      const WebStateListChangeMove& moveChange =
          change.As<WebStateListChangeMove>();
      NSUInteger itemIndex =
          [self itemIndexFromWebStateListIndex:selection.index];
      [self.consumer
          moveItemWithID:moveChange.moved_web_state()->GetStableIdentifier()
                 toIndex:itemIndex];
      break;
    }
    case WebStateListChange::Type::kReplace: {
      if (IsPinnedTabsEnabled() &&
          webStateList->IsWebStatePinnedAt(selection.index)) {
        return;
      }

      const WebStateListChangeReplace& replaceChange =
          change.As<WebStateListChangeReplace>();
      web::WebState* replacedWebState = replaceChange.replaced_web_state();
      web::WebState* insertedWebState = replaceChange.inserted_web_state();
      TabSwitcherItem* newItem =
          [[WebStateTabSwitcherItem alloc] initWithWebState:insertedWebState];
      [self.consumer replaceItemID:replacedWebState->GetStableIdentifier()
                          withItem:newItem];

      _scopedWebStateObservation->RemoveObservation(replacedWebState);
      _scopedWebStateObservation->AddObservation(insertedWebState);
      break;
    }
    case WebStateListChange::Type::kInsert: {
      if (IsPinnedTabsEnabled() &&
          webStateList->IsWebStatePinnedAt(selection.index)) {
        [self.consumer
            selectItemWithID:GetActiveWebStateIdentifier(
                                 webStateList,
                                 WebStateSearchCriteria{
                                     .pinned_state = PinnedState::kNonPinned,
                                 })];
        return;
      }

      const WebStateListChangeInsert& insertChange =
          change.As<WebStateListChangeInsert>();
      web::WebState* insertedWebState = insertChange.inserted_web_state();
      TabSwitcherItem* item =
          [[WebStateTabSwitcherItem alloc] initWithWebState:insertedWebState];
      NSUInteger itemIndex =
          [self itemIndexFromWebStateListIndex:selection.index];
      [self.consumer insertItem:item
                        atIndex:itemIndex
                 selectedItemID:GetActiveWebStateIdentifier(
                                    webStateList,
                                    WebStateSearchCriteria{
                                        .pinned_state = PinnedState::kNonPinned,
                                    })];

      _scopedWebStateObservation->AddObservation(insertedWebState);
      break;
    }
  }
}

- (void)webStateList:(WebStateList*)webStateList
    willDetachWebState:(web::WebState*)webState
               atIndex:(int)index {
  DCHECK_EQ(_webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    return;
  }

  // If the WebState is pinned and it is not in the consumer's items list,
  // consumer will filter it out in the method's implementation.
  [self.consumer
      removeItemWithID:webState->GetStableIdentifier()
        selectedItemID:GetActiveWebStateIdentifier(
                           webStateList,
                           WebStateSearchCriteria{
                               .pinned_state = PinnedState::kNonPinned,
                           })];

  const bool isPinnedWebState =
      IsPinnedTabsEnabled() && webStateList->IsWebStatePinnedAt(index);

  // The pinned WebState could be detached only in case it was displayed in the
  // Tab Search and was closed from the context menu. In such a case there were
  // no observation added for it. Therefore, there is no need to remove one.
  if (!isPinnedWebState) {
    _scopedWebStateObservation->RemoveObservation(webState);
  }
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  DCHECK_EQ(_webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    return;
  }

  // If the selected index changes as a result of the last webstate being
  // detached, atIndex will be kInvalidIndex.
  if (atIndex == WebStateList::kInvalidIndex) {
    [self.consumer selectItemWithID:nil];
    return;
  }

  [self.consumer selectItemWithID:newWebState->GetStableIdentifier()];
}

- (void)webStateList:(WebStateList*)webStateList
    didChangePinnedStateForWebState:(web::WebState*)webState
                            atIndex:(int)index {
  DCHECK_EQ(_webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    return;
  }

  if (IsPinnedTabsEnabled() && webStateList->IsWebStatePinnedAt(index)) {
    [self.consumer
        removeItemWithID:webState->GetStableIdentifier()
          selectedItemID:GetActiveWebStateIdentifier(
                             webStateList,
                             WebStateSearchCriteria{
                                 .pinned_state = PinnedState::kNonPinned,
                             })];

    _scopedWebStateObservation->RemoveObservation(webState);
  } else {
    TabSwitcherItem* item =
        [[WebStateTabSwitcherItem alloc] initWithWebState:webState];
    NSUInteger itemIndex = [self itemIndexFromWebStateListIndex:index];
    [self.consumer insertItem:item
                      atIndex:itemIndex
               selectedItemID:GetActiveWebStateIdentifier(
                                  webStateList,
                                  WebStateSearchCriteria{
                                      .pinned_state = PinnedState::kNonPinned,
                                  })];

    _scopedWebStateObservation->AddObservation(webState);
  }
}

- (void)webStateListWillBeginBatchOperation:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);
  _scopedWebStateObservation->RemoveAllObservations();
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);

  [self addWebStateObservations];
  [self populateConsumerItems];
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
  [self.consumer replaceItemID:webState->GetStableIdentifier() withItem:item];
}

#pragma mark - SnapshotCacheObserver

- (void)snapshotCache:(SnapshotCache*)snapshotCache
    didUpdateSnapshotForID:(NSString*)snapshotID {
  web::WebState* webState = nullptr;
  for (int i = self.webStateList->GetIndexOfFirstNonPinnedWebState();
       i < self.webStateList->count(); i++) {
    SnapshotTabHelper* snapshotTabHelper =
        SnapshotTabHelper::FromWebState(self.webStateList->GetWebStateAt(i));
    if ([snapshotID isEqualToString:snapshotTabHelper->GetSnapshotID()]) {
      webState = self.webStateList->GetWebStateAt(i);
      break;
    }
  }
  if (webState) {
    // It is possible to observe an updated snapshot for a WebState before
    // observing that the WebState has been added to the WebStateList. It is the
    // consumer's responsibility to ignore any updates before inserts.
    TabSwitcherItem* item =
        [[WebStateTabSwitcherItem alloc] initWithWebState:webState];
    // Items are indexed with the stable identifier. Don't pass a snapshot ID
    // here.
    [self.consumer replaceItemID:webState->GetStableIdentifier() withItem:item];
  }
}

#pragma mark - GridCommands

- (void)addNewItem {
  NSUInteger itemIndex =
      [self itemIndexFromWebStateListIndex:self.webStateList->count()];
  [self insertNewItemAtIndex:itemIndex];
}

- (void)insertNewItemAtIndex:(NSUInteger)index {
  [self insertNewItemAtIndex:index withURL:GURL(kChromeUINewTabURL)];
}

- (void)moveItemWithID:(NSString*)itemID toIndex:(NSUInteger)destinationIndex {
  int sourceIndex = GetTabIndex(self.webStateList,
                                WebStateSearchCriteria{
                                    .identifier = itemID,
                                    .pinned_state = PinnedState::kNonPinned,
                                });
  if (sourceIndex != WebStateList::kInvalidIndex) {
    int destinationWebStateListIndex =
        [self webStateListIndexFromItemIndex:destinationIndex];
    self.webStateList->MoveWebStateAt(sourceIndex,
                                      destinationWebStateListIndex);
  }
}

- (void)selectItemWithID:(NSString*)itemID {
  int index = GetTabIndex(self.webStateList, WebStateSearchCriteria{
                                                 .identifier = itemID,
                                             });
  WebStateList* itemWebStateList = self.webStateList;
  if (index == WebStateList::kInvalidIndex) {
    // If this is a search result, it may contain items from other windows or
    // from the inactive browser - check inactive browser and other windows
    // before giving up.
    BrowserList* browserList =
        BrowserListFactory::GetForBrowserState(self.browserState);
    Browser* browser = GetBrowserForTabWithId(
        browserList, itemID, self.browserState->IsOffTheRecord());

    if (!browser) {
      return;
    }

    if (browser->IsInactive()) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridOpenInactiveTabSearchResult"));
      index = itemWebStateList->count();
      MoveTabToBrowser(itemID, self.browser, index);
    } else {
      // Other windows case.
      itemWebStateList = browser->GetWebStateList();
      index = GetTabIndex(itemWebStateList,
                          WebStateSearchCriteria{
                              .identifier = itemID,
                              .pinned_state = PinnedState::kNonPinned,
                          });
      SceneState* targetSceneState =
          SceneStateBrowserAgent::FromBrowser(browser)->GetSceneState();
      SceneState* currentSceneState =
          SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();

      UISceneActivationRequestOptions* options =
          [[UISceneActivationRequestOptions alloc] init];
      options.requestingScene = currentSceneState.scene;

      [[UIApplication sharedApplication]
          requestSceneSessionActivation:targetSceneState.scene.session
                           userActivity:nil
                                options:options
                           errorHandler:^(NSError* error) {
                             LOG(ERROR) << base::SysNSStringToUTF8(
                                 error.localizedDescription);
                             NOTREACHED();
                           }];
    }
  }

  web::WebState* selectedWebState = itemWebStateList->GetWebStateAt(index);
  LogPriceDropMetrics(selectedWebState);

  base::TimeDelta timeSinceLastActivation =
      base::Time::Now() - selectedWebState->GetLastActiveTime();
  base::UmaHistogramCustomTimes(
      "IOS.TabGrid.TabSelected.TimeSinceLastActivation",
      timeSinceLastActivation, base::Minutes(1), base::Days(24), 50);

  // Don't attempt a no-op activation. Normally this is not an issue, but it's
  // possible that this method (-selectItemWithID:) is being called as part of
  // a WebStateListObserver callback, in which case even a no-op activation
  // will cause a CHECK().
  if (index == itemWebStateList->active_index()) {
    // In search mode the consumer doesn't have any information about the
    // selected item. So even if the active webstate is the same as the one that
    // is being selected, make sure that the consumer update its selected item.
    [self.consumer selectItemWithID:itemID];
    return;
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridMoveToExistingTab"));
  }

  // Avoid a reentrant activation. This is a fix for crbug.com/1134663, although
  // ignoring the slection at this point may do weird things.
  if (itemWebStateList->IsMutating()) {
    return;
  }

  // It should be safe to activate here.
  itemWebStateList->ActivateWebStateAt(index);
}

- (BOOL)isItemWithIDSelected:(NSString*)itemID {
  int index = GetTabIndex(self.webStateList,
                          WebStateSearchCriteria{.identifier = itemID});
  if (index == WebStateList::kInvalidIndex) {
    return NO;
  }
  return index == self.webStateList->active_index();
}

- (void)setPinState:(BOOL)pinState forItemWithIdentifier:(NSString*)identifier {
  SetWebStatePinnedState(self.webStateList, identifier, pinState);
}

- (void)closeItemWithID:(NSString*)itemID {
  int index = GetTabIndex(self.webStateList,
                          WebStateSearchCriteria{
                              .identifier = itemID,
                              .pinned_state = PinnedState::kNonPinned,
                          });
  if (index != WebStateList::kInvalidIndex) {
    self.webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
    return;
  }

  // `index` is `WebStateList::kInvalidIndex`, so `itemID` should be a search
  // result from a different window. Since this item is not from the current
  // browser, no UI updates will be sent to the current grid. Notify the current
  // grid consumer about the change.
  [self.consumer removeItemWithID:itemID selectedItemID:nil];
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridSearchCloseTabFromAnotherWindow"));

  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(self.browserState);
  Browser* browser = GetBrowserForTabWithId(
      browserList, itemID, self.browserState->IsOffTheRecord());

  // If this tab is still associated with another browser, remove it from the
  // associated web state list.
  if (browser) {
    WebStateList* itemWebStateList = browser->GetWebStateList();
    index = GetTabIndex(itemWebStateList,
                        WebStateSearchCriteria{
                            .identifier = itemID,
                            .pinned_state = PinnedState::kNonPinned,
                        });
    itemWebStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
  }
}

- (void)closeItemsWithIDs:(NSArray<NSString*>*)itemIDs {
  __block bool allTabsClosed = true;

  base::UmaHistogramCounts100("IOS.TabGrid.Selection.CloseTabs", itemIDs.count);
  RecordTabGridCloseTabsCount(itemIDs.count);

  self.webStateList->PerformBatchOperation(
      base::BindOnce(^(WebStateList* list) {
        for (NSString* itemID in itemIDs) {
          int index =
              GetTabIndex(list, WebStateSearchCriteria{
                                    .identifier = itemID,
                                    .pinned_state = PinnedState::kNonPinned,
                                });
          if (index != WebStateList::kInvalidIndex) {
            list->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
          }
        }

        allTabsClosed = list->empty();
      }));

  if (allTabsClosed) {
    if (!self.browserState->IsOffTheRecord()) {
      base::RecordAction(base::UserMetricsAction(
          "MobileTabGridSelectionCloseAllRegularTabsConfirmed"));
    } else {
      base::RecordAction(base::UserMetricsAction(
          "MobileTabGridSelectionCloseAllIncognitoTabsConfirmed"));
    }
  }
}

- (void)closeAllItems {
  RecordTabGridCloseTabsCount(self.webStateList->count());
  if (!self.browserState->IsOffTheRecord()) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridCloseAllRegularTabs"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridCloseAllIncognitoTabs"));
  }
  // This is a no-op if `webStateList` is already empty.
  self.webStateList->CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);
  SnapshotBrowserAgent::FromBrowser(self.browser)->RemoveAllSnapshots();
}

- (void)saveAndCloseAllItems {
  RecordTabGridCloseTabsCount(self.webStateList->count());
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridCloseAllRegularTabs"));

  if (self.webStateList->empty())
    return;

  int old_size =
      self.tabRestoreService ? self.tabRestoreService->entries().size() : 0;

  if (IsPinnedTabsEnabled()) {
    BOOL hasPinnedWebStatesOnly =
        self.webStateList->GetIndexOfFirstNonPinnedWebState() ==
        self.webStateList->count();

    if (hasPinnedWebStatesOnly) {
      return;
    }

    self.closedSessionWindow = SerializeWebStateList(self.webStateList);
    self.webStateList->CloseAllNonPinnedWebStates(
        WebStateList::CLOSE_USER_ACTION);
  } else {
    self.closedSessionWindow = SerializeWebStateList(self.webStateList);
    self.webStateList->CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);
  }

  self.syncedClosedTabsCount =
      self.tabRestoreService
          ? self.tabRestoreService->entries().size() - old_size
          : 0;
}

- (void)undoCloseAllItems {
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridUndoCloseAllRegularTabs"));
  if (!self.closedSessionWindow)
    return;
  SessionRestorationBrowserAgent::FromBrowser(self.browser)
      ->RestoreSessionWindow(self.closedSessionWindow,
                             SessionRestorationScope::kRegularOnly);
  self.closedSessionWindow = nil;
  [self removeEntriesFromTabRestoreService];
  self.syncedClosedTabsCount = 0;
}

- (void)discardSavedClosedItems {
  if (!self.closedSessionWindow)
    return;
  self.syncedClosedTabsCount = 0;
  self.closedSessionWindow = nil;
  SnapshotBrowserAgent::FromBrowser(self.browser)->RemoveAllSnapshots();
}

- (void)
    showCloseItemsConfirmationActionSheetWithItems:(NSArray<NSString*>*)items
                                            anchor:
                                                (UIBarButtonItem*)buttonAnchor {
  [self.delegate dismissPopovers];

  [self.delegate
      showCloseItemsConfirmationActionSheetWithTabGridMediator:self
                                                         items:items
                                                        anchor:buttonAnchor];
}

- (void)shareItems:(NSArray<NSString*>*)items
            anchor:(UIBarButtonItem*)buttonAnchor {
  [self.delegate dismissPopovers];

  NSMutableArray<URLWithTitle*>* URLs = [[NSMutableArray alloc] init];
  for (NSString* itemIdentifier in items) {
    TabItem* item = GetTabItem(self.webStateList,
                               WebStateSearchCriteria{
                                   .identifier = itemIdentifier,
                                   .pinned_state = PinnedState::kNonPinned,
                               });
    URLWithTitle* URL = [[URLWithTitle alloc] initWithURL:item.URL
                                                    title:item.title];
    [URLs addObject:URL];
  }
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridSelectionShareTabs"));
  base::UmaHistogramCounts100("IOS.TabGrid.Selection.ShareTabs", items.count);
  [self.delegate tabGridMediator:self shareURLs:URLs anchor:buttonAnchor];
}

- (NSArray<UIMenuElement*>*)addToButtonMenuElementsForItems:
    (NSArray<NSString*>*)items {
  if (!self.browser) {
    return nil;
  }

  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:MenuScenarioHistogram::kTabGridAddTo];

  __weak TabGridMediator* weakSelf = self;

  UIAction* bookmarkAction = [actionFactory actionToBookmarkWithBlock:^{
    [weakSelf addItemsToBookmarks:items];
  }];
  // Bookmarking can be disabled from prefs (from an enterprise policy),
  // if that's the case grey out the option in the menu.
  BOOL isEditBookmarksEnabled =
      self.browser->GetBrowserState()->GetPrefs()->GetBoolean(
          bookmarks::prefs::kEditBookmarksEnabled);
  if (!isEditBookmarksEnabled)
    bookmarkAction.attributes = UIMenuElementAttributesDisabled;

  return @[
    [actionFactory actionToAddToReadingListWithBlock:^{
      [weakSelf addItemsToReadingList:items];
    }],
    bookmarkAction
  ];
}

- (void)searchItemsWithText:(NSString*)searchText {
  TabsSearchService* searchService =
      TabsSearchServiceFactory::GetForBrowserState(self.browserState);
  const std::u16string& searchTerm = base::SysNSStringToUTF16(searchText);
  searchService->Search(
      searchTerm,
      base::BindOnce(^(
          std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
        NSMutableArray* currentBrowserItems = [[NSMutableArray alloc] init];
        NSMutableArray* remainingItems = [[NSMutableArray alloc] init];
        for (const TabsSearchService::TabsSearchBrowserResults& browserResults :
             results) {
          for (web::WebState* webState : browserResults.web_states) {
            TabSwitcherItem* item =
                [[WebStateTabSwitcherItem alloc] initWithWebState:webState];
            if (browserResults.browser == self.browser) {
              [currentBrowserItems addObject:item];
            } else {
              [remainingItems addObject:item];
            }
          }
        }

        NSArray* allItems = nil;
        // If there are results from Browsers other than the current one,
        // append those results to the end.
        if (remainingItems.count) {
          allItems = [currentBrowserItems
              arrayByAddingObjectsFromArray:remainingItems];
        } else {
          allItems = currentBrowserItems;
        }
        [self.consumer populateItems:allItems selectedItemID:nil];
      }));
}

- (void)resetToAllItems {
  [self populateConsumerItems];
}

- (void)fetchSearchHistoryResultsCountForText:(NSString*)searchText
                                   completion:(void (^)(size_t))completion {
  TabsSearchService* search_service =
      TabsSearchServiceFactory::GetForBrowserState(self.browserState);
  const std::u16string& searchTerm = base::SysNSStringToUTF16(searchText);
  search_service->SearchHistory(searchTerm,
                                base::BindOnce(^(size_t resultCount) {
                                  completion(resultCount);
                                }));
}

#pragma mark GridCommands helpers

- (void)insertNewItemAtIndex:(NSUInteger)index withURL:(const GURL&)newTabURL {
  // The incognito mediator's Browser is briefly set to nil after the last
  // incognito tab is closed.  This occurs because the incognito BrowserState
  // needs to be destroyed to correctly clear incognito browsing data.  Don't
  // attempt to create a new WebState with a nil BrowserState.
  if (!self.browser)
    return;

  // There are some circumstances where a new tab insertion can be erroniously
  // triggered while another web state list mutation is happening. To ensure
  // those bugs don't become crashes, check that the web state list is OK to
  // mutate.
  if (self.webStateList->IsMutating()) {
    // Shouldn't have happened!
    DCHECK(false) << "Reentrant web state insertion!";
    return;
  }

  DCHECK(self.browserState);
  web::WebState::CreateParams params(self.browserState);
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);

  web::NavigationManager::WebLoadParams loadParams(newTabURL);
  loadParams.transition_type = ui::PAGE_TRANSITION_TYPED;
  webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  int webStateListIndex = [self webStateListIndexFromItemIndex:index];

  self.webStateList->InsertWebState(
      base::checked_cast<int>(webStateListIndex), std::move(webState),
      (WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE),
      WebStateOpener());
}

#pragma mark - TabCollectionDragDropHandler

- (UIDragItem*)dragItemForItemWithID:(NSString*)itemID {
  web::WebState* webState = GetWebState(
      self.webStateList, WebStateSearchCriteria{
                             .identifier = itemID,
                             .pinned_state = PinnedState::kNonPinned,
                         });
  return CreateTabDragItem(webState);
}

- (void)dragWillBeginForItemWithID:(NSString*)itemID {
  _dragItemID = [itemID copy];
}

- (void)dragSessionDidEnd {
  _dragItemID = nil;
}

- (UIDropOperation)dropOperationForDropSession:(id<UIDropSession>)session {
  UIDragItem* dragItem = session.localDragSession.items.firstObject;

  // Tab move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabInfo class]]) {
    TabInfo* tabInfo = static_cast<TabInfo*>(dragItem.localObject);
    // If the dropped tab is from the same Chrome window and has been removed,
    // cancel the drop operation.
    if (_dragItemID == tabInfo.tabID &&
        GetWebStateIndex(self.webStateList,
                         WebStateSearchCriteria{
                             .identifier = tabInfo.tabID,
                             .pinned_state = PinnedState::kNonPinned,
                         }) == WebStateList::kInvalidIndex) {
      return UIDropOperationCancel;
    }
    if (self.browserState->IsOffTheRecord() && tabInfo.incognito) {
      return UIDropOperationMove;
    }
    if (!self.browserState->IsOffTheRecord() && !tabInfo.incognito) {
      return UIDropOperationMove;
    }
    // Tabs of different profiles (regular/incognito) cannot be dropped.
    return UIDropOperationForbidden;
  }

  // All URLs originating from Chrome create a new tab (as opposed to moving a
  // tab).
  if ([dragItem.localObject isKindOfClass:[NSURL class]]) {
    return UIDropOperationCopy;
  }

  // URLs are accepted when drags originate from outside Chrome.
  NSArray<NSString*>* acceptableTypes = @[ UTTypeURL.identifier ];
  if ([session hasItemsConformingToTypeIdentifiers:acceptableTypes]) {
    return UIDropOperationCopy;
  }

  // Other UTI types such as image data or file data cannot be dropped.
  return UIDropOperationForbidden;
}

- (void)dropItem:(UIDragItem*)dragItem
               toIndex:(NSUInteger)destinationIndex
    fromSameCollection:(BOOL)fromSameCollection {
  // Tab move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabInfo class]]) {
    TabInfo* tabInfo = static_cast<TabInfo*>(dragItem.localObject);
    if (!fromSameCollection) {
      // Try to unpin the tab. If the returned index is invalid that means the
      // tab lives in another Browser.
      int tabIndex = WebStateList::kInvalidIndex;
      if (IsPinnedTabsEnabled()) {
        tabIndex = SetWebStatePinnedState(self.webStateList, tabInfo.tabID,
                                          /*pin_state=*/NO);
      }
      if (tabIndex == WebStateList::kInvalidIndex) {
        // Move tab across Browsers.
        base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                      DragItemOrigin::kOtherBrwoser);
        MoveTabToBrowser(tabInfo.tabID, self.browser, destinationIndex);
        return;
      }
      base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                    DragItemOrigin::kSameBrowser);
    } else {
      base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                    DragItemOrigin::kSameCollection);
    }

    // Reorder tab within same grid.
    [self moveItemWithID:tabInfo.tabID toIndex:destinationIndex];
    return;
  }
  base::UmaHistogramEnumeration(kUmaGridViewDragOrigin, DragItemOrigin::kOther);

  // Handle URLs from within Chrome synchronously using a local object.
  if ([dragItem.localObject isKindOfClass:[URLInfo class]]) {
    URLInfo* droppedURL = static_cast<URLInfo*>(dragItem.localObject);
    [self insertNewItemAtIndex:destinationIndex withURL:droppedURL.URL];
    return;
  }
}

- (void)dropItemFromProvider:(NSItemProvider*)itemProvider
                     toIndex:(NSUInteger)destinationIndex
          placeholderContext:
              (id<UICollectionViewDropPlaceholderContext>)placeholderContext {
  if (![itemProvider canLoadObjectOfClass:[NSURL class]]) {
    [placeholderContext deletePlaceholder];
    return;
  }

  auto loadHandler = ^(__kindof id<NSItemProviderReading> providedItem,
                       NSError* error) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [placeholderContext deletePlaceholder];
      NSURL* droppedURL = static_cast<NSURL*>(providedItem);
      [self insertNewItemAtIndex:destinationIndex
                         withURL:net::GURLWithNSURL(droppedURL)];
    });
  };
  [itemProvider loadObjectOfClass:[NSURL class] completionHandler:loadHandler];
}

#pragma mark - GridShareableItemsProvider

- (BOOL)isItemWithIdentifierSharable:(NSString*)identifier {
  web::WebState* webState = GetWebState(
      self.webStateList, WebStateSearchCriteria{
                             .identifier = identifier,
                             .pinned_state = PinnedState::kNonPinned,
                         });
  const GURL& URL = webState->GetVisibleURL();
  return URL.is_valid() && URL.SchemeIsHTTPOrHTTPS();
}

#pragma mark - Private

// Calls `-populateItems:selectedItemID:` on the consumer.
- (void)populateConsumerItems {
  [self.consumer populateItems:CreateItems(self.webStateList)
                selectedItemID:GetActiveWebStateIdentifier(
                                   self.webStateList,
                                   WebStateSearchCriteria{
                                       .pinned_state = PinnedState::kNonPinned,
                                   })];
}

// Adds an observations to every non-pinned WebState.
- (void)addWebStateObservations {
  int firstIndex = IsPinnedTabsEnabled()
                       ? self.webStateList->GetIndexOfFirstNonPinnedWebState()
                       : 0;
  for (int i = firstIndex; i < self.webStateList->count(); i++) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    _scopedWebStateObservation->AddObservation(webState);
  }
}

// Removes `self.syncedClosedTabsCount` most recent entries from the
// TabRestoreService.
- (void)removeEntriesFromTabRestoreService {
  if (!self.tabRestoreService) {
    return;
  }
  std::vector<SessionID> identifiers;
  auto iter = self.tabRestoreService->entries().begin();
  auto end = self.tabRestoreService->entries().end();
  for (int i = 0; i < self.syncedClosedTabsCount && iter != end; i++) {
    identifiers.push_back(iter->get()->id);
    iter++;
  }
  for (const SessionID sessionID : identifiers) {
    self.tabRestoreService->RemoveTabEntryById(sessionID);
  }
}

// Returns a SnapshotCache for the current browser.
- (SnapshotCache*)snapshotCache {
  if (!self.browser)
    return nil;
  return SnapshotBrowserAgent::FromBrowser(self.browser)->snapshot_cache();
}

- (void)addItemsToReadingList:(NSArray<NSString*>*)items {
  [self.delegate dismissPopovers];

  base::UmaHistogramCounts100("IOS.TabGrid.Selection.AddToReadingList",
                              items.count);

  NSArray<URLWithTitle*>* URLs = [self urlsWithTitleFromItemIDs:items];

  ReadingListAddCommand* command =
      [[ReadingListAddCommand alloc] initWithURLs:URLs];
  ReadingListBrowserAgent* readingListBrowserAgent =
      ReadingListBrowserAgent::FromBrowser(self.browser);
  readingListBrowserAgent->AddURLsToReadingList(command.URLs);
}

- (void)addItemsToBookmarks:(NSArray<NSString*>*)items {
  id<BookmarksCommands> bookmarkHandler =
      HandlerForProtocol(_browser->GetCommandDispatcher(), BookmarksCommands);

  if (!bookmarkHandler) {
    return;
  }
  [self.delegate dismissPopovers];
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridAddedMultipleNewBookmarks"));
  base::UmaHistogramCounts100("IOS.TabGrid.Selection.AddToBookmarks",
                              items.count);

  NSArray<URLWithTitle*>* URLs = [self urlsWithTitleFromItemIDs:items];

  [bookmarkHandler bookmarkWithFolderChooser:URLs];
}

- (NSArray<URLWithTitle*>*)urlsWithTitleFromItemIDs:(NSArray<NSString*>*)items {
  NSMutableArray<URLWithTitle*>* URLs = [[NSMutableArray alloc] init];
  for (NSString* itemIdentifier in items) {
    TabItem* item = GetTabItem(self.webStateList,
                               WebStateSearchCriteria{
                                   .identifier = itemIdentifier,
                                   .pinned_state = PinnedState::kNonPinned,
                               });
    URLWithTitle* URL = [[URLWithTitle alloc] initWithURL:item.URL
                                                    title:item.title];
    [URLs addObject:URL];
  }
  return URLs;
}

// Converts the WebStateList indexes to the collection view's item indexes by
// shifting indexes by the number of pinned WebStates.
- (NSUInteger)itemIndexFromWebStateListIndex:(int)index {
  if (!IsPinnedTabsEnabled()) {
    return index;
  }

  // If WebStateList's index is invalid or it's inside of pinned WebStates
  // range return invalid index.
  if (index == WebStateList::kInvalidIndex ||
      index < self.webStateList->GetIndexOfFirstNonPinnedWebState()) {
    return NSNotFound;
  }

  return index - self.webStateList->GetIndexOfFirstNonPinnedWebState();
}

// Converts the collection view's item indexes to the WebStateList indexes by
// shifting indexes by the number of pinned WebStates.
- (int)webStateListIndexFromItemIndex:(NSUInteger)index {
  if (!IsPinnedTabsEnabled()) {
    return index;
  }

  if (index == NSNotFound) {
    return WebStateList::kInvalidIndex;
  }

  return index + self.webStateList->GetIndexOfFirstNonPinnedWebState();
}

- (NSSet<NSString*>*)nonPinnedWebStates {
  NSMutableSet<NSString*>* nonPinnedWebStates = [NSMutableSet set];

  for (int index = self.webStateList->GetIndexOfFirstNonPinnedWebState();
       index < self.webStateList->count(); ++index) {
    web::WebState* webState = self.webStateList->GetWebStateAt(index);
    if (webState != nil && webState->GetStableIdentifier() != nil) {
      [nonPinnedWebStates addObject:webState->GetStableIdentifier()];
    }
  }

  return [nonPinnedWebStates copy];
}

@end
