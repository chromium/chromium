// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_caching_interface.h"

#include <cstring>
#include <string_view>
#include <variant>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_event.h"
#include "components/persistent_cache/entry.h"
#include "gpu/command_buffer/service/gpu_persistent_cache.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_preferences.h"
#include "net/base/io_buffer.h"

namespace gpu::webgpu {

DawnCachingInterface::DawnCachingInterface(
    scoped_refptr<detail::DawnMemoryCache> backend,
    CacheBlobCallback callback)
    : memory_cache_backend_(std::move(backend)),
      cache_blob_callback_(std::move(callback)) {}

DawnCachingInterface::DawnCachingInterface(
    scoped_refptr<detail::DawnMemoryCache> backend,
    std::unique_ptr<GpuPersistentCache> persistent_cache)
    : memory_cache_backend_(std::move(backend)),
      persistent_cache_(std::move(persistent_cache)) {}

DawnCachingInterface::~DawnCachingInterface() = default;

void DawnCachingInterface::InitializePersistentCache(
    persistent_cache::BackendParams backend_params) {
  CHECK(persistent_cache_);
  // TODO(crbug.com/399642827): PersistentCache's sqlite backend has default
  // in-memory page cache of 2 MB.
  // See https://www.sqlite.org/pragma.html#pragma_cache_size
  // Since we have our own memory cache here, we might want to disable the
  // page cache or at least reduce its max size.
  persistent_cache_->InitializeCache(std::move(backend_params));
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

  std::unique_ptr<persistent_cache::Entry> entry =
      persistent_cache_->LoadEntry(key_str);
  if (!entry) {
    return 0u;
  }

  size_t bytes_copied = 0;
  if (value_size > 0) {
    bytes_copied = entry->CopyContentTo(
        UNSAFE_TODO(base::span(static_cast<uint8_t*>(value_out), value_size)));
  }

  if (memory_cache()) {
    memory_cache()->StoreData(key_str, entry->GetContentSpan().data(),
                              entry->GetContentSize());
  }

  if (bytes_copied > 0) {
    return bytes_copied;
  }

  return entry->GetContentSize();
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

scoped_refptr<detail::DawnMemoryCache>
DawnCachingInterfaceFactory::GetOrCreateMemoryCache(
    const gpu::GpuDiskCacheHandle& handle) {
  DCHECK(gpu::GetHandleType(handle) == gpu::GpuDiskCacheType::kDawnWebGPU ||
         gpu::GetHandleType(handle) == gpu::GpuDiskCacheType::kDawnGraphite);

  if (const auto it = backends_.find(handle); it != backends_.end()) {
    return it->second;
  }

  scoped_refptr<detail::DawnMemoryCache> backend = backend_factory_.Run();
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

scoped_refptr<detail::DawnMemoryCache>
DawnCachingInterfaceFactory::CreateDefaultInMemoryBackend() {
  return base::MakeRefCounted<detail::DawnMemoryCache>(
      GetDefaultGpuDiskCacheSize());
}

namespace detail {

DawnMemoryCache::Entry::Entry(std::string key,
                              const void* value,
                              size_t value_size)
    : key_(std::move(key)),
      data_(static_cast<const char*>(value), value_size) {}

DawnMemoryCache::Entry::~Entry() = default;

std::string_view DawnMemoryCache::Entry::Key() const {
  return key_;
}

size_t DawnMemoryCache::Entry::TotalSize() const {
  return key_.length() + data_.length();
}

size_t DawnMemoryCache::Entry::DataSize() const {
  return data_.length();
}

size_t DawnMemoryCache::Entry::ReadData(void* value_out,
                                        size_t value_size) const {
  // First handle "peek" case where use is trying to get the size of the entry.
  if (value_out == nullptr && value_size == 0) {
    return DataSize();
  }

  // Otherwise, verify that the size that is being copied out is identical.
  TRACE_EVENT0("gpu", "DawnCachingInterface::CacheHit");
  DCHECK(value_size == DataSize());
  data_.copy(static_cast<char*>(value_out), value_size);
  return value_size;
}

bool operator<(const std::unique_ptr<DawnMemoryCache::Entry>& lhs,
               const std::unique_ptr<DawnMemoryCache::Entry>& rhs) {
  return lhs->Key() < rhs->Key();
}

bool operator<(const std::unique_ptr<DawnMemoryCache::Entry>& lhs,
               std::string_view rhs) {
  return lhs->Key() < rhs;
}

bool operator<(std::string_view lhs,
               const std::unique_ptr<DawnMemoryCache::Entry>& rhs) {
  return lhs < rhs->Key();
}

DawnMemoryCache::DawnMemoryCache(size_t max_size) : max_size_(max_size) {}

DawnMemoryCache::~DawnMemoryCache() = default;

size_t DawnMemoryCache::LoadData(std::string_view key,
                                 void* value_out,
                                 size_t value_size) {
  // Because we are tracking LRU, even loads modify internal state so mutex is
  // required.
  base::AutoLock lock(mutex_);

  auto it = entries_.find(key);
  if (it == entries_.end()) {
    return 0u;
  }

  // Even if this was just a "peek" operation to get size, the entry was
  // accessed so move it to the back of the eviction queue.
  std::unique_ptr<Entry>& entry = *it;
  entry->RemoveFromList();
  lru_.Append(entry.get());
  return entry->ReadData(value_out, value_size);
}

void DawnMemoryCache::StoreData(std::string_view key,
                                const void* value,
                                size_t value_size) {
  // Don't need to do anything if we are not storing anything.
  if (value == nullptr || value_size == 0) {
    return;
  }

  base::AutoLock lock(mutex_);

  // If an entry for this key already exists, first evict the existing entry.
  if (auto it = entries_.find(key); it != entries_.end()) {
    const std::unique_ptr<Entry>& entry = *it;
    EvictEntry(entry.get());
  }

  // If the entry is too large for the cache, we cannot store it so skip. We
  // avoid creating the entry here early since it would incur unneeded large
  // copies.
  size_t entry_size = key.length() + value_size;
  if (entry_size >= max_size_) {
    return;
  }

  // Evict least used entries until we have enough room to add the new entry.
  auto entry = std::make_unique<Entry>(std::string(key), value, value_size);
  DCHECK(entry->TotalSize() == entry_size);
  while (current_size_ + entry_size > max_size_) {
    EvictEntry(lru_.head()->value());
  }

  // Add the entry size to the overall size and update the eviction queue.
  current_size_ += entry->TotalSize();
  lru_.Append(entry.get());

  auto [it, inserted] = entries_.insert(std::move(entry));
  DCHECK(inserted);
}

void DawnMemoryCache::PurgeMemory(
    base::MemoryPressureLevel memory_pressure_level) {
  base::AutoLock lock(mutex_);
  size_t new_limit = gpu::UpdateShaderCacheSizeOnMemoryPressure(
      max_size_, memory_pressure_level);
  // Evict the least recently used entries until we reach the `new_limit`
  while (current_size_ > new_limit) {
    EvictEntry(lru_.head()->value());
  }
}

void DawnMemoryCache::OnMemoryDump(const std::string& dump_name,
                                   base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(mutex_);

  using base::trace_event::MemoryAllocatorDump;
  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, current_size_);
  dump->AddScalar(MemoryAllocatorDump::kNameObjectCount,
                  MemoryAllocatorDump::kUnitsObjects, entries_.size());
}

void DawnMemoryCache::EvictEntry(DawnMemoryCache::Entry* entry) {
  // Always remove the entry from the LRU first because removing it from the
  // entry map will cause the entry to be destroyed.
  entry->RemoveFromList();

  // Update the size information.
  current_size_ -= entry->TotalSize();

  // Finally remove the entry from the map thereby destroying the entry.
  entries_.erase(entry->Key());
}

}  // namespace detail

}  // namespace gpu::webgpu
