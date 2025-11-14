// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_PERSISTENT_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_PERSISTENT_CACHE_H_

#include <dawn/platform/DawnPlatform.h>

#include <atomic>
#include <memory>
#include <string_view>

#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/sequenced_task_runner.h"
#include "components/persistent_cache/backend_params.h"
#include "gpu/gpu_gles2_export.h"

namespace persistent_cache {
class PersistentCache;
class Entry;
}  // namespace persistent_cache

namespace gpu {

// Wraps a persistent_cache::PersistentCache to be used as a Dawn cache.
class GPU_GLES2_EXPORT GpuPersistentCache
    : public dawn::platform::CachingInterface {
 public:
  struct GPU_GLES2_EXPORT AsyncDiskWriteOpts {
    AsyncDiskWriteOpts();
    AsyncDiskWriteOpts(const AsyncDiskWriteOpts&);
    AsyncDiskWriteOpts(AsyncDiskWriteOpts&&);
    ~AsyncDiskWriteOpts();
    AsyncDiskWriteOpts& operator=(const AsyncDiskWriteOpts&);
    AsyncDiskWriteOpts& operator=(AsyncDiskWriteOpts&&);

    // The task runner to use for asynchronous writes. If null, writes will be
    // synchronous.
    scoped_refptr<base::SequencedTaskRunner> task_runner;
    // The maximum number of bytes that can be pending for an asynchronous
    // write. If the pending bytes exceed this limit, the write will be
    // performed after the initial delay and will not be rescheduled even if the
    // cache is not idle.
    size_t max_pending_bytes_to_write = std::numeric_limits<size_t>::max();
  };

  // If `async_write_options.task_runner` is null, then writes are synchronous.
  explicit GpuPersistentCache(std::string_view cache_prefix,
                              AsyncDiskWriteOpts async_write_options = {});
  ~GpuPersistentCache() override;

  GpuPersistentCache(const GpuPersistentCache&) = delete;
  GpuPersistentCache& operator=(const GpuPersistentCache&) = delete;

  // This can only be called once but is thread safe w.r.t loads and stores.
  void InitializeCache(persistent_cache::BackendParams backend_params);

  // dawn::platform::CachingInterface implementation.
  size_t LoadData(const void* key,
                  size_t key_size,
                  void* value,
                  size_t value_size) override;
  void StoreData(const void* key,
                 size_t key_size,
                 const void* value,
                 size_t value_size) override;

  std::unique_ptr<persistent_cache::Entry> LoadEntry(std::string_view key);

 private:
  struct DiskCache;

  // Prefix to prepend to UMA histogram's name. e.g GraphiteDawn, WebGPU
  const std::string cache_prefix_;

  std::atomic<size_t> load_count_ = 0;
  std::atomic<size_t> store_count_ = 0;

  base::AtomicFlag initialized_;
  scoped_refptr<DiskCache> disk_cache_;
  const AsyncDiskWriteOpts async_write_options_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_PERSISTENT_CACHE_H_
