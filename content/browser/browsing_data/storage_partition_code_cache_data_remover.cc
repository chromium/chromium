// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/storage_partition_code_cache_data_remover.h"

#include "base/location.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/browsing_data/conditional_cache_deletion_helper.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"

namespace content {

StoragePartitionCodeCacheDataRemover::StoragePartitionCodeCacheDataRemover(
    GeneratedCodeCacheContext* generated_code_cache_context)
    : generated_code_cache_context_(generated_code_cache_context) {}

// static
StoragePartitionCodeCacheDataRemover*
StoragePartitionCodeCacheDataRemover::Create(
    content::StoragePartition* storage_partition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return new StoragePartitionCodeCacheDataRemover(
      storage_partition->GetGeneratedCodeCacheContext());
}

StoragePartitionCodeCacheDataRemover::~StoragePartitionCodeCacheDataRemover() {}

void StoragePartitionCodeCacheDataRemover::Remove(
    base::OnceClosure done_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!done_callback.is_null())
      << __func__ << " called with a null callback";
  done_callback_ = std::move(done_callback);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&StoragePartitionCodeCacheDataRemover::ClearJSCodeCache,
                     base::Unretained(this)));
}

void StoragePartitionCodeCacheDataRemover::ClearedCodeCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(done_callback_).Run();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void StoragePartitionCodeCacheDataRemover::ClearJSCodeCache() {
  int status = net::ERR_FAILED;
  if (generated_code_cache_context_ &&
      generated_code_cache_context_->generated_js_code_cache()) {
    // TODO(crbug.com/866419): Currently we just clear entire caches.
    // Change it to conditionally clear entries based on the filters.
    // Likewise for WASM code cache.
    status =
        generated_code_cache_context_->generated_js_code_cache()->ClearCache(
            base::BindRepeating(
                &StoragePartitionCodeCacheDataRemover::ClearWASMCodeCache,
                base::Unretained(this)));
  }
  // The callback would be called by the cache backend once the request is
  // serviced.
  if (status != net::ERR_IO_PENDING) {
    ClearWASMCodeCache(status);
  }
}

// |rv| is the returned when clearing the code cache. We don't handle
// any errors here, so the result value is ignored.
void StoragePartitionCodeCacheDataRemover::ClearWASMCodeCache(int rv) {
  int status = net::ERR_FAILED;
  if (generated_code_cache_context_ &&
      generated_code_cache_context_->generated_wasm_code_cache()) {
    // TODO(crbug.com/866419): Currently we just clear entire caches.
    // Change it to conditionally clear entries based on the filters.
    // Likewise for JS code cache.
    status =
        generated_code_cache_context_->generated_wasm_code_cache()->ClearCache(
            base::BindRepeating(
                &StoragePartitionCodeCacheDataRemover::DoneClearCodeCache,
                base::Unretained(this)));
  }
  // The callback would be called by the cache backend once the request is
  // serviced.
  if (status != net::ERR_IO_PENDING) {
    DoneClearCodeCache(status);
  }
}

// |rv| is the returned when clearing the code cache. We don't handle
// any errors here, so the result value is ignored.
void StoragePartitionCodeCacheDataRemover::DoneClearCodeCache(int rv) {
  // Notify the UI thread that we are done.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&StoragePartitionCodeCacheDataRemover::ClearedCodeCache,
                     base::Unretained(this)));
}

}  // namespace content
