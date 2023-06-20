// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

#import <ostream>

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebStateListChangeDetach::WebStateListChangeDetach(
    raw_ptr<web::WebState> detached_web_state)
    : detached_web_state_(detached_web_state) {}

WebStateListChange::Type WebStateListChangeDetach::type() const {
  return kType;
}

WebStateListChangeMove::WebStateListChangeMove(
    raw_ptr<web::WebState> moved_web_state,
    int moved_from_index)
    : moved_web_state_(moved_web_state), moved_from_index_(moved_from_index) {}

WebStateListChange::Type WebStateListChangeMove::type() const {
  return kType;
}

WebStateListChangeReplace::WebStateListChangeReplace(
    raw_ptr<web::WebState> replaced_web_state,
    raw_ptr<web::WebState> inserted_web_state)
    : replaced_web_state_(replaced_web_state),
      inserted_web_state_(inserted_web_state) {}

WebStateListChange::Type WebStateListChangeReplace::type() const {
  return kType;
}

WebStateListChangeInsert::WebStateListChangeInsert(
    raw_ptr<web::WebState> inserted_web_state)
    : inserted_web_state_(inserted_web_state) {}

WebStateListChange::Type WebStateListChangeInsert::type() const {
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

void WebStateListObserver::WillDetachWebStateAt(WebStateList* web_state_list,
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
