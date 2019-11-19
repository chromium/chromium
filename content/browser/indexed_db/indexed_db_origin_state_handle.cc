// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_origin_state_handle.h"

#include "content/browser/indexed_db/indexed_db_origin_state.h"

namespace content {

IndexedDBOriginStateHandle::IndexedDBOriginStateHandle() = default;
IndexedDBOriginStateHandle::IndexedDBOriginStateHandle(
    base::WeakPtr<IndexedDBOriginState> origin_state)
    : origin_state_(origin_state) {}
IndexedDBOriginStateHandle::IndexedDBOriginStateHandle(
    IndexedDBOriginStateHandle&&) = default;
IndexedDBOriginStateHandle& IndexedDBOriginStateHandle::operator=(
    IndexedDBOriginStateHandle&&) = default;

IndexedDBOriginStateHandle::~IndexedDBOriginStateHandle() {
  if (origin_state_)
    origin_state_->OnHandleDestruction();
}

void IndexedDBOriginStateHandle::Release() {
  if (origin_state_) {
    origin_state_->OnHandleDestruction();
    origin_state_.reset();
  }
}

bool IndexedDBOriginStateHandle::IsHeld() const {
  return !!origin_state_;
}

}  // namespace content
