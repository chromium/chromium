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
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/pinned_tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/apple/url_conversions.h"

using PinnedState = WebStateSearchCriteria::PinnedState;

namespace {

// Constructs an array of TabSwitcherItems from a `web_state_list`.
NSArray<TabSwitcherItem*>* CreatePinnedTabConsumerItems(
    WebStateList* web_state_list) {
  NSMutableArray<TabSwitcherItem*>* items = [[NSMutableArray alloc] init];
  int pinnedWebStatesCount = web_state_list->pinned_tabs_count();

  for (int i = 0; i < pinnedWebStatesCount; i++) {
    DCHECK(web_state_list->IsWebStatePinnedAt(i));

    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    [items addObject:[[PinnedItem alloc] initWithWebState:web_state]];
  }
  return items;
}

// Returns the identifier of the currently active pinned tab.
web::WebStateID GetActivePinnedTabID(WebStateList* web_state_list) {
  web::WebState* active_web_state =
      GetActiveWebState(web_state_list, PinnedState::kPinned);
  if (!active_web_state) {
    return web::WebStateID();
  }
  return active_web_state->GetUniqueIdentifier();
}

}  // namespace

@interface PinnedTabsMediator () <CRWWebStateObserver, WebStateListObserving>

// The list from the browser.
@property(nonatomic, assign) WebStateList* webStateList;
// The UI consumer to which updates are made.
@property(nonatomic, weak) id<PinnedTabCollectionConsumer> consumer;

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

  // URL loader to open tabs when needed.
  raw_ptr<UrlLoadingBrowserAgent> _URLLoader;
}

