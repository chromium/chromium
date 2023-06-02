// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

#import <ostream>

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebStateListChangeReplace::WebStateListChangeReplace(
    raw_ptr<web::WebState> replaced_web_state,
    raw_ptr<web::WebState> inserted_web_state)
    : replaced_web_state_(replaced_web_state),
      inserted_web_state_(inserted_web_state) {}

WebStateListChange::Type WebStateListChangeReplace::type() const {
  return kType;
}

WebStateListObserver::WebStateListObserver() = default;

WebStateListObserver::~WebStateListObserver() {
  CHECK(!IsInObserverList())
      << "WebStateListObserver needs to be removed from WebStateList observer "
         "list before their destruction.";
}

void WebStateListObserver::WebStateListChanged(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateSelection& selection) {}

void WebStateListObserver::WebStateInsertedAt(WebStateList* web_state_list,
                                              web::WebState* web_state,
                                              int index,
                                              bool activating) {}

void WebStateListObserver::WebStateMoved(WebStateList* web_state_list,
                                         web::WebState* web_state,
                                         int from_index,
                                         int to_index) {}

void WebStateListObserver::WillDetachWebStateAt(WebStateList* web_state_list,
                                                web::WebState* web_state,
                                                int index) {}

void WebStateListObserver::WebStateDetachedAt(WebStateList* web_state_list,
                                              web::WebState* web_state,
                                              int index) {}

void WebStateListObserver::WillCloseWebStateAt(WebStateList* web_state_list,
                                               web::WebState* web_state,
                                               int index,
                                               bool user_action) {}

void WebStateListObserver::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {}

void WebStateListObserver::WebStatePinnedStateChanged(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {}

void WebStateListObserver::WillBeginBatchOperation(
    WebStateList* web_state_list) {}

void WebStateListObserver::BatchOperationEnded(WebStateList* web_state_list) {}

void WebStateListObserver::WebStateListDestroyed(WebStateList* web_state_list) {
}
