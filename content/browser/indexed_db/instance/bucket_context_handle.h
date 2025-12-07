// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_BUCKET_CONTEXT_HANDLE_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_BUCKET_CONTEXT_HANDLE_H_

#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content::indexed_db {

class BucketContext;

// This object tells the BucketContext that there is still something using the
// backing store, and the BucketContext won't close it until all handles are
// destroyed. "Something" means, for example, a `Connection`.
class CONTENT_EXPORT BucketContextHandle {
 public:
  BucketContextHandle();
  explicit BucketContextHandle(BucketContext& bucket_context);
  BucketContextHandle(BucketContextHandle&&);
  BucketContextHandle& operator=(BucketContextHandle&&);
  BucketContextHandle(const BucketContextHandle&);
  BucketContextHandle operator=(const BucketContextHandle&);

  ~BucketContextHandle();

  bool IsHeld() const;

  void Release();

  // Returns null if the factory was destroyed, which should only happen on
  // context destruction.
  BucketContext* bucket_context() { return bucket_context_.get(); }

  BucketContext* operator->() {
    CHECK(bucket_context_.get());
    return bucket_context_.get();
  }
  const BucketContext* operator->() const {
    CHECK(bucket_context_.get());
    return bucket_context_.get();
  }

 private:
  base::WeakPtr<BucketContext> bucket_context_;
};

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_BUCKET_CONTEXT_HANDLE_H_
