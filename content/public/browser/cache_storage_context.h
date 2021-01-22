// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CACHE_STORAGE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_CACHE_STORAGE_CONTEXT_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted_delete_on_sequence.h"

namespace url {
class Origin;
}

namespace content {

struct StorageUsageInfo;

// Represents the per-BrowserContext Cache Storage data.
class CacheStorageContext
    : public base::RefCountedDeleteOnSequence<CacheStorageContext> {
 public:
  explicit CacheStorageContext(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : RefCountedDeleteOnSequence<CacheStorageContext>(
            std::move(task_runner)) {}

  using GetUsageInfoCallback =
      base::OnceCallback<void(const std::vector<StorageUsageInfo>& usage_info)>;

  // Methods used in response to browsing data and quota manager requests.
  // May be called on any sequence.
  virtual void GetAllOriginsInfo(GetUsageInfoCallback callback) = 0;
  virtual void DeleteForOrigin(const url::Origin& origin) = 0;

 protected:
  friend class base::RefCountedDeleteOnSequence<CacheStorageContext>;
  friend class base::DeleteHelper<CacheStorageContext>;
  virtual ~CacheStorageContext() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CACHE_STORAGE_CONTEXT_H_
