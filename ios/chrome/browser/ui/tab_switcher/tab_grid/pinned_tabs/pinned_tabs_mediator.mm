// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_mediator.h"

#import <UIKit/UIKit.h>

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/scoped_multi_source_observation.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/url/url_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Constructs an array of TabSwitcherItems from a `web_state_list`.
NSArray* CreatePinnedTabConsumerItems(WebStateList* web_state_list) {
  NSMutableArray* items = [[NSMutableArray alloc] init];
  int pinnedWebStatesCount = web_state_list->GetIndexOfFirstNonPinnedWebState();

  for (int i = 0; i < pinnedWebStatesCount; i++) {
    DCHECK(web_state_list->IsWebStatePinnedAt(i));

    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    [items addObject:CreateItem(web_state)];
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
    [self.consumer selectItemWithID:GetActiveWebStateIdentifier(
                                        webStateList, /*pinned=*/YES)];
    return;
  }

  [self.consumer
          insertItem:CreateItem(webState)
             atIndex:index
      selectedItemID:GetActiveWebStateIdentifier(webStateList, /*pinned=*/YES)];

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
                      withItem:CreateItem(newWebState)];

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
    [self.consumer selectItemWithID:GetActiveWebStateIdentifier(
                                        webStateList, /*pinned=*/YES)];
    return;
  }

  [self.consumer removeItemWithID:webState->GetStableIdentifier()
                   selectedItemID:GetActiveWebStateIdentifier(webStateList,
                                                              /*pinned=*/YES)];

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
    [self.consumer insertItem:CreateItem(webState)
                      atIndex:index
               selectedItemID:GetActiveWebStateIdentifier(webStateList,
                                                          /*pinned=*/YES)];

    _scopedWebStateObservation->AddObservation(webState);
  } else {
    [self.consumer removeItemWithID:webState->GetStableIdentifier()
                     selectedItemID:GetActiveWebStateIdentifier(
                                        webStateList, /*pinned=*/YES)];

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
                      withItem:CreateItem(webState)];
}

#pragma mark - PinnedTabsCommands

- (void)selectItemWithID:(NSString*)itemID {
  int index =
      GetIndexOfTabWithIdentifier(self.webStateList, itemID, /*pinned=*/YES);
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

  // TODO(crbug.com/1382015): Record some "pinned tabs" related metrics and
  // check if "LogPriceDropMetrics" method needs be added.
  itemWebStateList->ActivateWebStateAt(index);
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
  [self.consumer
       populateItems:CreatePinnedTabConsumerItems(self.webStateList)
      selectedItemID:GetActiveWebStateIdentifier(self.webStateList, YES)];
}

@end
