// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_caching_interface.h"

#include <cstring>
#include <string_view>
#include <tuple>
#include <variant>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_event.h"
#include "gpu/config/gpu_preferences.h"
#include "net/base/io_buffer.h"

namespace wgpu::detail {
namespace {

using LoadCacheDataConverter =
    CArgConverter<WGPUDawnLoadCacheDataCallbackInfo,
                  std::tuple<std::string_view, base::span<uint8_t>>>;
using StoreCacheDataConverter =
    CArgConverter<WGPUDawnStoreCacheDataCallbackInfo,
                  std::tuple<std::string_view, base::span<const uint8_t>>>;

}  // namespace

// static
LoadCacheDataConverter::Result LoadCacheDataConverter::Convert(
    size_t key_size,
    const uint8_t* key,
    size_t value_size,
    uint8_t* value) {
  return std::make_tuple(
      std::string_view(reinterpret_cast<const char*>(key), key_size),
      // SAFETY: `key` and `value` are provided by Dawn with valid sizes.
      UNSAFE_BUFFERS(base::span<uint8_t>(value, value_size)));
}

// static
StoreCacheDataConverter::Result StoreCacheDataConverter::Convert(
    size_t key_size,
    const uint8_t* key,
    size_t value_size,
    const uint8_t* value) {
  return std::make_tuple(
      std::string_view(reinterpret_cast<const char*>(key), key_size),
      // SAFETY: `key` and `value` are provided by Dawn with valid sizes.
      UNSAFE_BUFFERS(base::span<const uint8_t>(value, value_size)));
}

}  // namespace wgpu::detail

namespace gpu::webgpu {

DawnInMemoryCache::DawnInMemoryCache(scoped_refptr<MemoryCache> backend,
                                     CacheBlobCallback callback)
    : memory_cache_backend_(std::move(backend)),
      cache_blob_callback_(std::move(callback)) {}

DawnInMemoryCache::~DawnInMemoryCache() = default;

size_t DawnInMemoryCache::FindKey(std::string_view key) {
  if (memory_cache() == nullptr) {
    return 0u;
  }
  auto entry = memory_cache()->Find(key);
  if (!entry) {
    return 0u;
  }
  return entry->DataSize();
}

size_t DawnInMemoryCache::LoadData(std::string_view key,
                                   base::span<uint8_t> dst) {
  if (memory_cache() == nullptr) {
    return 0u;
  }
  auto entry = memory_cache()->Find(key);
  if (!entry) {
    return 0u;
  }

  auto src = entry->Data();
  if (src.size() <= dst.size()) {
    dst.copy_prefix_from(src);
    return entry->DataSize();
  }
  return 0u;
}

void DawnInMemoryCache::StoreData(std::string_view key,
                                  base::span<const uint8_t> src) {
  if (memory_cache() == nullptr || src.empty()) {
    return;
  }
  memory_cache()->Store(key, src);

  // Send the cache entry to be stored on the host-side if applicable.
  if (cache_blob_callback_) {
    std::string key_str_copy(key);
    std::string src_str(reinterpret_cast<const char*>(src.data()), src.size());
    cache_blob_callback_.Run(key_str_copy, src_str);
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

std::unique_ptr<DawnInMemoryCache> DawnCachingInterfaceFactory::CreateInstance(
    const gpu::GpuDiskCacheHandle& handle,
    DawnInMemoryCache::CacheBlobCallback callback) {
  return base::WrapUnique(new DawnInMemoryCache(GetOrCreateMemoryCache(handle),
                                                std::move(callback)));
}

std::unique_ptr<DawnInMemoryCache>
DawnCachingInterfaceFactory::CreateInstance() {
  return base::WrapUnique(new DawnInMemoryCache(backend_factory_.Run()));
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
    backend->OnUpdateMemoryLimit(current_memory_limit_);
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

void DawnCachingInterfaceFactory::OnUpdateMemoryLimit(
    base::MemoryLimit memory_limit) {
  current_memory_limit_ = memory_limit;
  for (auto& [key, backend] : backends_) {
    backend->OnUpdateMemoryLimit(memory_limit);
  }
}

void DawnCachingInterfaceFactory::OnReleaseMemory(
    base::MemoryLimit memory_limit) {
  current_memory_limit_ = memory_limit;
  for (auto& [key, backend] : backends_) {
    CHECK(std::holds_alternative<GpuDiskCacheDawnGraphiteHandle>(key) ||
          std::holds_alternative<GpuDiskCacheDawnWebGPUHandle>(key));
    backend->OnReleaseMemory(memory_limit);
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