- (instancetype)initWithConsumer:(id<PinnedTabCollectionConsumer>)consumer {
  if ((self = [super init])) {
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
  _URLLoader = browser ? UrlLoadingBrowserAgent::FromBrowser(browser) : nullptr;

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

  if (!webStateList->IsWebStatePinnedAt(detachChange.detached_from_index())) {
    [self.consumer selectItemWithID:GetActivePinnedTabID(webStateList)];
    return;
  }

  web::WebState* detachedWebState = detachChange.detached_web_state();
  [self.consumer removeItemWithID:detachedWebState->GetUniqueIdentifier()
                   selectedItemID:GetActivePinnedTabID(webStateList)];

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
      if (selectionOnlyChange.pinned_state_changed()) {
        [self changePinnedStateForWebState:selectionOnlyChange.web_state()
                                   atIndex:selectionOnlyChange.index()];
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

      if (moveChange.pinned_state_changed()) {
        // The pinned state can be updated when a tab is moved.
        [self changePinnedStateForWebState:moveChange.moved_web_state()
                                   atIndex:moveChange.moved_to_index()];
      } else if (webStateList->IsWebStatePinnedAt(
                     moveChange.moved_to_index())) {
        // PinnedTabsMediator handles only pinned tabs because non pinned tabs
        // are handled in BaseGridMediator.
        [self.consumer
            moveItemWithID:moveChange.moved_web_state()->GetUniqueIdentifier()
                   toIndex:moveChange.moved_to_index()];
      }
      break;
    }
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replaceChange =
          change.As<WebStateListChangeReplace>();
      if (!webStateList->IsWebStatePinnedAt(replaceChange.index())) {
        break;
      }
      web::WebState* replacedWebState = replaceChange.replaced_web_state();
      web::WebState* insertedWebState = replaceChange.inserted_web_state();
      TabSwitcherItem* newItem =
          [[PinnedItem alloc] initWithWebState:insertedWebState];
      [self.consumer replaceItemID:replacedWebState->GetUniqueIdentifier()
                          withItem:newItem];

      _scopedWebStateObservation->RemoveObservation(replacedWebState);
      _scopedWebStateObservation->AddObservation(insertedWebState);
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insertChange =
          change.As<WebStateListChangeInsert>();
      if (!webStateList->IsWebStatePinnedAt(insertChange.index())) {
        [self.consumer selectItemWithID:GetActivePinnedTabID(webStateList)];
        break;
      }
      web::WebState* insertedWebState = insertChange.inserted_web_state();
      TabSwitcherItem* item =
          [[PinnedItem alloc] initWithWebState:insertedWebState];
      [self.consumer insertItem:item
                        atIndex:insertChange.index()
                 selectedItemID:GetActivePinnedTabID(webStateList)];

      _scopedWebStateObservation->AddObservation(insertedWebState);
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created. Grouped tabs can never be pinned.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated. Grouped can
      // never be pinned.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved. Grouped tabs can never be pinned.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted. Grouped tabs can never be pinned.
      break;
  }

  if (status.active_web_state_change()) {
    // If the selected index changes as a result of the last webstate being
    // detached, the active index will be kInvalidIndex.
    if (webStateList->active_index() == WebStateList::kInvalidIndex) {
      [self.consumer selectItemWithID:web::WebStateID()];
      return;
    }

    if (!webStateList->IsWebStatePinnedAt(webStateList->active_index())) {
      [self.consumer selectItemWithID:web::WebStateID()];
      return;
    }

    [self.consumer
        selectItemWithID:status.new_active_web_state->GetUniqueIdentifier()];
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
  TabSwitcherItem* item = [[PinnedItem alloc] initWithWebState:webState];
  [self.consumer replaceItemID:webState->GetUniqueIdentifier() withItem:item];
}

#pragma mark - TabCollectionDragDropHandler

- (UIDragItem*)dragItemForItem:(TabSwitcherItem*)item {
  web::WebState* webState =
      GetWebState(self.webStateList, WebStateSearchCriteria{
                                         .identifier = item.identifier,
                                         .pinned_state = PinnedState::kPinned,
                                     });

  return CreateTabDragItem(webState);
}

- (UIDropOperation)dropOperationForDropSession:(id<UIDropSession>)session
                                       toIndex:(NSUInteger)destinationIndex {
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
  WebStateList* webStateList = self.webStateList;

  // Tab move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabInfo class]]) {
    TabInfo* tabInfo = static_cast<TabInfo*>(dragItem.localObject);

    // Try to pin the tab, if pinned nothing happens.
    SetWebStatePinnedState(webStateList, tabInfo.tabID,
                           /*pin_state=*/true);

    int sourceWebStateIndex =
        GetWebStateIndex(webStateList, WebStateSearchCriteria{
                                           .identifier = tabInfo.tabID,
                                           .pinned_state = PinnedState::kPinned,
                                       });

    if (sourceWebStateIndex == WebStateList::kInvalidIndex) {
      // Move tab across Browsers.
      base::UmaHistogramEnumeration(kUmaPinnedViewDragOrigin,
                                    DragItemOrigin::kOtherBrowser);
      const WebStateList::InsertionParams params =
          WebStateList::InsertionParams::AtIndex(destinationIndex).Pinned();
      MoveTabToBrowser(tabInfo.tabID, self.browser, params);
      return;
    }

    if (fromSameCollection) {
      base::UmaHistogramEnumeration(kUmaPinnedViewDragOrigin,
                                    DragItemOrigin::kSameCollection);
    } else {
      base::UmaHistogramEnumeration(kUmaPinnedViewDragOrigin,
                                    DragItemOrigin::kSameBrowser);
    }

    // Reorder tabs.
    const auto insertionParams =
        WebStateList::InsertionParams::AtIndex(destinationIndex);
    MoveWebStateWithIdentifierToInsertionParams(
        tabInfo.tabID, insertionParams, webStateList, fromSameCollection);
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
  base::UmaHistogramEnumeration(kUmaPinnedViewDragOrigin,
                                DragItemOrigin::kOther);

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
  int pinnedWebStatesCount = _webStateList->pinned_tabs_count();

  for (int i = 0; i < pinnedWebStatesCount; i++) {
    DCHECK(_webStateList->IsWebStatePinnedAt(i));

    web::WebState* webState = _webStateList->GetWebStateAt(i);
    _scopedWebStateObservation->AddObservation(webState);
  }
}

- (void)populateConsumerItems {
  [self.consumer populateItems:CreatePinnedTabConsumerItems(self.webStateList)
                selectedItemID:GetActivePinnedTabID(self.webStateList)];
}

// Returns the `UIDropOperation` corresponding to the given `tabInfo`.
- (UIDropOperation)dropOperationForTabInfo:(TabInfo*)tabInfo {
  if (tabInfo.incognito) {
    return UIDropOperationForbidden;
  }

  return UIDropOperationMove;
}

// Inserts a new item with the given`newTabURL` at `index`.
- (void)insertNewItemAtIndex:(NSUInteger)index withURL:(const GURL&)newTabURL {
  // There are some circumstances where a new tab insertion can be erroneously
  // triggered while another web state list mutation is happening. To ensure
  // those bugs don't become crashes, check that the web state list is OK to
  // mutate.
  if (self.webStateList->IsMutating()) {
    // Shouldn't have happened!
    DCHECK(false) << "Reentrant web state insertion!";
    return;
  }
  CHECK(_URLLoader);

  UrlLoadParams params = UrlLoadParams::InNewTab(newTabURL);
  params.append_to = OpenPosition::kSpecifiedIndex;
  params.insertion_index = index;
  params.load_pinned = true;
  _URLLoader->Load(params);
}

// Inserts/removes a pinned item to/from the collection.
- (void)changePinnedStateForWebState:(web::WebState*)webState
                             atIndex:(int)index {
  if (self.webStateList->IsWebStatePinnedAt(index)) {
    TabSwitcherItem* item = [[PinnedItem alloc] initWithWebState:webState];
    [self.consumer insertItem:item
                      atIndex:index
               selectedItemID:GetActivePinnedTabID(self.webStateList)];

    _scopedWebStateObservation->AddObservation(webState);
  } else {
    [self.consumer removeItemWithID:webState->GetUniqueIdentifier()
                     selectedItemID:GetActivePinnedTabID(self.webStateList)];

    _scopedWebStateObservation->RemoveObservation(webState);
  }
}

@end
