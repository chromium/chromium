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
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/drag_and_drop/drag_item_util.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/main/browser_util.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/url/url_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using PinnedState = WebStateSearchCriteria::PinnedState;

namespace {

// Constructs an array of TabSwitcherItems from a `web_state_list`.
NSArray* CreatePinnedTabConsumerItems(WebStateList* web_state_list) {
  NSMutableArray* items = [[NSMutableArray alloc] init];
  int pinnedWebStatesCount = web_state_list->GetIndexOfFirstNonPinnedWebState();

  for (int i = 0; i < pinnedWebStatesCount; i++) {
    DCHECK(web_state_list->IsWebStatePinnedAt(i));

    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    [items addObject:GetTabSwitcherItem(web_state)];
  }
  return [items copy];
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

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  DCHECK_EQ(_webStateList, webStateList);

  if (webStateList->IsBatchInProgress()) {
    return;
  }

  if (!webStateList->IsWebStatePinnedAt(index)) {
    [self.consumer
        selectItemWithID:GetActiveWebStateIdentifier(
                             webStateList,
                             WebStateSearchCriteria{
                                 .pinned_state = PinnedState::kPinned,
                             })];
    return;
  }

  [self.consumer
          insertItem:GetTabSwitcherItem(webState)
             atIndex:index
      selectedItemID:GetActiveWebStateIdentifier(
                         webStateList, WebStateSearchCriteria{
                                           .pinned_state = PinnedState::kPinned,
                                       })];

  _scopedWebStateObservation->AddObservation(webState);
}

- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex {
  DCHECK_EQ(_webStateList, webStateList);

  if (webStateList->IsBatchInProgress()) {
    return;
  }

  if (!webStateList->IsWebStatePinnedAt(toIndex)) {
    return;
  }

  [self.consumer moveItemWithID:webState->GetStableIdentifier()
                        toIndex:toIndex];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  DCHECK_EQ(_webStateList, webStateList);

  if (webStateList->IsBatchInProgress()) {
    return;
  }

  if (!webStateList->IsWebStatePinnedAt(index)) {
    return;
  }

  [self.consumer replaceItemID:oldWebState->GetStableIdentifier()
                      withItem:GetTabSwitcherItem(newWebState)];

  _scopedWebStateObservation->RemoveObservation(oldWebState);
  _scopedWebStateObservation->AddObservation(newWebState);
}

- (void)webStateList:(WebStateList*)webStateList
    willDetachWebState:(web::WebState*)webState
               atIndex:(int)index {
  DCHECK_EQ(_webStateList, webStateList);

  if (webStateList->IsBatchInProgress()) {
    return;
  }

  if (!webStateList) {
    return;
  }

  if (!webStateList->IsWebStatePinnedAt(index)) {
    [self.consumer
        selectItemWithID:GetActiveWebStateIdentifier(
                             webStateList,
                             WebStateSearchCriteria{
                                 .pinned_state = PinnedState::kPinned,
                             })];
    return;
  }

  [self.consumer removeItemWithID:webState->GetStableIdentifier()
                   selectedItemID:GetActiveWebStateIdentifier(
                                      webStateList,
                                      WebStateSearchCriteria{
                                          .pinned_state = PinnedState::kPinned,
                                      })];

  _scopedWebStateObservation->RemoveObservation(webState);
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

  if (!webStateList->IsWebStatePinnedAt(atIndex)) {
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

  if (webStateList->IsWebStatePinnedAt(index)) {
    [self.consumer insertItem:GetTabSwitcherItem(webState)
                      atIndex:index
               selectedItemID:GetActiveWebStateIdentifier(
                                  webStateList,
                                  WebStateSearchCriteria{
                                      .pinned_state = PinnedState::kPinned,
                                  })];

    _scopedWebStateObservation->AddObservation(webState);
  } else {
    [self.consumer
        removeItemWithID:webState->GetStableIdentifier()
          selectedItemID:GetActiveWebStateIdentifier(
                             webStateList,
                             WebStateSearchCriteria{
                                 .pinned_state = PinnedState::kPinned,
                             })];

    _scopedWebStateObservation->RemoveObservation(webState);
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
  [self.consumer replaceItemID:webState->GetStableIdentifier()
                      withItem:GetTabSwitcherItem(webState)];
}

#pragma mark - GridImageDataSource

- (void)snapshotForIdentifier:(NSString*)identifier
                   completion:(void (^)(UIImage*))completion {
  web::WebState* webState =
      GetWebState(self.webStateList, WebStateSearchCriteria{
                                         .identifier = identifier,
                                         .pinned_state = PinnedState::kPinned,
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
      GetWebState(self.webStateList, WebStateSearchCriteria{
                                         .identifier = identifier,
                                         .pinned_state = PinnedState::kPinned,
                                     });

  if (!webState) {
    return;
  }

  // NTP tabs get the Chrome product favicon.
  if (IsURLNtp(webState->GetVisibleURL())) {
    UIImage* chromeProductIcon = CustomSymbolWithPointSize(
        kChromeProductSymbol, kPinnedCellFaviconSymbolPointSize);
    completion(chromeProductIcon);
    return;
  }

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
  // TODO (crbug.com/1406524): Implement or remove.
}

- (void)clearPreloadedSnapshots {
  // TODO (crbug.com/1406524): Implement or remove.
}

#pragma mark - TabCollectionCommands

- (void)selectItemWithID:(NSString*)itemID {
  base::RecordAction(base::UserMetricsAction("MobileTabGridPinnedTabSelected"));

  int index =
      GetTabIndex(self.webStateList, WebStateSearchCriteria{
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
}

- (void)closeItemWithID:(NSString*)itemID {
  int index =
      GetTabIndex(self.webStateList, WebStateSearchCriteria{
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
  int sourceIndex =
      GetTabIndex(self.webStateList, WebStateSearchCriteria{
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
  _dragItemID = [itemID copy];
  web::WebState* webState =
      GetWebState(self.webStateList, WebStateSearchCriteria{
                                         .identifier = itemID,
                                         .pinned_state = PinnedState::kPinned,
                                     });

  return CreateTabDragItem(webState);
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
        GetTabIndex(self.webStateList, WebStateSearchCriteria{
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

  // Insert a new webState using the `INSERT_PINNED` flag.
  self.webStateList->InsertWebState(
      base::checked_cast<int>(index), std::move(webState),
      (WebStateList::INSERT_PINNED), WebStateOpener());
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

@end
