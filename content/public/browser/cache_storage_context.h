// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CACHE_STORAGE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_CACHE_STORAGE_CONTEXT_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "components/services/storage/public/mojom/cache_storage_control.mojom.h"

namespace content {

// Represents the per-BrowserContext Cache Storage data.
// TODO(enne): transition callers of this base class over to use mojo
// interface, and then fold base class into CacheStorageContextImpl.
class CacheStorageContext
    : public base::RefCountedDeleteOnSequence<CacheStorageContext>,
      public storage::mojom::CacheStorageControl {
 public:
  explicit CacheStorageContext(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : RefCountedDeleteOnSequence<CacheStorageContext>(
            std::move(task_runner)) {}

 protected:
  friend class base::RefCountedDeleteOnSequence<CacheStorageContext>;
  friend class base::DeleteHelper<CacheStorageContext>;
  ~CacheStorageContext() override = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CACHE_STORAGE_CONTEXT_H_
