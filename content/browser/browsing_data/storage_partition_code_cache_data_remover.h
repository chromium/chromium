// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_DATA_STORAGE_PARTITION_CODE_CACHE_DATA_REMOVER_H_
#define CONTENT_BROWSER_BROWSING_DATA_STORAGE_PARTITION_CODE_CACHE_DATA_REMOVER_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequenced_task_runner_helpers.h"

namespace content {

class StoragePartition;
class GeneratedCodeCacheContext;

// Helper to remove http cache data from a StoragePartition. This class is
// created on the UI thread and calls the provided callback and destroys itself
// on the UI thread after the code caches are cleared. This class also takes a
// reference to the generated_code_cache_context and is used in read-only mode
// on both the UI / IO thread. Since this isn't modified, it is OK to access it
// on both threads. The caches are actually cleared on the IO threads. When the
// Remove function is called, it posts tasks on the IO thread to clear the code
// caches. Once the the caches are cleared, the callback is called on the UI
// thread.
class StoragePartitionCodeCacheDataRemover {
 public:
  // Creates a StoragePartitionCodeCacheDataRemover that deletes all cache
  // entries.
  static StoragePartitionCodeCacheDataRemover* Create(
      content::StoragePartition* storage_partition);

  // Calls |done_callback| on UI thread upon completion and also destroys
  // itself on UI thread.
  void Remove(base::OnceClosure done_callback);

 private:
  // StoragePartitionCodeCacheDataRemover deletes itself (using DeleteHelper)
  // and is not supposed to be deleted by other objects so make destructor
  // private and DeleteHelper a friend.
  friend class base::DeleteHelper<StoragePartitionCodeCacheDataRemover>;

  explicit StoragePartitionCodeCacheDataRemover(
      GeneratedCodeCacheContext* generated_code_cache_context);

  ~StoragePartitionCodeCacheDataRemover();

  // Executed on UI thread.
  void ClearedCodeCache();

  // Executed on IO thread.
  void ClearJSCodeCache();
  void ClearWASMCodeCache(int rv);
  void DoneClearCodeCache(int rv);

  const scoped_refptr<GeneratedCodeCacheContext> generated_code_cache_context_;

  base::OnceClosure done_callback_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionCodeCacheDataRemover);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_DATA_STORAGE_PARTITION_CODE_CACHE_DATA_REMOVER_H_
