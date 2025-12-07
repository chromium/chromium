// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

#import <ostream>

#import "base/check.h"

WebStateListChangeStatusOnly::WebStateListChangeStatusOnly(
    web::WebState* web_state,
    int index,
    bool pinned_state_changed,
    const TabGroup* old_group,
    const TabGroup* new_group)
    : web_state_(web_state),
      old_group_(old_group),
      new_group_(new_group),
      index_(index),
      pinned_state_changed_(pinned_state_changed) {
  if (index != WebStateList::kInvalidIndex) {
    CHECK(web_state_);
  }
}

WebStateListChange::Type WebStateListChangeStatusOnly::type() const {
  return kType;
}

WebStateListChangeDetach::WebStateListChangeDetach(
    web::WebState* detached_web_state,
    int detached_from_index,
    DetachReason detach_reason,
    const TabGroup* group)
    : detached_web_state_(detached_web_state),
      group_(group),
      detached_from_index_(detached_from_index),
      detach_reason_(detach_reason) {
  CHECK(detached_web_state_);
}

WebStateListChange::Type WebStateListChangeDetach::type() const {
  return kType;
}

WebStateListChangeMove::WebStateListChangeMove(web::WebState* moved_web_state,
                                               int moved_from_index,
                                               int moved_to_index,
                                               bool pinned_state_changed,
                                               const TabGroup* old_group,
                                               const TabGroup* new_group)
    : moved_web_state_(moved_web_state),
      old_group_(old_group),
      new_group_(new_group),
      moved_from_index_(moved_from_index),
      moved_to_index_(moved_to_index),
      pinned_state_changed_(pinned_state_changed) {
  CHECK(moved_web_state_);
}

WebStateListChange::Type WebStateListChangeMove::type() const {
  return kType;
}

WebStateListChangeReplace::WebStateListChangeReplace(
    web::WebState* replaced_web_state,
    web::WebState* inserted_web_state,
    int index)
    : replaced_web_state_(replaced_web_state),
      inserted_web_state_(inserted_web_state),
      index_(index) {
  CHECK(replaced_web_state_);
  CHECK(inserted_web_state_);
}

WebStateListChange::Type WebStateListChangeReplace::type() const {
  return kType;
}

WebStateListChangeInsert::WebStateListChangeInsert(
    web::WebState* inserted_web_state,
    int index,
    const TabGroup* group)
    : inserted_web_state_(inserted_web_state), group_(group), index_(index) {
  CHECK(inserted_web_state_);
}

WebStateListChange::Type WebStateListChangeInsert::type() const {
  return kType;
}

WebStateListChangeGroupCreate::WebStateListChangeGroupCreate(
    const TabGroup* created_group)
    : created_group_(created_group) {
  CHECK(created_group_);
}

WebStateListChange::Type WebStateListChangeGroupCreate::type() const {
  return kType;
}

WebStateListChangeGroupVisualDataUpdate::
    WebStateListChangeGroupVisualDataUpdate(
        const TabGroup* updated_group,
        const tab_groups::TabGroupVisualData& old_visual_data)
    : updated_group_(updated_group), old_visual_data_(old_visual_data) {
  CHECK(updated_group_);
}

WebStateListChange::Type WebStateListChangeGroupVisualDataUpdate::type() const {
  return kType;
}

WebStateListChangeGroupMove::WebStateListChangeGroupMove(
    const TabGroup* moved_group,
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
    const TabGroup* deleted_group)
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
