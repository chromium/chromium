// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_bucket_state_handle.h"

#include "content/browser/indexed_db/indexed_db_bucket_state.h"

namespace content {

IndexedDBBucketStateHandle::IndexedDBBucketStateHandle() = default;
IndexedDBBucketStateHandle::IndexedDBBucketStateHandle(
    base::WeakPtr<IndexedDBBucketState> bucket_state)
    : bucket_state_(bucket_state) {}
IndexedDBBucketStateHandle::IndexedDBBucketStateHandle(
    IndexedDBBucketStateHandle&&) = default;
IndexedDBBucketStateHandle& IndexedDBBucketStateHandle::operator=(
    IndexedDBBucketStateHandle&&) = default;

IndexedDBBucketStateHandle::~IndexedDBBucketStateHandle() {
  if (bucket_state_)
    bucket_state_->OnHandleDestruction();
}

void IndexedDBBucketStateHandle::Release() {
  if (bucket_state_) {
    bucket_state_->OnHandleDestruction();
    bucket_state_.reset();
  }
}

bool IndexedDBBucketStateHandle::IsHeld() const {
  return !!bucket_state_;
}

}  // namespace content
