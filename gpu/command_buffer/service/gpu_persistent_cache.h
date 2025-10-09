// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_PERSISTENT_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_PERSISTENT_CACHE_H_

#include <dawn/platform/DawnPlatform.h>

#include <memory>
#include <string_view>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
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
  explicit GpuPersistentCache(std::string_view cache_prefix);
  ~GpuPersistentCache() override;

  GpuPersistentCache(const GpuPersistentCache&) = delete;
  GpuPersistentCache& operator=(const GpuPersistentCache&) = delete;

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
  std::string GetHistogramName(std::string_view metric) const;

  // Prefix to prepend to UMA histogram's name. e.g GraphiteDawn, WebGPU
  const std::string cache_prefix_;

  size_t load_count_ = 0;
  size_t store_count_ = 0;

  base::Lock lock_;
  std::unique_ptr<persistent_cache::PersistentCache> persistent_cache_
      GUARDED_BY(lock_);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_PERSISTENT_CACHE_H_
