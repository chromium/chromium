// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_mediator.h"

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/scoped_multi_source_observation.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/drag_and_drop/drag_item_util.h"
#import "ios/chrome/browser/main/browser_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/mac/url_conversions.h"

using PinnedState = WebStateSearchCriteria::PinnedState;

namespace {

// Constructs an array of TabSwitcherItems from a `web_state_list`.
NSArray<TabSwitcherItem*>* CreatePinnedTabConsumerItems(
    WebStateList* web_state_list) {
  NSMutableArray<TabSwitcherItem*>* items = [[NSMutableArray alloc] init];
  int pinnedWebStatesCount = web_state_list->GetIndexOfFirstNonPinnedWebState();

  for (int i = 0; i < pinnedWebStatesCount; i++) {
    DCHECK(web_state_list->IsWebStatePinnedAt(i));

    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    [items
        addObject:[[WebStateTabSwitcherItem alloc] initWithWebState:web_state]];
  }
  return items;
}

}  // namespace

@interface PinnedTabsMediator () <CRWWebStateObserver, WebStateListObserving>

// The list from the browser.
@property(nonatomic, assign) WebStateList* webStateList;
// The browser state from the browser.
@property(nonatomic, readonly) ChromeBrowserState* browserState;
// The UI consumer to which updates are made.
@property(nonatomic, weak) id<TabCollectionConsumer> consumer;

@end

@implementation PinnedTabsMediator {
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
    DCHECK(IsPinnedTabsEnabled());
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

#pragma mark - Public properties

- (void)setBrowser:(Browser*)browser {
  _scopedWebStateListObservation->RemoveAllObservations();
  _scopedWebStateObservation->RemoveAllObservations();

  _browser = browser;

  _webStateList = browser ? browser->GetWebStateList() : nullptr;
  _browserState = browser ? browser->GetBrowserState() : nullptr;

  if (_webStateList) {
    _scopedWebStateListObservation->AddObservation(_webStateList);

    [self addWebStateObservations];
    [self populateConsumerItems];
  }
}

#pragma mark - WebStateListObserving

- (void)willChangeWebStateList:(WebStateList*)webStateList
                        change:(const WebStateListChangeDetach&)detachChange
                        status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    return;
  }

  if (!webStateList) {
    return;
  }

  if (!webStateList->IsWebStatePinnedAt(status.index)) {
    [self.consumer
        selectItemWithID:GetActiveWebStateIdentifier(
                             webStateList,
                             WebStateSearchCriteria{
                                 .pinned_state = PinnedState::kPinned,
                             })];
    return;
  }

  web::WebState* detachedWebState = detachChange.detached_web_state();
  [self.consumer removeItemWithID:detachedWebState->GetStableIdentifier()
                   selectedItemID:GetActiveWebStateIdentifier(
                                      webStateList,
                                      WebStateSearchCriteria{
                                          .pinned_state = PinnedState::kPinned,
                                      })];

  _scopedWebStateObservation->RemoveObservation(detachedWebState);
}

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    return;
  }

  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly: {
      const WebStateListChangeStatusOnly& selectionOnlyChange =
          change.As<WebStateListChangeStatusOnly>();
      if (status.pinned_state_change) {
        [self changePinnedStateForWebState:selectionOnlyChange
                                               .selected_web_state()
                                   atIndex:status.index];
        break;
      }
      // The activation is handled after this switch statement.
      break;
    }
    case WebStateListChange::Type::kDetach:
      // Do nothing when a WebState is detached.
      break;
    case WebStateListChange::Type::kMove: {
      const WebStateListChangeMove& moveChange =
          change.As<WebStateListChangeMove>();
      if (webStateList->IsWebStatePinnedAt(status.index)) {
        // PinnedTabsMediator handles only pinned tabs because non pinned tabs
        // are handled in BaseGridMediator.
        [self.consumer
            moveItemWithID:moveChange.moved_web_state()->GetStableIdentifier()
                   toIndex:status.index];
      }

      // The pinned state can be updated when a tab is moved.
      if (status.pinned_state_change) {
        [self changePinnedStateForWebState:moveChange.moved_web_state()
                                   atIndex:status.index];
      }
      break;
    }
    case WebStateListChange::Type::kReplace: {
      if (!webStateList->IsWebStatePinnedAt(status.index)) {
        break;
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
      if (!webStateList->IsWebStatePinnedAt(status.index)) {
        [self.consumer
            selectItemWithID:GetActiveWebStateIdentifier(
                                 webStateList,
                                 WebStateSearchCriteria{
                                     .pinned_state = PinnedState::kPinned,
                                 })];
        break;
      }

      const WebStateListChangeInsert& insertChange =
          change.As<WebStateListChangeInsert>();
      web::WebState* insertedWebState = insertChange.inserted_web_state();
      TabSwitcherItem* item =
          [[WebStateTabSwitcherItem alloc] initWithWebState:insertedWebState];
      [self.consumer insertItem:item
                        atIndex:status.index
                 selectedItemID:GetActiveWebStateIdentifier(
                                    webStateList,
                                    WebStateSearchCriteria{
                                        .pinned_state = PinnedState::kPinned,
                                    })];

      _scopedWebStateObservation->AddObservation(insertedWebState);
      break;
    }
  }

  if (status.active_web_state_change()) {
    // If the selected index changes as a result of the last webstate being
    // detached, the active index will be kInvalidIndex.
    if (webStateList->active_index() == WebStateList::kInvalidIndex) {
      [self.consumer selectItemWithID:nil];
      return;
    }

    if (!webStateList->IsWebStatePinnedAt(webStateList->active_index())) {
      [self.consumer selectItemWithID:nil];
      return;
    }

    [self.consumer
        selectItemWithID:status.new_active_web_state->GetStableIdentifier()];
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

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);

  _scopedWebStateListObservation.reset();
  _webStateList = nullptr;
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

