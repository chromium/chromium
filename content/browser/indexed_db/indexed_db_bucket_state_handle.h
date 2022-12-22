// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_STATE_HANDLE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_STATE_HANDLE_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {
class IndexedDBBucketState;

// This handle tells the IndexedDBBucketState that there is still something
// using the bucket, and the IndexedDBBucketState won't close until all
// handles are destroyed. Destroying this handle can cause the bucket state
// to synchronously destruct, which modifies the `factories_per_bucket_`
// map in IndexedDBFactory.
class CONTENT_EXPORT IndexedDBBucketStateHandle {
 public:
  IndexedDBBucketStateHandle();
  explicit IndexedDBBucketStateHandle(
      base::WeakPtr<IndexedDBBucketState> bucket_state);
  IndexedDBBucketStateHandle(IndexedDBBucketStateHandle&&);
  IndexedDBBucketStateHandle& operator=(IndexedDBBucketStateHandle&&);

  IndexedDBBucketStateHandle(const IndexedDBBucketStateHandle&) = delete;
  IndexedDBBucketStateHandle& operator=(const IndexedDBBucketStateHandle&) =
      delete;

  ~IndexedDBBucketStateHandle();

  bool IsHeld() const;

  void Release();

  // Returns null if the factory was destroyed, which should only happen on
  // context destruction.
  IndexedDBBucketState* bucket_state() { return bucket_state_.get(); }

 private:
  base::WeakPtr<IndexedDBBucketState> bucket_state_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_STATE_HANDLE_H_
