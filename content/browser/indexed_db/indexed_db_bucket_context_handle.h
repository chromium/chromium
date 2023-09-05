// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_CONTEXT_HANDLE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_CONTEXT_HANDLE_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {
class IndexedDBBucketContext;

// This handle tells the IndexedDBBucketContext that there is still something
// using the bucket, and the IndexedDBBucketContext won't close until all
// handles are destroyed. Destroying this handle can cause the bucket state
// to synchronously destruct, which modifies the `bucket_contexts_`
// map in IndexedDBFactory.
class CONTENT_EXPORT IndexedDBBucketContextHandle {
 public:
  IndexedDBBucketContextHandle();
  explicit IndexedDBBucketContextHandle(IndexedDBBucketContext& bucket_context);
  IndexedDBBucketContextHandle(IndexedDBBucketContextHandle&&);
  IndexedDBBucketContextHandle& operator=(IndexedDBBucketContextHandle&&);
  IndexedDBBucketContextHandle(const IndexedDBBucketContextHandle&);
  IndexedDBBucketContextHandle operator=(const IndexedDBBucketContextHandle&);

  ~IndexedDBBucketContextHandle();

  bool IsHeld() const;

  void Release();

  // Returns null if the factory was destroyed, which should only happen on
  // context destruction.
  IndexedDBBucketContext* bucket_context() { return bucket_context_.get(); }

  IndexedDBBucketContext* operator->() {
    CHECK(bucket_context_.get());
    return bucket_context_.get();
  }
  const IndexedDBBucketContext* operator->() const {
    CHECK(bucket_context_.get());
    return bucket_context_.get();
  }

 private:
  base::WeakPtr<IndexedDBBucketContext> bucket_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_BUCKET_CONTEXT_HANDLE_H_