#pragma mark - TabCollectionCommands

- (void)selectItemWithID:(NSString*)itemID {
  base::RecordAction(base::UserMetricsAction("MobileTabGridPinnedTabSelected"));

  int index = GetWebStateIndex(self.webStateList,
                               WebStateSearchCriteria{
                                   .identifier = itemID,
                                   .pinned_state = PinnedState::kPinned,
                               });
  WebStateList* itemWebStateList = self.webStateList;

  if (index == WebStateList::kInvalidIndex) {
    return;
  }

  web::WebState* selectedWebState = itemWebStateList->GetWebStateAt(index);

  base::TimeDelta timeSinceLastActivation =
      base::Time::Now() - selectedWebState->GetLastActiveTime();
  base::UmaHistogramCustomTimes(
      "IOS.TabGrid.TabSelected.TimeSinceLastActivation",
      timeSinceLastActivation, base::Minutes(1), base::Days(24), 50);

  if (index != itemWebStateList->active_index()) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridMoveToExistingTab"));
  }

  itemWebStateList->ActivateWebStateAt(index);

  LogPinnedTabsUsedForDefaultBrowserPromo();
}

- (void)closeItemWithID:(NSString*)itemID {
  int index = GetWebStateIndex(self.webStateList,
                               WebStateSearchCriteria{
                                   .identifier = itemID,
                                   .pinned_state = PinnedState::kPinned,
                               });
  if (index == WebStateList::kInvalidIndex) {
    return;
  }

  self.webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
}

- (void)setPinState:(BOOL)pinState forItemWithIdentifier:(NSString*)identifier {
  SetWebStatePinnedState(self.webStateList, identifier, pinState);
}

- (void)moveItemWithID:(NSString*)itemID toIndex:(NSUInteger)destinationIndex {
  int sourceIndex = GetWebStateIndex(self.webStateList,
                                     WebStateSearchCriteria{
                                         .identifier = itemID,
                                         .pinned_state = PinnedState::kPinned,
                                     });
  if (sourceIndex != WebStateList::kInvalidIndex) {
    int destinationWebStateListIndex =
        [self webStateListIndexFromItemIndex:destinationIndex];
    self.webStateList->MoveWebStateAt(sourceIndex,
                                      destinationWebStateListIndex);
  }
}

#pragma mark - TabCollectionDragDropHandler

- (UIDragItem*)dragItemForItemWithID:(NSString*)itemID {
  web::WebState* webState =
      GetWebState(self.webStateList, WebStateSearchCriteria{
                                         .identifier = itemID,
                                         .pinned_state = PinnedState::kPinned,
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
    return [self dropOperationForTabInfo:tabInfo];
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
      // Try to pin the tab. If the returned index is invalid that means the
      // tab lives in another Browser.
      int tabIndex = SetWebStatePinnedState(self.webStateList, tabInfo.tabID,
                                            /*pin_state=*/YES);
      if (tabIndex == WebStateList::kInvalidIndex) {
        BrowserList* browserList =
            BrowserListFactory::GetForBrowserState(self.browserState);
        BrowserAndIndex tabBrowserAndIndex = FindBrowserAndIndex(
            tabInfo.tabID, browserList->AllRegularBrowsers());
        if (!tabBrowserAndIndex.browser) {
          // This could happen if the tab is deleted during a drag-and-drop
          // action.
          return;
        }

        // Move tab across Browsers.
        base::UmaHistogramEnumeration(kUmaPinnedViewDragOrigin,
                                      DragItemOrigin::kOtherBrwoser);
        MoveTabToBrowser(tabInfo.tabID, self.browser, destinationIndex,
                         WebStateList::INSERT_PINNED);
        return;
      }
      base::UmaHistogramEnumeration(kUmaPinnedViewDragOrigin,
                                    DragItemOrigin::kSameBrowser);
    } else {
      base::UmaHistogramEnumeration(kUmaPinnedViewDragOrigin,
                                    DragItemOrigin::kSameCollection);
    }

    // Reorder tabs.
    [self.consumer moveItemWithID:tabInfo.tabID toIndex:destinationIndex];
    return;
  }
  base::UmaHistogramEnumeration(kUmaPinnedViewDragOrigin,
                                DragItemOrigin::kOther);

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

  __weak __typeof(self) weakSelf = self;
  auto loadHandler =
      ^(__kindof id<NSItemProviderReading> providedItem, NSError* error) {
        dispatch_async(dispatch_get_main_queue(), ^{
          [placeholderContext deletePlaceholder];
          NSURL* droppedURL = static_cast<NSURL*>(providedItem);
          [weakSelf insertNewItemAtIndex:destinationIndex
                                 withURL:net::GURLWithNSURL(droppedURL)];
        });
      };
  [itemProvider loadObjectOfClass:[NSURL class] completionHandler:loadHandler];
}

