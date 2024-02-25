// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_bucket_context_handle.h"

#include "content/browser/indexed_db/indexed_db_bucket_context.h"

namespace content {

IndexedDBBucketContextHandle::IndexedDBBucketContextHandle() = default;
IndexedDBBucketContextHandle::IndexedDBBucketContextHandle(
    IndexedDBBucketContext& bucket_context)
    : bucket_context_(bucket_context.AsWeakPtr()) {
  bucket_context_->OnHandleCreated();
}

IndexedDBBucketContextHandle::IndexedDBBucketContextHandle(
    IndexedDBBucketContextHandle&&) = default;
IndexedDBBucketContextHandle& IndexedDBBucketContextHandle::operator=(
    IndexedDBBucketContextHandle&&) = default;

IndexedDBBucketContextHandle::IndexedDBBucketContextHandle(
    const IndexedDBBucketContextHandle& other)
    : bucket_context_(other.bucket_context_) {
  if (bucket_context_) {
    bucket_context_->OnHandleCreated();
  }
}

IndexedDBBucketContextHandle IndexedDBBucketContextHandle::operator=(
    const IndexedDBBucketContextHandle& other) {
  return IndexedDBBucketContextHandle(other);
}

IndexedDBBucketContextHandle::~IndexedDBBucketContextHandle() {
  if (bucket_context_) {
    bucket_context_->OnHandleDestruction();
  }
}

void IndexedDBBucketContextHandle::Release() {
  if (bucket_context_) {
    bucket_context_->OnHandleDestruction();
    bucket_context_.reset();
  }
}

bool IndexedDBBucketContextHandle::IsHeld() const {
  return !!bucket_context_;
}

}  // namespace content
