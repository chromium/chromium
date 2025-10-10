// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_IN_MEMORY_INDEX_H_
#define NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_IN_MEMORY_INDEX_H_

#include "net/base/net_export.h"
#include "net/disk_cache/sql/indexed_pair_set.h"
#include "net/disk_cache/sql/sql_persistent_store.h"

namespace disk_cache {

// A class that holds an in-memory index of the cache entries. It provides fast
// lookups of cache entries by their hash and resource ID.
class NET_EXPORT_PRIVATE SqlPersistentStoreInMemoryIndex {
 public:
  SqlPersistentStoreInMemoryIndex();
  ~SqlPersistentStoreInMemoryIndex();

  SqlPersistentStoreInMemoryIndex(const SqlPersistentStoreInMemoryIndex&) =
      delete;
  SqlPersistentStoreInMemoryIndex& operator=(
      const SqlPersistentStoreInMemoryIndex&) = delete;
  SqlPersistentStoreInMemoryIndex(
      SqlPersistentStoreInMemoryIndex&& other) noexcept;
  SqlPersistentStoreInMemoryIndex& operator=(
      SqlPersistentStoreInMemoryIndex&& other) noexcept;

  bool Insert(CacheEntryKey::Hash hash, SqlPersistentStore::ResId res_id);
  bool Contains(CacheEntryKey::Hash hash) const;
  bool Remove(SqlPersistentStore::ResId res_id);
  bool Remove(CacheEntryKey::Hash hash, SqlPersistentStore::ResId res_id);
  void Clear();

  size_t size() const { return res_id_to_hash_map_.size(); }

 private:
  using HashResIdSet =
      IndexedPairSet<CacheEntryKey::Hash, SqlPersistentStore::ResId>;
  using ResIdToHashMap =
      absl::flat_hash_map<SqlPersistentStore::ResId, CacheEntryKey::Hash>;

  bool RemoveInternal(ResIdToHashMap::iterator it);

  HashResIdSet hash_res_id_set_;
  ResIdToHashMap res_id_to_hash_map_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_IN_MEMORY_INDEX_H_
