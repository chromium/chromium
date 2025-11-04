// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MEMORY_CACHE_H_
#define GPU_COMMAND_BUFFER_SERVICE_MEMORY_CACHE_H_

#include "base/containers/flat_set.h"
#include "base/containers/linked_list.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
class GPU_GLES2_EXPORT MemoryCache : public base::RefCounted<MemoryCache> {
 public:
  explicit MemoryCache(size_t max_size,
                       std::string_view cache_hit_trace_event = "");

  size_t LoadData(std::string_view key, void* value_out, size_t value_size);
  void StoreData(std::string_view key, const void* value, size_t value_size);

  void PurgeMemory(base::MemoryPressureLevel memory_pressure_level);

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::ProcessMemoryDump* pmd);

 private:
  // Internal entry class for LRU tracking and holding key/value pair.
  class Entry : public base::LinkNode<Entry> {
   public:
    Entry(std::string key, const void* value, size_t value_size);
    ~Entry();

    std::string_view Key() const;

    size_t TotalSize() const;
    size_t DataSize() const;

    base::span<const uint8_t> Data() const;
    size_t ReadData(void* value_out, size_t value_size) const;

   private:
    const std::string key_;
    const std::vector<uint8_t> data_;
  };

  // Overrides for transparent flat_set lookups using a string.
  friend bool operator<(const std::unique_ptr<Entry>& lhs,
                        const std::unique_ptr<Entry>& rhs);
  friend bool operator<(const std::unique_ptr<Entry>& lhs,
                        std::string_view rhs);
  friend bool operator<(std::string_view lhs,
                        const std::unique_ptr<Entry>& rhs);

  friend class base::RefCounted<MemoryCache>;
  ~MemoryCache();

  void EvictEntry(Entry* entry) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  base::Lock mutex_;
  base::flat_set<std::unique_ptr<Entry>> entries_ GUARDED_BY(mutex_);
  base::LinkedList<Entry> lru_ GUARDED_BY(mutex_);

  const size_t max_size_;
  size_t current_size_ GUARDED_BY(mutex_) = 0;

  const std::string cache_hit_trace_event_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MEMORY_CACHE_H_
