// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"

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