#pragma mark - Private

- (void)addWebStateObservations {
  int pinnedWebStatesCount = _webStateList->GetIndexOfFirstNonPinnedWebState();

  for (int i = 0; i < pinnedWebStatesCount; i++) {
    DCHECK(_webStateList->IsWebStatePinnedAt(i));

    web::WebState* webState = _webStateList->GetWebStateAt(i);
    _scopedWebStateObservation->AddObservation(webState);
  }
}

- (void)populateConsumerItems {
  [self.consumer populateItems:CreatePinnedTabConsumerItems(self.webStateList)
                selectedItemID:GetActiveWebStateIdentifier(
                                   self.webStateList,
                                   WebStateSearchCriteria{
                                       .pinned_state = PinnedState::kPinned,
                                   })];
}

// Returns the `UIDropOperation` corresponding to the given `tabInfo`.
- (UIDropOperation)dropOperationForTabInfo:(TabInfo*)tabInfo {
  // If the dropped tab is from the same Chrome window and has been removed,
  // cancel the drop operation.
  if (_dragItemID == tabInfo.tabID) {
    const BOOL tabExists =
        GetWebStateIndex(self.webStateList,
                         WebStateSearchCriteria{
                             .identifier = tabInfo.tabID,
                             .pinned_state = PinnedState::kPinned,
                         }) != WebStateList::kInvalidIndex;
    if (!tabExists) {
      return UIDropOperationCancel;
    }
  }

  if (tabInfo.incognito) {
    return UIDropOperationForbidden;
  }

  return UIDropOperationMove;
}

// Inserts a new item with the given`newTabURL` at `index`.
- (void)insertNewItemAtIndex:(NSUInteger)index withURL:(const GURL&)newTabURL {
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

  // Insert a new webState using the `INSERT_PINNED` flag and activate it.
  self.webStateList->InsertWebState(
      base::checked_cast<int>(index), std::move(webState),
      (WebStateList::INSERT_PINNED | WebStateList::INSERT_ACTIVATE),
      WebStateOpener());
}

// Converts the collection view's item index to WebStateList index.
// Returns `kInvalidIndex` if `index` is out of range.
- (int)webStateListIndexFromItemIndex:(NSUInteger)index {
  if (index == NSNotFound) {
    return WebStateList::kInvalidIndex;
  }

  int webStateListIndex = index;
  int webStateListLastIndex =
      self.webStateList->GetIndexOfFirstNonPinnedWebState() - 1;

  if (webStateListIndex > webStateListLastIndex) {
    return WebStateList::kInvalidIndex;
  }

  return webStateListIndex;
}

// Inserts/removes a pinned item to/from the collection.
- (void)changePinnedStateForWebState:(web::WebState*)webState
                             atIndex:(int)index {
  if (self.webStateList->IsWebStatePinnedAt(index)) {
    TabSwitcherItem* item =
        [[WebStateTabSwitcherItem alloc] initWithWebState:webState];
    [self.consumer insertItem:item
                      atIndex:index
               selectedItemID:GetActiveWebStateIdentifier(
                                  self.webStateList,
                                  WebStateSearchCriteria{
                                      .pinned_state = PinnedState::kPinned,
                                  })];

    _scopedWebStateObservation->AddObservation(webState);
  } else {
    [self.consumer
        removeItemWithID:webState->GetStableIdentifier()
          selectedItemID:GetActiveWebStateIdentifier(
                             self.webStateList,
                             WebStateSearchCriteria{
                                 .pinned_state = PinnedState::kPinned,
                             })];

    _scopedWebStateObservation->RemoveObservation(webState);
  }
}

@end
