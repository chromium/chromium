// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/bucket_context_handle.h"

#include "content/browser/indexed_db/instance/bucket_context.h"

namespace content::indexed_db {

BucketContextHandle::BucketContextHandle() = default;
BucketContextHandle::BucketContextHandle(BucketContext& bucket_context)
    : bucket_context_(bucket_context.AsWeakPtr()) {
  bucket_context_->OnHandleCreated();
}

BucketContextHandle::BucketContextHandle(BucketContextHandle&&) = default;
BucketContextHandle& BucketContextHandle::operator=(BucketContextHandle&&) =
    default;

BucketContextHandle::BucketContextHandle(const BucketContextHandle& other)
    : bucket_context_(other.bucket_context_) {
  if (bucket_context_) {
    bucket_context_->OnHandleCreated();
  }
}

BucketContextHandle BucketContextHandle::operator=(
    const BucketContextHandle& other) {
  return BucketContextHandle(other);
}

BucketContextHandle::~BucketContextHandle() {
  if (bucket_context_) {
    bucket_context_->OnHandleDestruction();
  }
}

void BucketContextHandle::Release() {
  if (bucket_context_) {
    bucket_context_->OnHandleDestruction();
    bucket_context_.reset();
  }
}

bool BucketContextHandle::IsHeld() const {
  return !!bucket_context_;
}

}  // namespace content::indexed_db
