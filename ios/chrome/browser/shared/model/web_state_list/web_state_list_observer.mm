// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

#import <ostream>

#import "base/check.h"

WebStateListChangeStatusOnly::WebStateListChangeStatusOnly(
    raw_ptr<web::WebState> web_state,
    raw_ptr<const TabGroup> old_group,
    raw_ptr<const TabGroup> new_group)
    : web_state_(web_state), old_group_(old_group), new_group_(new_group) {}

WebStateListChange::Type WebStateListChangeStatusOnly::type() const {
  return kType;
}

WebStateListChangeDetach::WebStateListChangeDetach(
    raw_ptr<web::WebState> detached_web_state,
    bool is_closing,
    bool is_user_action,
    raw_ptr<const TabGroup> group)
    : detached_web_state_(detached_web_state),
      is_closing_(is_closing),
      is_user_action_(is_user_action),
      group_(group) {}

WebStateListChange::Type WebStateListChangeDetach::type() const {
  return kType;
}

WebStateListChangeMove::WebStateListChangeMove(
    raw_ptr<web::WebState> moved_web_state,
    int moved_from_index,
    raw_ptr<const TabGroup> old_group,
    raw_ptr<const TabGroup> new_group)
    : moved_web_state_(moved_web_state),
      moved_from_index_(moved_from_index),
      old_group_(old_group),
      new_group_(new_group) {}

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
    raw_ptr<web::WebState> inserted_web_state,
    raw_ptr<const TabGroup> group)
    : inserted_web_state_(inserted_web_state), group_(group) {}

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
