// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MEMORY_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_MEMORY_CACHE_H_

#include "base/containers/flat_set.h"
#include "base/containers/heap_array.h"
#include "base/containers/linked_list.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
// MemoryCacheEntry class for LRU tracking and holding key/value pair.
class GPU_GLES2_EXPORT MemoryCacheEntry
    : public base::LinkNode<MemoryCacheEntry>,
      public base::RefCountedThreadSafe<MemoryCacheEntry> {
 public:
  MemoryCacheEntry(std::string_view key, base::span<const uint8_t> data);
  MemoryCacheEntry(std::string_view key, base::HeapArray<uint8_t> data);

  std::string_view Key() const;

  size_t TotalSize() const;
  size_t DataSize() const;

  base::span<const uint8_t> Data() const;
  size_t ReadData(void* value_out, size_t value_size) const;

 private:
  friend class base::RefCountedThreadSafe<MemoryCacheEntry>;
  ~MemoryCacheEntry();

  const std::string key_;
  const base::HeapArray<uint8_t> data_;
};

class GPU_GLES2_EXPORT MemoryCache : public base::RefCounted<MemoryCache> {
 public:
  explicit MemoryCache(size_t max_size,
                       std::string_view cache_hit_trace_event = "");

  scoped_refptr<MemoryCacheEntry> Store(std::string_view key,
                                        base::span<const uint8_t> data);
  scoped_refptr<MemoryCacheEntry> Store(std::string_view key,
                                        base::HeapArray<uint8_t> data);
  scoped_refptr<MemoryCacheEntry> Find(std::string_view key);

  void PurgeMemory(base::MemoryPressureLevel memory_pressure_level);

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::ProcessMemoryDump* pmd);
  template <typename Fn>
  void ForEach(Fn fn) {
    base::AutoLock lock(mutex_);

    for (auto* node = lru_.head(); node != lru_.end(); node = node->next()) {
      fn(node->value());
    }
  }

 private:
  // Overrides for transparent flat_set lookups using a string.
  friend bool operator<(const scoped_refptr<MemoryCacheEntry>& lhs,
                        const scoped_refptr<MemoryCacheEntry>& rhs);
  friend bool operator<(const scoped_refptr<MemoryCacheEntry>& lhs,
                        std::string_view rhs);
  friend bool operator<(std::string_view lhs,
                        const scoped_refptr<MemoryCacheEntry>& rhs);

  friend class base::RefCounted<MemoryCache>;
  ~MemoryCache();

  void EvictEntry(std::string_view key) EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void EvictEntry(MemoryCacheEntry* entry) EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void InsertEntry(scoped_refptr<MemoryCacheEntry> entry)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  bool CanFitMemoryCacheEntry(size_t data_size) const;

  base::Lock mutex_;
  base::flat_set<scoped_refptr<MemoryCacheEntry>> entries_ GUARDED_BY(mutex_);
  base::LinkedList<MemoryCacheEntry> lru_ GUARDED_BY(mutex_);

  const size_t max_size_;
  size_t current_size_ GUARDED_BY(mutex_) = 0;

  const std::string cache_hit_trace_event_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MEMORY_CACHE_H_
