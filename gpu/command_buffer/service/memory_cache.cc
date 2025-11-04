// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/memory_cache.h"

#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "gpu/command_buffer/service/service_utils.h"

namespace gpu {

MemoryCache::Entry::Entry(std::string key, const void* value, size_t value_size)
    : key_(std::move(key)),
      data_(static_cast<const uint8_t*>(value),
            UNSAFE_BUFFERS(static_cast<const uint8_t*>(value) + value_size)) {}

MemoryCache::Entry::~Entry() = default;

std::string_view MemoryCache::Entry::Key() const {
  return key_;
}

size_t MemoryCache::Entry::TotalSize() const {
  return key_.length() + data_.size();
}

size_t MemoryCache::Entry::DataSize() const {
  return data_.size();
}

size_t MemoryCache::Entry::ReadData(void* value_out, size_t value_size) const {
  // First handle "peek" case where use is trying to get the size of the entry.
  if (value_out == nullptr && value_size == 0) {
    return DataSize();
  }

  // Otherwise, verify that the size that is being copied out is identical.
  DCHECK(value_size == DataSize());
  std::copy(data_.begin(), data_.end(), static_cast<uint8_t*>(value_out));
  return value_size;
}

base::span<const uint8_t> MemoryCache::Entry::Data() const {
  return data_;
}

bool operator<(const std::unique_ptr<MemoryCache::Entry>& lhs,
               const std::unique_ptr<MemoryCache::Entry>& rhs) {
  return lhs->Key() < rhs->Key();
}

bool operator<(const std::unique_ptr<MemoryCache::Entry>& lhs,
               std::string_view rhs) {
  return lhs->Key() < rhs;
}

bool operator<(std::string_view lhs,
               const std::unique_ptr<MemoryCache::Entry>& rhs) {
  return lhs < rhs->Key();
}

MemoryCache::MemoryCache(size_t max_size,
                         std::string_view cache_hit_trace_event)
    : max_size_(max_size), cache_hit_trace_event_(cache_hit_trace_event) {}

MemoryCache::~MemoryCache() = default;

size_t MemoryCache::LoadData(std::string_view key,
                             void* value_out,
                             size_t value_size) {
  // Because we are tracking LRU, even loads modify internal state so mutex is
  // required.
  base::AutoLock lock(mutex_);

  auto it = entries_.find(key);
  if (it == entries_.end()) {
    return 0u;
  }

  if (value_size > 0 && !cache_hit_trace_event_.empty()) {
    TRACE_EVENT0("gpu", cache_hit_trace_event_.c_str());
  }

  // Even if this was just a "peek" operation to get size, the entry was
  // accessed so move it to the back of the eviction queue.
  std::unique_ptr<Entry>& entry = *it;
  entry->RemoveFromList();
  lru_.Append(entry.get());
  return entry->ReadData(value_out, value_size);
}

void MemoryCache::StoreData(std::string_view key,
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

void MemoryCache::EvictEntry(MemoryCache::Entry* entry) {
  // Always remove the entry from the LRU first because removing it from the
  // entry map will cause the entry to be destroyed.
  entry->RemoveFromList();

  // Update the size information.
  current_size_ -= entry->TotalSize();

  // Finally remove the entry from the map thereby destroying the entry.
  entries_.erase(entry->Key());
}
}  // namespace gpu
