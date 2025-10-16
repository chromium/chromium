// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store_in_memory_index.h"

#include <limits>

namespace disk_cache {

using Hash = CacheEntryKey::Hash;
using ResId = SqlPersistentStore::ResId;

SqlPersistentStoreInMemoryIndex::SqlPersistentStoreInMemoryIndex() = default;
SqlPersistentStoreInMemoryIndex::~SqlPersistentStoreInMemoryIndex() = default;

SqlPersistentStoreInMemoryIndex::SqlPersistentStoreInMemoryIndex(
    SqlPersistentStoreInMemoryIndex&& other) noexcept = default;
SqlPersistentStoreInMemoryIndex& SqlPersistentStoreInMemoryIndex::operator=(
    SqlPersistentStoreInMemoryIndex&& other) noexcept = default;

bool SqlPersistentStoreInMemoryIndex::Insert(Hash hash, ResId res_id) {
  std::optional<ResId32> res_id_32 = ToResId32(res_id);
  if (res_id_32.has_value()) {
    return impl32_.Insert(hash, *res_id_32);
  }

  if (!impl64_) {
    impl64_.emplace();
  }
  return impl64_->Insert(hash, res_id);
}

bool SqlPersistentStoreInMemoryIndex::Contains(Hash hash) const {
  if (impl32_.Contains(hash)) {
    return true;
  }
  return impl64_ && impl64_->Contains(hash);
}

bool SqlPersistentStoreInMemoryIndex::Remove(ResId res_id) {
  std::optional<ResId32> res_id_32 = ToResId32(res_id);
  if (res_id_32.has_value()) {
    return impl32_.Remove(*res_id_32);
  }
  return impl64_ && impl64_->Remove(res_id);
}

bool SqlPersistentStoreInMemoryIndex::Remove(Hash hash, ResId res_id) {
  std::optional<ResId32> res_id_32 = ToResId32(res_id);
  if (res_id_32.has_value()) {
    return impl32_.Remove(hash, *res_id_32);
  }
  return impl64_ && impl64_->Remove(hash, res_id);
}

void SqlPersistentStoreInMemoryIndex::Clear() {
  impl32_.Clear();
  impl64_.reset();
}

// static
std::optional<SqlPersistentStoreInMemoryIndex::ResId32>
SqlPersistentStoreInMemoryIndex::ToResId32(ResId res_id) {
  if (res_id.value() < 0 ||
      res_id.value() > std::numeric_limits<uint32_t>::max()) {
    return std::nullopt;
  }
  return ResId32(static_cast<uint32_t>(res_id.value()));
}

size_t SqlPersistentStoreInMemoryIndex::size() const {
  size_t size = impl32_.size();
  if (impl64_) {
    size += impl64_->size();
  }
  return size;
}

}  // namespace disk_cache
