// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

#import <ostream>

#import "base/check.h"

WebStateListChangeStatusOnly::WebStateListChangeStatusOnly(
    raw_ptr<web::WebState> selected_web_state)
    : selected_web_state_(selected_web_state) {}

WebStateListChange::Type WebStateListChangeStatusOnly::type() const {
  return kType;
}

WebStateListChangeDetach::WebStateListChangeDetach(
    raw_ptr<web::WebState> detached_web_state,
    bool is_closing,
    bool is_user_action)
    : detached_web_state_(detached_web_state),
      is_closing_(is_closing),
      is_user_action_(is_user_action) {}

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

void WebStateListObserver::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {}

void WebStateListObserver::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {}

void WebStateListObserver::WillBeginBatchOperation(
    WebStateList* web_state_list) {}

void WebStateListObserver::BatchOperationEnded(WebStateList* web_state_list) {}

void WebStateListObserver::WebStateListDestroyed(WebStateList* web_state_list) {
}
