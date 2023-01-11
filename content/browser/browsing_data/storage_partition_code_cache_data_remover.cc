// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/storage_partition_code_cache_data_remover.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/browser/browsing_data/conditional_cache_deletion_helper.h"
#include "content/browser/code_cache/generated_code_cache.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "net/base/completion_repeating_callback.h"
#include "net/disk_cache/disk_cache.h"

namespace content {

StoragePartitionCodeCacheDataRemover::StoragePartitionCodeCacheDataRemover(
    GeneratedCodeCacheContext* generated_code_cache_context,
    base::RepeatingCallback<bool(const GURL&)> url_predicate,
    base::Time begin_time,
    base::Time end_time)
    : generated_code_cache_context_(generated_code_cache_context),
      begin_time_(begin_time),
      end_time_(end_time),
      url_predicate_(std::move(url_predicate)) {}

// static
StoragePartitionCodeCacheDataRemover*
StoragePartitionCodeCacheDataRemover::Create(
    content::StoragePartition* storage_partition,
    base::RepeatingCallback<bool(const GURL&)> url_predicate,
    base::Time begin_time,
    base::Time end_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return new StoragePartitionCodeCacheDataRemover(
      storage_partition->GetGeneratedCodeCacheContext(),
      std::move(url_predicate), begin_time, end_time);
}

StoragePartitionCodeCacheDataRemover::~StoragePartitionCodeCacheDataRemover() {}

void StoragePartitionCodeCacheDataRemover::Remove(
    base::OnceClosure done_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!done_callback.is_null())
      << __func__ << " called with a null callback";
  done_callback_ = std::move(done_callback);

  GeneratedCodeCacheContext::RunOrPostTask(
      generated_code_cache_context_, FROM_HERE,
      base::BindOnce(&StoragePartitionCodeCacheDataRemover::ClearJSCodeCache,
                     base::Unretained(this)));
}

void StoragePartitionCodeCacheDataRemover::ClearedCodeCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(done_callback_).Run();
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void StoragePartitionCodeCacheDataRemover::ClearCache(
    net::CompletionOnceCallback callback,
    disk_cache::Backend* backend) {
  if (backend == nullptr) {
    std::move(callback).Run(net::ERR_FAILED);
    return;
  }

  // Create a split version of callback so that we can use it synchronously in
  // case result != net::ERR_IO_PENDING.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  int result = net::ERR_FAILED;
  if (!url_predicate_.is_null()) {
    result =
        (new ConditionalCacheDeletionHelper(
             backend, ConditionalCacheDeletionHelper::CreateURLAndTimeCondition(
                          std::move(url_predicate_),
                          base::BindRepeating(
                              &GeneratedCodeCache::GetResourceURLFromKey),
                          begin_time_, end_time_)))
            ->DeleteAndDestroySelfWhenFinished(std::move(split_callback.first));
  } else if (begin_time_.is_null() && end_time_.is_max()) {
    result = backend->DoomAllEntries(std::move(split_callback.first));
  } else {
    result = backend->DoomEntriesBetween(begin_time_, end_time_,
                                         std::move(split_callback.first));
  }
  // When result is ERR_IO_PENDING the callback would be called after the
  // operation has finished.
  if (result != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(result);
  }
}

void StoragePartitionCodeCacheDataRemover::ClearJSCodeCache() {
  if (generated_code_cache_context_ &&
      generated_code_cache_context_->generated_js_code_cache()) {
    generated_code_cache_context_->generated_js_code_cache()
        ->ClearInMemoryCache();

    net::CompletionOnceCallback callback = base::BindOnce(
        &StoragePartitionCodeCacheDataRemover::ClearWASMCodeCache,
        base::Unretained(this));
    generated_code_cache_context_->generated_js_code_cache()->GetBackend(
        base::BindOnce(&StoragePartitionCodeCacheDataRemover::ClearCache,
                       base::Unretained(this), std::move(callback)));
  } else {
    // When there is no JS cache, see if we need to remove WASM cache. When
    // there is JS cache, the WASM cache would be removed after the JS cache.
    ClearWASMCodeCache(net::ERR_FAILED);
  }
}

// |rv| is the returned when clearing the code cache. We don't handle
// any errors here, so the result value is ignored.
void StoragePartitionCodeCacheDataRemover::ClearWASMCodeCache(int rv) {
  if (generated_code_cache_context_ &&
      generated_code_cache_context_->generated_wasm_code_cache()) {
    generated_code_cache_context_->generated_wasm_code_cache()
        ->ClearInMemoryCache();

    net::CompletionOnceCallback callback = base::BindOnce(
        &StoragePartitionCodeCacheDataRemover::ClearWebUIJSCodeCache,
        base::Unretained(this));
    generated_code_cache_context_->generated_wasm_code_cache()->GetBackend(
        base::BindOnce(&StoragePartitionCodeCacheDataRemover::ClearCache,
                       base::Unretained(this), std::move(callback)));
  } else {
    // There is no Wasm cache, so move on to the next step.
    ClearWebUIJSCodeCache(net::ERR_FAILED);
  }
}

// |rv| is the returned when clearing the code cache. We don't handle
// any errors here, so the result value is ignored.
void StoragePartitionCodeCacheDataRemover::ClearWebUIJSCodeCache(int rv) {
  if (generated_code_cache_context_ &&
      generated_code_cache_context_->generated_webui_js_code_cache()) {
    generated_code_cache_context_->generated_webui_js_code_cache()
        ->ClearInMemoryCache();

    net::CompletionOnceCallback callback = base::BindOnce(
        &StoragePartitionCodeCacheDataRemover::DoneClearCodeCache,
        base::Unretained(this));
    generated_code_cache_context_->generated_webui_js_code_cache()->GetBackend(
        base::BindOnce(&StoragePartitionCodeCacheDataRemover::ClearCache,
                       base::Unretained(this), std::move(callback)));
  } else {
    // There is no WebUI JS cache, done with clearing caches.
    DoneClearCodeCache(net::ERR_FAILED);
  }
}

// |rv| is the returned when clearing the code cache. We don't handle
// any errors here, so the result value is ignored.
void StoragePartitionCodeCacheDataRemover::DoneClearCodeCache(int rv) {
  // Notify the UI thread that we are done.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&StoragePartitionCodeCacheDataRemover::ClearedCodeCache,
                     base::Unretained(this)));
}

}  // namespace content
