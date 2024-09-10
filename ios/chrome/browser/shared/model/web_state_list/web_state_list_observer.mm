// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

#import <ostream>

#import "base/check.h"

WebStateListChangeStatusOnly::WebStateListChangeStatusOnly(
    raw_ptr<web::WebState> web_state,
    int index,
    bool pinned_state_changed,
    raw_ptr<const TabGroup> old_group,
    raw_ptr<const TabGroup> new_group)
    : web_state_(web_state),
      index_(index),
      pinned_state_changed_(pinned_state_changed),
      old_group_(old_group),
      new_group_(new_group) {}

WebStateListChange::Type WebStateListChangeStatusOnly::type() const {
  return kType;
}

WebStateListChangeDetach::WebStateListChangeDetach(
    raw_ptr<web::WebState> detached_web_state,
    int detached_from_index,
    bool is_closing,
    bool is_user_action,
    bool is_tabs_cleanup,
    raw_ptr<const TabGroup> group)
    : detached_web_state_(detached_web_state),
      detached_from_index_(detached_from_index),
      is_closing_(is_closing),
      is_user_action_(is_user_action),
      is_tabs_cleanup_(is_tabs_cleanup),
      group_(group) {}

WebStateListChange::Type WebStateListChangeDetach::type() const {
  return kType;
}

WebStateListChangeMove::WebStateListChangeMove(
    raw_ptr<web::WebState> moved_web_state,
    int moved_from_index,
    int moved_to_index,
    bool pinned_state_changed,
    raw_ptr<const TabGroup> old_group,
    raw_ptr<const TabGroup> new_group)
    : moved_web_state_(moved_web_state),
      moved_from_index_(moved_from_index),
      moved_to_index_(moved_to_index),
      pinned_state_changed_(pinned_state_changed),
      old_group_(old_group),
      new_group_(new_group) {}

WebStateListChange::Type WebStateListChangeMove::type() const {
  return kType;
}

WebStateListChangeReplace::WebStateListChangeReplace(
    raw_ptr<web::WebState> replaced_web_state,
    raw_ptr<web::WebState> inserted_web_state,
    int index)
    : replaced_web_state_(replaced_web_state),
      inserted_web_state_(inserted_web_state),
      index_(index) {}

WebStateListChange::Type WebStateListChangeReplace::type() const {
  return kType;
}

WebStateListChangeInsert::WebStateListChangeInsert(
    raw_ptr<web::WebState> inserted_web_state,
    int index,
    raw_ptr<const TabGroup> group)
    : inserted_web_state_(inserted_web_state), index_(index), group_(group) {}

WebStateListChange::Type WebStateListChangeInsert::type() const {
  return kType;
}

WebStateListChangeGroupCreate::WebStateListChangeGroupCreate(
    raw_ptr<const TabGroup> created_group)
    : created_group_(created_group) {
  CHECK(created_group_);
}

WebStateListChange::Type WebStateListChangeGroupCreate::type() const {
  return kType;
}

WebStateListChangeGroupVisualDataUpdate::
    WebStateListChangeGroupVisualDataUpdate(
        raw_ptr<const TabGroup> updated_group,
        const tab_groups::TabGroupVisualData& old_visual_data)
    : updated_group_(updated_group), old_visual_data_(old_visual_data) {
  DCHECK(updated_group_);
}

WebStateListChange::Type WebStateListChangeGroupVisualDataUpdate::type() const {
  return kType;
}

WebStateListChangeGroupMove::WebStateListChangeGroupMove(
    raw_ptr<const TabGroup> moved_group,
    TabGroupRange moved_from_range,
    TabGroupRange moved_to_range)
    : moved_group_(moved_group),
      moved_from_range_(moved_from_range),
      moved_to_range_(moved_to_range) {
  CHECK(moved_group_);
  CHECK(moved_from_range_ != moved_to_range_);
}

WebStateListChange::Type WebStateListChangeGroupMove::type() const {
  return kType;
}

WebStateListChangeGroupDelete::WebStateListChangeGroupDelete(
    raw_ptr<const TabGroup> deleted_group)
    : deleted_group_(deleted_group) {
  CHECK(deleted_group_);
}

WebStateListChange::Type WebStateListChangeGroupDelete::type() const {
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
