// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebStateListObserverBridge::WebStateListObserverBridge(
    id<WebStateListObserving> observer)
    : observer_(observer) {}

WebStateListObserverBridge::~WebStateListObserverBridge() {}

void WebStateListObserverBridge::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  const SEL selector = @selector(willChangeWebStateList:change:status:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ willChangeWebStateList:web_state_list
                             change:detach_change
                             status:status];
}

void WebStateListObserverBridge::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  const SEL selector = @selector(didChangeWebStateList:change:status:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ didChangeWebStateList:web_state_list change:change status:status];
}

// TODO(crbug.com/1442546): Remove WebStateActivatedAt() once all observers
// handle the activation inside -didChangeWebStateList:change:status:.
void WebStateListObserverBridge::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {
  const SEL selector = @selector(webStateList:
                      didChangeActiveWebState:oldWebState:atIndex:reason:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ webStateList:web_state_list
      didChangeActiveWebState:new_web_state
                  oldWebState:old_web_state
                      atIndex:active_index
                       reason:reason];
}

void WebStateListObserverBridge::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  const SEL selector = @selector(webStateListWillBeginBatchOperation:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ webStateListWillBeginBatchOperation:web_state_list];
}

void WebStateListObserverBridge::BatchOperationEnded(
    WebStateList* web_state_list) {
  const SEL selector = @selector(webStateListBatchOperationEnded:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ webStateListBatchOperationEnded:web_state_list];
}

void WebStateListObserverBridge::WebStateListDestroyed(
    WebStateList* web_state_list) {
  const SEL selector = @selector(webStateListDestroyed:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ webStateListDestroyed:web_state_list];
}
