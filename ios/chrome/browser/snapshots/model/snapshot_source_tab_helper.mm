// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_source_tab_helper.h"

#import "base/memory/ptr_util.h"
#import "ios/web/public/web_state.h"

SnapshotSourceTabHelper::~SnapshotSourceTabHelper() = default;

SnapshotSourceTabHelper::SnapshotSourceTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  CHECK(web_state_);
  web_state_observation_.Observe(web_state_);
}

bool SnapshotSourceTabHelper::CanTakeSnapshot() const {
  web::WebState* source_web_state =
      overriding_source_web_state_.get() ?: web_state_.get();
  CHECK(source_web_state);
  return source_web_state->CanTakeSnapshot();
}

void SnapshotSourceTabHelper::TakeSnapshot(const CGRect rect,
                                           SnapshotCallback callback) {
  web::WebState* source_web_state =
      overriding_source_web_state_.get() ?: web_state_.get();
  CHECK(source_web_state);
  return source_web_state->TakeSnapshot(rect, callback);
}

UIView* SnapshotSourceTabHelper::GetView() {
  web::WebState* source_web_state =
      overriding_source_web_state_.get() ?: web_state_.get();
  CHECK(source_web_state);
  return source_web_state->GetView();
}

void SnapshotSourceTabHelper::SetOverridingSourceWebState(
    web::WebState* overriding_source_web_state) {
  overriding_source_web_state_.reset();
  if (overriding_source_web_state) {
    overriding_source_web_state_ = overriding_source_web_state->GetWeakPtr();
  }
}

void SnapshotSourceTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state_observation_.Reset();
  web_state_ = nullptr;
}
