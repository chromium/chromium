// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store_in_memory_index.h"

namespace disk_cache {

SqlPersistentStoreInMemoryIndex::SqlPersistentStoreInMemoryIndex() = default;
SqlPersistentStoreInMemoryIndex::~SqlPersistentStoreInMemoryIndex() = default;

SqlPersistentStoreInMemoryIndex::SqlPersistentStoreInMemoryIndex(
    SqlPersistentStoreInMemoryIndex&& other) noexcept = default;
SqlPersistentStoreInMemoryIndex& SqlPersistentStoreInMemoryIndex::operator=(
    SqlPersistentStoreInMemoryIndex&& other) noexcept = default;

bool SqlPersistentStoreInMemoryIndex::Insert(CacheEntryKey::Hash hash,
                                             SqlPersistentStore::ResId res_id) {
  if (res_id_to_hash_map_.contains(res_id)) {
    return false;
  }
  if (hash_res_id_set_.Insert(hash, res_id)) {
    res_id_to_hash_map_[res_id] = hash;
    return true;
  }
  return false;
}

bool SqlPersistentStoreInMemoryIndex::Contains(CacheEntryKey::Hash hash) const {
  return hash_res_id_set_.Contains(hash);
}

bool SqlPersistentStoreInMemoryIndex::Remove(SqlPersistentStore::ResId res_id) {
  auto it = res_id_to_hash_map_.find(res_id);
  if (it == res_id_to_hash_map_.end()) {
    return false;
  }
  return RemoveInternal(it);
}

bool SqlPersistentStoreInMemoryIndex::Remove(CacheEntryKey::Hash hash,
                                             SqlPersistentStore::ResId res_id) {
  auto it = res_id_to_hash_map_.find(res_id);
  if (it == res_id_to_hash_map_.end()) {
    return false;
  }
  if (it->second != hash) {
    return false;
  }
  return RemoveInternal(it);
}

void SqlPersistentStoreInMemoryIndex::Clear() {
  hash_res_id_set_.Clear();
  res_id_to_hash_map_.clear();
}

bool SqlPersistentStoreInMemoryIndex::RemoveInternal(
    ResIdToHashMap::iterator it) {
  DCHECK(it != res_id_to_hash_map_.end());
  if (!hash_res_id_set_.Remove(it->second, it->first)) {
    return false;
  }
  res_id_to_hash_map_.erase(it);
  return true;
}

}  // namespace disk_cache
