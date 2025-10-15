// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_persistent_cache.h"

#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/logging.h"
#include "base/synchronization/lock_subtle.h"
#include "base/trace_event/trace_event.h"
#include "components/persistent_cache/entry.h"
#include "components/persistent_cache/persistent_cache.h"

namespace gpu {

// We have to enable lock tracking to allow PersistentCache to be used on
// multiple threads/different sequences.
#if DCHECK_IS_ON()
#define SCOPED_LOCK(lock) \
  base::AutoLock auto_lock(lock, base::subtle::LockTracking::kEnabled)
#else
#define SCOPED_LOCK(lock) base::AutoLock auto_lock(lock)
#endif  // DCHECK_IS_ON()


GpuPersistentCache::GpuPersistentCache() = default;

GpuPersistentCache::~GpuPersistentCache() {
  SCOPED_LOCK(lock_);
  persistent_cache_.reset();
}

void GpuPersistentCache::InitializeCache(
    base::File db_file,
    base::File journal_file,
    base::UnsafeSharedMemoryRegion shared_lock) {
  SCOPED_LOCK(lock_);
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
  std::string_view key_str(static_cast<const char*>(key), key_size);
  std::unique_ptr<persistent_cache::Entry> entry = LoadEntry(key_str);
  if (!entry) {
    return 0;
  }

  if (value_size > 0) {
    return entry->CopyContentTo(
        UNSAFE_TODO(base::span(static_cast<uint8_t*>(value), value_size)));
  }

  return entry->GetContentSize();
}

std::unique_ptr<persistent_cache::Entry> GpuPersistentCache::LoadEntry(
    std::string_view key) {
  SCOPED_LOCK(lock_);
  TRACE_EVENT1("gpu", "GpuPersistentCache::LoadEntry", "persistent_cache_",
               !!persistent_cache_);

  if (!persistent_cache_) {
    return nullptr;
  }

  return persistent_cache_->Find(key);

}

void GpuPersistentCache::StoreData(const void* key,
                                   size_t key_size,
                                   const void* value,
                                   size_t value_size) {
  TRACE_EVENT0("gpu", "GpuPersistentCache::StoreData");
  SCOPED_LOCK(lock_);
  if (!persistent_cache_) {
    return;
  }

  std::string_view key_str(static_cast<const char*>(key), key_size);
  persistent_cache_->Insert(
      key_str,
      UNSAFE_TODO(base::span(static_cast<const uint8_t*>(value), value_size)));
}

}  // namespace gpu
