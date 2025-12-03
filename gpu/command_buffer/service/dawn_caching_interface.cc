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
#include "gpu/config/gpu_preferences.h"
#include "net/base/io_buffer.h"

namespace gpu::webgpu {

DawnCachingInterface::DawnCachingInterface(scoped_refptr<MemoryCache> backend,
                                           CacheBlobCallback callback)
    : memory_cache_backend_(std::move(backend)),
      cache_blob_callback_(std::move(callback)) {}

DawnCachingInterface::~DawnCachingInterface() = default;

size_t DawnCachingInterface::LoadData(const void* key,
                                      size_t key_size,
                                      void* value_out,
                                      size_t value_size) {
  if (memory_cache() == nullptr) {
    return 0u;
  }

  std::string_view key_str(static_cast<const char*>(key), key_size);
  auto entry = memory_cache()->Find(key_str);
  if (!entry) {
    return 0u;
  }
  return entry->ReadData(value_out, value_size);
}

void DawnCachingInterface::StoreData(const void* key,
                                     size_t key_size,
                                     const void* value,
                                     size_t value_size) {
  if (memory_cache() == nullptr || value == nullptr || value_size <= 0) {
    return;
  }

  std::string key_str(static_cast<const char*>(key), key_size);
  memory_cache()->Store(
      key_str, UNSAFE_BUFFERS(
                   base::span(static_cast<const uint8_t*>(value), value_size)));

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
