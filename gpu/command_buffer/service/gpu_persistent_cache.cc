// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_persistent_cache.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "components/persistent_cache/entry.h"
#include "components/persistent_cache/persistent_cache.h"

namespace gpu {

GpuPersistentCache::GpuPersistentCache() = default;

GpuPersistentCache::~GpuPersistentCache() = default;

void GpuPersistentCache::InitializeCache(
    base::File db_file,
    base::File journal_file,
    base::UnsafeSharedMemoryRegion shared_lock) {
  base::AutoLock auto_lock(lock_);
  persistent_cache::BackendParams params;
  params.type = persistent_cache::BackendType::kSqlite;
  params.db_file = std::move(db_file);
  params.db_file_is_writable = true;
  params.journal_file = std::move(journal_file);
  params.journal_file_is_writable = true;
  params.shared_lock = std::move(shared_lock);
  persistent_cache_ =
      persistent_cache::PersistentCache::Open(std::move(params));
}

size_t GpuPersistentCache::LoadData(const void* key,
                                    size_t key_size,
                                    void* value,
                                    size_t value_size) {
  TRACE_EVENT0("gpu", "GpuPersistentCache::LoadData");
  base::AutoLock auto_lock(lock_);
  if (!persistent_cache_) {
    return 0;
  }

  std::string_view key_str(static_cast<const char*>(key), key_size);
  std::unique_ptr<persistent_cache::Entry> entry =
      persistent_cache_->Find(key_str);

  if (!entry) {
    return 0;
  }

  if (value_size > 0) {
    return entry->CopyContentTo(
        UNSAFE_TODO(base::span(static_cast<uint8_t*>(value), value_size)));
  }

  return entry->GetContentSize();
}

void GpuPersistentCache::StoreData(const void* key,
                                   size_t key_size,
                                   const void* value,
                                   size_t value_size) {
  TRACE_EVENT0("gpu", "GpuPersistentCache::StoreData");
  base::AutoLock auto_lock(lock_);
  if (!persistent_cache_) {
    return;
  }

  std::string_view key_str(static_cast<const char*>(key), key_size);
  persistent_cache_->Insert(
      key_str,
      UNSAFE_TODO(base::span(static_cast<const uint8_t*>(value), value_size)));
}

}  // namespace gpu
