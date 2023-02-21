// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_mediator.h"

#import "base/notreached.h"
#import "base/scoped_multi_source_observation.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ScopedWebStateListObservation =
    base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>;
using ScopedWebStateObservation =
    base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>;

@interface InactiveTabsMediator () <CRWWebStateObserver,
                                    WebStateListObserving> {
  // Observers for WebStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<ScopedWebStateListObservation> _scopedWebStateListObservation;
  // Observers for WebStates.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<ScopedWebStateObservation> _scopedWebStateObservation;
}

// The UI consumer to which updates are made.
@property(nonatomic, weak, readonly) id<TabCollectionConsumer> consumer;
// The list of inactive tabs.
@property(nonatomic, assign, readonly) WebStateList* webStateList;

@end

@implementation InactiveTabsMediator

- (instancetype)initWithConsumer:(id<TabCollectionConsumer>)consumer {
  if (self = [super init]) {
    DCHECK(IsInactiveTabsEnabled());
    _consumer = consumer;
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _scopedWebStateListObservation =
        std::make_unique<ScopedWebStateListObservation>(
            _webStateListObserverBridge.get());
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _scopedWebStateObservation = std::make_unique<ScopedWebStateObservation>(
        _webStateObserverBridge.get());
  }
  return self;
}

#pragma mark - Public properties

- (void)setInactiveBrowser:(Browser*)inactiveBrowser {
  _scopedWebStateListObservation->RemoveAllObservations();
  _scopedWebStateObservation->RemoveAllObservations();

  _inactiveBrowser = inactiveBrowser;
  _webStateList = inactiveBrowser->GetWebStateList();

  if (_webStateList) {
    _scopedWebStateListObservation->AddObservation(_webStateList);
    [self addWebStateObservations];
    [self populateConsumerItems];
  }
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

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  NOTREACHED();
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
  DCHECK(!webStateList->IsBatchInProgress());

  [_consumer removeItemWithID:webState->GetStableIdentifier()
               selectedItemID:nil];

  _scopedWebStateObservation->RemoveObservation(webState);
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
  NOTREACHED();
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  NOTREACHED();
}

#pragma mark - Private

// Add observers to all web states from the list.
- (void)addWebStateObservations {
  for (int i = 0; i < _webStateList->count(); i++) {
    web::WebState* webState = _webStateList->GetWebStateAt(i);
    _scopedWebStateObservation->AddObservation(webState);
  }
}

// Calls `-populateItems:selectedItemID:` on the consumer.
- (void)populateConsumerItems {
  NSMutableArray* items = [[NSMutableArray alloc] init];
  for (int i = 0; i < _webStateList->count(); i++) {
    web::WebState* webState = _webStateList->GetWebStateAt(i);
    [items addObject:GetTabSwitcherItem(webState)];
  }
  [_consumer populateItems:items selectedItemID:nil];
}

@end
