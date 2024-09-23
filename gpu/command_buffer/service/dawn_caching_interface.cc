// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_caching_interface.h"

#include <cstring>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "gpu/config/gpu_preferences.h"
#include "net/base/io_buffer.h"

namespace gpu::webgpu {

DawnCachingInterface::DawnCachingInterface(
    scoped_refptr<detail::DawnCachingBackend> backend,
    CacheBlobCallback callback)
    : backend_(std::move(backend)), cache_blob_callback_(std::move(callback)) {}

DawnCachingInterface::~DawnCachingInterface() = default;

size_t DawnCachingInterface::LoadData(const void* key,
                                      size_t key_size,
                                      void* value_out,
                                      size_t value_size) {
  if (backend() == nullptr) {
    return 0u;
  }
  std::string key_str(static_cast<const char*>(key), key_size);
  return backend()->LoadData(key_str, value_out, value_size);
}

void DawnCachingInterface::StoreData(const void* key,
                                     size_t key_size,
                                     const void* value,
                                     size_t value_size) {
  if (backend() == nullptr || value == nullptr || value_size <= 0) {
    return;
  }
  std::string key_str(static_cast<const char*>(key), key_size);
  backend()->StoreData(key_str, value, value_size);

  // Send the cache entry to be stored on the host-side if applicable.
  if (cache_blob_callback_) {
    std::string value_str(static_cast<const char*>(value), value_size);
    cache_blob_callback_.Run(gpu::GpuDiskCacheType::kDawnWebGPU, key_str,
                             value_str);
  }
}

DawnCachingInterfaceFactory::DawnCachingInterfaceFactory(BackendFactory factory)
    : backend_factory_(factory) {}

DawnCachingInterfaceFactory::DawnCachingInterfaceFactory()
    : DawnCachingInterfaceFactory(base::BindRepeating(
          &DawnCachingInterfaceFactory::CreateDefaultInMemoryBackend)) {}

DawnCachingInterfaceFactory::~DawnCachingInterfaceFactory() = default;

std::unique_ptr<DawnCachingInterface>
DawnCachingInterfaceFactory::CreateInstance(
    const gpu::GpuDiskCacheHandle& handle,
    DawnCachingInterface::CacheBlobCallback callback) {
  DCHECK(gpu::GetHandleType(handle) == gpu::GpuDiskCacheType::kDawnWebGPU ||
         gpu::GetHandleType(handle) == gpu::GpuDiskCacheType::kDawnGraphite);

  if (const auto it = backends_.find(handle); it != backends_.end()) {
    return base::WrapUnique(
        new DawnCachingInterface(it->second, std::move(callback)));
  }

  scoped_refptr<detail::DawnCachingBackend> backend = backend_factory_.Run();
  if (backend != nullptr) {
    backends_[handle] = backend;
  }
  return base::WrapUnique(
      new DawnCachingInterface(std::move(backend), std::move(callback)));
}

std::unique_ptr<DawnCachingInterface>
DawnCachingInterfaceFactory::CreateInstance() {
  return base::WrapUnique(new DawnCachingInterface(backend_factory_.Run()));
}

void DawnCachingInterfaceFactory::ReleaseHandle(
    const gpu::GpuDiskCacheHandle& handle) {
  DCHECK(gpu::GetHandleType(handle) == gpu::GpuDiskCacheType::kDawnWebGPU ||
         gpu::GetHandleType(handle) == gpu::GpuDiskCacheType::kDawnGraphite);

  backends_.erase(handle);
}

scoped_refptr<detail::DawnCachingBackend>
DawnCachingInterfaceFactory::CreateDefaultInMemoryBackend() {
  return base::MakeRefCounted<detail::DawnCachingBackend>(
      GetDefaultGpuDiskCacheSize());
}

namespace detail {

DawnCachingBackend::Entry::Entry(const std::string& key,
                                 const void* value,
                                 size_t value_size)
    : key_(key), data_(static_cast<const char*>(value), value_size) {}

const std::string& DawnCachingBackend::Entry::Key() const {
  return key_;
}

size_t DawnCachingBackend::Entry::TotalSize() const {
  return key_.length() + data_.length();
}

size_t DawnCachingBackend::Entry::DataSize() const {
  return data_.length();
}

size_t DawnCachingBackend::Entry::ReadData(void* value_out,
                                           size_t value_size) const {
  // First handle "peek" case where use is trying to get the size of the entry.
  if (value_out == nullptr && value_size == 0) {
    return DataSize();
  }

  // Otherwise, verify that the size that is being copied out is identical.
  TRACE_EVENT0("gpu", "DawnCachingInterface::CacheHit");
  DCHECK(value_size == DataSize());
  memcpy(value_out, data_.data(), value_size);
  return value_size;
}

bool operator<(const std::unique_ptr<DawnCachingBackend::Entry>& lhs,
               const std::unique_ptr<DawnCachingBackend::Entry>& rhs) {
  return lhs->Key() < rhs->Key();
}

bool operator<(const std::unique_ptr<DawnCachingBackend::Entry>& lhs,
               const std::string& rhs) {
  return lhs->Key() < rhs;
}

bool operator<(const std::string& lhs,
               const std::unique_ptr<DawnCachingBackend::Entry>& rhs) {
  return lhs < rhs->Key();
}

DawnCachingBackend::DawnCachingBackend(size_t max_size) : max_size_(max_size) {}

DawnCachingBackend::~DawnCachingBackend() = default;

size_t DawnCachingBackend::LoadData(const std::string& key,
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

void DawnCachingBackend::StoreData(const std::string& key,
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
  auto entry = std::make_unique<Entry>(key, value, value_size);
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

void DawnCachingBackend::EvictEntry(DawnCachingBackend::Entry* entry) {
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
