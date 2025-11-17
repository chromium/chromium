// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_caching_interface.h"

#include <cstring>
#include <string_view>
#include <variant>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/gpu_persistent_cache.h"
#include "gpu/config/gpu_preferences.h"
#include "net/base/io_buffer.h"

namespace gpu::webgpu {

DawnCachingInterface::DawnCachingInterface(scoped_refptr<MemoryCache> backend,
                                           CacheBlobCallback callback)
    : memory_cache_backend_(std::move(backend)),
      cache_blob_callback_(std::move(callback)) {}

DawnCachingInterface::DawnCachingInterface(
    scoped_refptr<MemoryCache> backend,
    std::unique_ptr<GpuPersistentCache> persistent_cache)
    : memory_cache_backend_(std::move(backend)),
      persistent_cache_(std::move(persistent_cache)) {}

DawnCachingInterface::~DawnCachingInterface() = default;

void DawnCachingInterface::InitializePersistentCache(
    persistent_cache::BackendParams backend_params,
    scoped_refptr<RefCountedGpuProcessShmCount> use_shader_cache_shm_count) {
  CHECK(persistent_cache_);
  // TODO(crbug.com/399642827): PersistentCache's sqlite backend has default
  // in-memory page cache of 2 MB.
  // See https://www.sqlite.org/pragma.html#pragma_cache_size
  // Since we have our own memory cache here, we might want to disable the
  // page cache or at least reduce its max size.
  persistent_cache_->InitializeCache(std::move(backend_params),
                                     std::move(use_shader_cache_shm_count));
}

size_t DawnCachingInterface::LoadData(const void* key,
                                      size_t key_size,
                                      void* value_out,
                                      size_t value_size) {
  std::string_view key_str(static_cast<const char*>(key), key_size);
  if (memory_cache() != nullptr) {
    size_t bytes_read =
        memory_cache()->LoadData(key_str, value_out, value_size);
    if (bytes_read > 0) {
      return bytes_read;
    }
  }

  if (!persistent_cache_) {
    return 0u;
  }

  size_t discovered_size = 0;
  base::HeapArray<uint8_t> content_for_memory_cache;

  // A BufferProvider for PersistentCache that puts the size of the content, in
  // bytes, into `discovered_size` and returns one of:
  // 1.  a view into the buffer at `value_out` if it is big enough,
  // 2.  a view into a new base::HeapArray (`content_for_memory_cache`) if the
  //     memory cache exists, or
  // 3.  an empty span.
  // SAFETY: Caller provides either null `value_out` or `value_out` plus
  // `value_size`.
  auto buffer_provider = [value = UNSAFE_BUFFERS(base::span(
                              static_cast<uint8_t*>(value_out), value_size)),
                          with_memory_cache = memory_cache() != nullptr,
                          &discovered_size,
                          &content_for_memory_cache](size_t content_size) {
    // Cache hit: retain the size.
    discovered_size = content_size;

    if (value.size() >= content_size) {
      return value.first(content_size);  // Case 1.
    }

    if (content_size != 0 && with_memory_cache) {
      content_for_memory_cache = base::HeapArray<uint8_t>::Uninit(content_size);
      return base::span<uint8_t>(content_for_memory_cache);  // Case 2.
    }

    return base::span<uint8_t>();  // Case 3.
  };

  if (!persistent_cache_->LoadEntry(key_str, std::move(buffer_provider))) {
    return 0;  // Cache miss or error.
  }

  if (!discovered_size) {
    return 0;  // Cache hit with zero data.
  }

  // Cache hit. The content is already in `value_out` if it is big enough.

  if (memory_cache()) {
    if (!content_for_memory_cache.empty()) {
      // The provider copied the content into a dedicated buffer for the memory
      // cache.
      memory_cache()->StoreData(key_str, content_for_memory_cache.data(),
                                content_for_memory_cache.size());
    } else if (value_size >= discovered_size) {
      // Copy the content from `value_out` into the memory cache.
      memory_cache()->StoreData(key_str, value_out, discovered_size);
    }
  }

  return discovered_size;
}

void DawnCachingInterface::StoreData(const void* key,
                                     size_t key_size,
                                     const void* value,
                                     size_t value_size) {
  if (value == nullptr || value_size <= 0) {
    return;
  }

  std::string key_str(static_cast<const char*>(key), key_size);
  if (memory_cache() != nullptr) {
    memory_cache()->StoreData(key_str, value, value_size);
  }

  if (persistent_cache_) {
    persistent_cache_->StoreData(key_str.data(), key_str.size(), value,
                                 value_size);
  }

  // Send the cache entry to be stored on the host-side if applicable.
  if (cache_blob_callback_) {
    std::string value_str(static_cast<const char*>(value), value_size);
    cache_blob_callback_.Run(key_str, value_str);
  }
}

DawnCachingInterfaceFactory::DawnCachingInterfaceFactory(BackendFactory factory)
    : backend_factory_(factory) {
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "DawnCache", base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

DawnCachingInterfaceFactory::DawnCachingInterfaceFactory()
    : DawnCachingInterfaceFactory(base::BindRepeating(
          &DawnCachingInterfaceFactory::CreateDefaultInMemoryBackend)) {}

DawnCachingInterfaceFactory::~DawnCachingInterfaceFactory() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

std::unique_ptr<DawnCachingInterface>
DawnCachingInterfaceFactory::CreateInstance(
    const gpu::GpuDiskCacheHandle& handle,
    DawnCachingInterface::CacheBlobCallback callback) {
  return base::WrapUnique(new DawnCachingInterface(
      GetOrCreateMemoryCache(handle), std::move(callback)));
}

std::unique_ptr<DawnCachingInterface>
DawnCachingInterfaceFactory::CreateInstance(
    const gpu::GpuDiskCacheHandle& handle,
    std::unique_ptr<GpuPersistentCache> persistent_cache) {
  return base::WrapUnique(new DawnCachingInterface(
      GetOrCreateMemoryCache(handle), std::move(persistent_cache)));
}

std::unique_ptr<DawnCachingInterface>
DawnCachingInterfaceFactory::CreateInstance() {
  return base::WrapUnique(new DawnCachingInterface(backend_factory_.Run()));
}

scoped_refptr<MemoryCache> DawnCachingInterfaceFactory::GetOrCreateMemoryCache(
    const gpu::GpuDiskCacheHandle& handle) {
  DCHECK(gpu::GetHandleType(handle) == gpu::GpuDiskCacheType::kDawnWebGPU ||
         gpu::GetHandleType(handle) == gpu::GpuDiskCacheType::kDawnGraphite);

  if (const auto it = backends_.find(handle); it != backends_.end()) {
    return it->second;
  }

  scoped_refptr<MemoryCache> backend = backend_factory_.Run();
  if (backend != nullptr) {
    backends_[handle] = backend;
  }

  return backend;
}

void DawnCachingInterfaceFactory::ReleaseHandle(
    const gpu::GpuDiskCacheHandle& handle) {
  DCHECK(gpu::GetHandleType(handle) == gpu::GpuDiskCacheType::kDawnWebGPU ||
         gpu::GetHandleType(handle) == gpu::GpuDiskCacheType::kDawnGraphite);

  backends_.erase(handle);
}

void DawnCachingInterfaceFactory::PurgeMemory(
    base::MemoryPressureLevel memory_pressure_level) {
  for (auto& [key, backend] : backends_) {
    CHECK(std::holds_alternative<GpuDiskCacheDawnGraphiteHandle>(key) ||
          std::holds_alternative<GpuDiskCacheDawnWebGPUHandle>(key));
    backend->PurgeMemory(memory_pressure_level);
  }
}

bool DawnCachingInterfaceFactory::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  const bool is_background =
      args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::kBackground;
  for (auto& [key, backend] : backends_) {
    if (std::holds_alternative<GpuDiskCacheDawnGraphiteHandle>(key)) {
      // There should only be a single graphite cache.
      backend->OnMemoryDump("gpu/shader_cache/graphite_cache", pmd);
    } else if (!is_background &&
               std::holds_alternative<GpuDiskCacheDawnWebGPUHandle>(key)) {
      // Note that in memory only webgpu caches aren't stored in `backends_` so
      // they won't produce memory dumps.
      std::string dump_name = base::StringPrintf(
          "gpu/shader_cache/webgpu_cache_0x%X", GetHandleValue(key));
      backend->OnMemoryDump(dump_name, pmd);
    }
  }
  return true;
}

scoped_refptr<MemoryCache>
DawnCachingInterfaceFactory::CreateDefaultInMemoryBackend() {
  return base::MakeRefCounted<MemoryCache>(GetDefaultGpuDiskCacheSize(),
                                           "DawnCachingInterface::CacheHit");
}
}  // namespace gpu::webgpu
