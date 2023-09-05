// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_bucket_context_handle.h"

#include "content/browser/indexed_db/indexed_db_bucket_context.h"

namespace content {

IndexedDBBucketContextHandle::IndexedDBBucketContextHandle() = default;
IndexedDBBucketContextHandle::IndexedDBBucketContextHandle(
    base::WeakPtr<IndexedDBBucketContext> bucket_state)
    : bucket_state_(bucket_state) {}
IndexedDBBucketContextHandle::IndexedDBBucketContextHandle(
    IndexedDBBucketContextHandle&&) = default;
IndexedDBBucketContextHandle& IndexedDBBucketContextHandle::operator=(
    IndexedDBBucketContextHandle&&) = default;

IndexedDBBucketContextHandle::~IndexedDBBucketContextHandle() {
  if (bucket_state_) {
    bucket_state_->OnHandleDestruction();
  }
}

void IndexedDBBucketContextHandle::Release() {
  if (bucket_state_) {
    bucket_state_->OnHandleDestruction();
    bucket_state_.reset();
  }
}

bool IndexedDBBucketContextHandle::IsHeld() const {
  return !!bucket_state_;
}

}  // namespace content
