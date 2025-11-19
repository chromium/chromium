// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/memory_cache.h"

#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "gpu/command_buffer/service/service_utils.h"

namespace gpu {

MemoryCacheEntry::MemoryCacheEntry(std::string_view key,
                                   base::span<const uint8_t> data)
    : key_(key), data_(base::HeapArray<uint8_t>::CopiedFrom(data)) {}

MemoryCacheEntry::MemoryCacheEntry(std::string_view key,
                                   base::HeapArray<uint8_t> data)
    : key_(key), data_(std::move(data)) {}

MemoryCacheEntry::~MemoryCacheEntry() = default;

std::string_view MemoryCacheEntry::Key() const {
  return key_;
}

size_t MemoryCacheEntry::TotalSize() const {
  return key_.length() + data_.size();
}

size_t MemoryCacheEntry::DataSize() const {
  return data_.size();
}

size_t MemoryCacheEntry::ReadData(void* value_out, size_t value_size) const {
  // First handle "peek" case where use is trying to get the size of the entry.
  if (value_out == nullptr && value_size == 0) {
    return DataSize();
  }

  // Otherwise, verify that the size that is being copied out is identical.
  DCHECK(value_size == DataSize());
  std::copy(data_.begin(), data_.end(), static_cast<uint8_t*>(value_out));
  return value_size;
}

base::span<const uint8_t> MemoryCacheEntry::Data() const {
  return data_;
}

bool operator<(const scoped_refptr<MemoryCacheEntry>& lhs,
               const scoped_refptr<MemoryCacheEntry>& rhs) {
  return lhs->Key() < rhs->Key();
}

bool operator<(const scoped_refptr<MemoryCacheEntry>& lhs,
               std::string_view rhs) {
  return lhs->Key() < rhs;
}

bool operator<(std::string_view lhs,
               const scoped_refptr<MemoryCacheEntry>& rhs) {
  return lhs < rhs->Key();
}

MemoryCache::MemoryCache(size_t max_size,
                         std::string_view cache_hit_trace_event)
    : max_size_(max_size), cache_hit_trace_event_(cache_hit_trace_event) {}

MemoryCache::~MemoryCache() = default;

scoped_refptr<MemoryCacheEntry> MemoryCache::Find(std::string_view key) {
  // Because we are tracking LRU, even loads modify internal state so mutex is
  // required.
  base::AutoLock lock(mutex_);

  auto it = entries_.find(key);
  if (it == entries_.end()) {
    return nullptr;
  }

  if (!cache_hit_trace_event_.empty()) {
    TRACE_EVENT0("gpu", cache_hit_trace_event_.c_str());
  }

  // Even if this was just a "peek" operation to get size, the entry was
  // accessed so move it to the back of the eviction queue.
  scoped_refptr<MemoryCacheEntry>& entry = *it;
  entry->RemoveFromList();
  lru_.Append(entry.get());
  return entry;
}

scoped_refptr<MemoryCacheEntry> MemoryCache::Store(
    std::string_view key,
    base::span<const uint8_t> data) {
  base::AutoLock lock(mutex_);

  EvictEntry(key);

  if (!CanFitMemoryCacheEntry(key.size() + data.size())) {
    return nullptr;
  }

  auto entry = base::MakeRefCounted<MemoryCacheEntry>(key, data);
  InsertEntry(entry);
  return entry;
}

scoped_refptr<MemoryCacheEntry> MemoryCache::Store(
    std::string_view key,
    base::HeapArray<uint8_t> data) {
  base::AutoLock lock(mutex_);

  EvictEntry(key);

  if (!CanFitMemoryCacheEntry(key.size() + data.size())) {
    return nullptr;
  }

  auto entry = base::MakeRefCounted<MemoryCacheEntry>(key, std::move(data));
  InsertEntry(entry);
  return entry;
}

void MemoryCache::PurgeMemory(base::MemoryPressureLevel memory_pressure_level) {
  base::AutoLock lock(mutex_);
  size_t new_limit = gpu::UpdateShaderCacheSizeOnMemoryPressure(
      max_size_, memory_pressure_level);
  // Evict the least recently used entries until we reach the `new_limit`
  while (current_size_ > new_limit) {
    EvictEntry(lru_.head()->value());
  }
}

void MemoryCache::OnMemoryDump(const std::string& dump_name,
                               base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(mutex_);

  using base::trace_event::MemoryAllocatorDump;
  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, current_size_);
  dump->AddScalar(MemoryAllocatorDump::kNameObjectCount,
                  MemoryAllocatorDump::kUnitsObjects, entries_.size());
}

void MemoryCache::EvictEntry(std::string_view key) {
  if (auto it = entries_.find(key); it != entries_.end()) {
    const scoped_refptr<MemoryCacheEntry>& entry = *it;
    EvictEntry(entry.get());
  }
}

void MemoryCache::EvictEntry(MemoryCacheEntry* entry) {
  // Always remove the entry from the LRU first because removing it from the
  // entry map will cause the entry to be destroyed.
  entry->RemoveFromList();

  // Update the size information.
  current_size_ -= entry->TotalSize();

  // Finally remove the entry from the map thereby destroying the entry.
  entries_.erase(entry->Key());
}

void MemoryCache::InsertEntry(scoped_refptr<MemoryCacheEntry> entry) {
  // Evict least used entries until we have enough room to add the new entry.
  while (current_size_ + entry->TotalSize() > max_size_) {
    EvictEntry(lru_.head()->value());
  }

  // Add the entry size to the overall size and update the eviction queue.
  current_size_ += entry->TotalSize();
  lru_.Append(entry.get());

  auto [it, inserted] = entries_.insert(entry);
  DCHECK(inserted);
}

bool MemoryCache::CanFitMemoryCacheEntry(size_t data_size) const {
  // Don't need to do anything if we are not storing anything.
  if (data_size == 0) {
    return false;
  }

  // If this entry is larger than we can make space for, don't allow it to be
  // stored.
  if (data_size >= max_size_) {
    return false;
  }

  return true;
}

}  // namespace gpu
