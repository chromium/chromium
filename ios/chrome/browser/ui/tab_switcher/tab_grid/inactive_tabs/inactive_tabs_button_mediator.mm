// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_button_mediator.h"

#import "base/notreached.h"
#import "base/scoped_multi_source_observation.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_count_consumer.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ScopedWebStateListObservation =
    base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>;

@interface InactiveTabsButtonMediator () <WebStateListObserving> {
  // The UI consumer to which updates are made.
  __weak id<InactiveTabsCountConsumer> _consumer;
  // The list of inactive tabs.
  base::WeakPtr<WebStateList> _webStateList;
  // Observers for WebStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<ScopedWebStateListObservation> _scopedWebStateListObservation;
}

@end

@implementation InactiveTabsButtonMediator

- (instancetype)initWithConsumer:(id<InactiveTabsCountConsumer>)consumer
                    webStateList:(base::WeakPtr<WebStateList>)webStateList {
  DCHECK(IsInactiveTabsEnabled());
  DCHECK(consumer);
  DCHECK(webStateList.get());
  self = [super init];
  if (self) {
    _consumer = consumer;
    _webStateList = webStateList;
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _scopedWebStateListObservation =
        std::make_unique<ScopedWebStateListObservation>(
            _webStateListObserverBridge.get());
    _scopedWebStateListObservation->AddObservation(_webStateList.get());
    [_consumer advertizeInactiveTabsWithCount:_webStateList->count()];
  }
  return self;
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
  DCHECK_EQ(_webStateList.get(), webStateList);
  [_consumer advertizeInactiveTabsWithCount:_webStateList->count()];
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  NOTREACHED();
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

@end
