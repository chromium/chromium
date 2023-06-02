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

void WebStateListObserverBridge::WebStateListChanged(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateSelection& selection) {
  switch (change.type()) {
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      const SEL selector = @selector(webStateList:
                               didReplaceWebState:withWebState:atIndex:);
      if (![observer_ respondsToSelector:selector]) {
        return;
      }

      // TODO(crbug.com/1442546): Introduce -webStateList:didWebStateListChanged
      // to WebStateListObserverBridge and replace
      // -webStateList:didReplaceWebState with it.
      [observer_ webStateList:web_state_list
           didReplaceWebState:replace_change.replaced_web_state()
                 withWebState:replace_change.inserted_web_state()
                      atIndex:selection.index];
      break;
    }
  }
}

void WebStateListObserverBridge::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  const SEL selector = @selector(webStateList:
                            didInsertWebState:atIndex:activating:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ webStateList:web_state_list
        didInsertWebState:web_state
                  atIndex:index
               activating:activating];
}

void WebStateListObserverBridge::WebStateMoved(WebStateList* web_state_list,
                                               web::WebState* web_state,
                                               int from_index,
                                               int to_index) {
  const SEL selector = @selector(webStateList:
                              didMoveWebState:fromIndex:toIndex:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ webStateList:web_state_list
          didMoveWebState:web_state
                fromIndex:from_index
                  toIndex:to_index];
}

void WebStateListObserverBridge::WillDetachWebStateAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  const SEL selector = @selector(webStateList:willDetachWebState:atIndex:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ webStateList:web_state_list
       willDetachWebState:web_state
                  atIndex:index];
}

void WebStateListObserverBridge::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  const SEL selector = @selector(webStateList:didDetachWebState:atIndex:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ webStateList:web_state_list
        didDetachWebState:web_state
                  atIndex:index];
}

void WebStateListObserverBridge::WillCloseWebStateAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool user_action) {
  const SEL selector = @selector(webStateList:
                            willCloseWebState:atIndex:userAction:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ webStateList:web_state_list
        willCloseWebState:web_state
                  atIndex:index
               userAction:(user_action ? YES : NO)];
}

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

void WebStateListObserverBridge::WebStatePinnedStateChanged(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  const SEL selector = @selector(webStateList:
              didChangePinnedStateForWebState:atIndex:);
  if (![observer_ respondsToSelector:selector]) {
    return;
  }

  [observer_ webStateList:web_state_list
      didChangePinnedStateForWebState:web_state
                              atIndex:index];
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
