// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_DATA_STORAGE_PARTITION_CODE_CACHE_DATA_REMOVER_H_
#define CONTENT_BROWSER_BROWSING_DATA_STORAGE_PARTITION_CODE_CACHE_DATA_REMOVER_H_

#include <stdint.h>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "url/gurl.h"

namespace disk_cache {
class Backend;
}

namespace content {

class StoragePartition;
class GeneratedCodeCacheContext;

// Helper to remove code cache data from a StoragePartition. This class is
// created on the UI thread and acts on the code cache thread.
class StoragePartitionCodeCacheDataRemover {
 public:
  // Creates a StoragePartitionCodeCacheDataRemover that deletes all cache
  // entries.
  static StoragePartitionCodeCacheDataRemover* Create(
      content::StoragePartition* storage_partition,
      base::RepeatingCallback<bool(const GURL&)> url_predicate,
      base::Time begin_time,
      base::Time end_time);

  StoragePartitionCodeCacheDataRemover(
      const StoragePartitionCodeCacheDataRemover&) = delete;
  StoragePartitionCodeCacheDataRemover& operator=(
      const StoragePartitionCodeCacheDataRemover&) = delete;

  // Calls |done_callback| on UI thread upon completion and also destroys
  // itself on UI thread.
  // This expects that either storage_partition with which this object was
  // created is live till the end of operation or GeneratedCodeCacheContext
  // is destroyed when the storage_partition is destroyed. This ensures the
  // code cache and hence the backend is destroyed. If this is not guaranteed
  // there could be a callback accessing the destroyed objects.
  void Remove(base::OnceClosure done_callback);

 private:
  // StoragePartitionCodeCacheDataRemover deletes itself (using DeleteHelper)
  // and is not supposed to be deleted by other objects so make destructor
  // private and DeleteHelper a friend.
  friend class base::DeleteHelper<StoragePartitionCodeCacheDataRemover>;

  explicit StoragePartitionCodeCacheDataRemover(
      GeneratedCodeCacheContext* generated_code_cache_context,
      base::RepeatingCallback<bool(const GURL&)> url_predicate,
      base::Time begin_time,
      base::Time end_time);

  ~StoragePartitionCodeCacheDataRemover();

  // Executed on UI thread.
  void ClearedCodeCache();

  // Executed on code cache thread.
  void ClearJSCodeCache();
  void ClearWASMCodeCache(int rv);
  void ClearWebUIJSCodeCache(int rv);
  void ClearCache(net::CompletionOnceCallback callback,
                  disk_cache::Backend* backend);
  void DoneClearCodeCache(int rv);

  const scoped_refptr<GeneratedCodeCacheContext> generated_code_cache_context_;

  base::OnceClosure done_callback_;
  base::Time begin_time_;
  base::Time end_time_;
  base::RepeatingCallback<bool(const GURL&)> url_predicate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_DATA_STORAGE_PARTITION_CODE_CACHE_DATA_REMOVER_H_
