// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_IN_MEMORY_INDEX_H_
#define NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_IN_MEMORY_INDEX_H_

#include "base/check.h"
#include "base/types/strong_alias.h"
#include "net/base/net_export.h"
#include "net/disk_cache/sql/indexed_pair_set.h"
#include "net/disk_cache/sql/sql_persistent_store.h"

namespace disk_cache {

// A class that holds an in-memory index of the cache entries. It provides fast
// lookups of cache entries by their hash and resource ID.
//
// This class is optimized for memory usage. It maintains two maps: one from
// CacheEntryKey::Hash to ResId, and another from ResId to Hash. While
// SqlPersistentStore::ResId is a 64-bit integer, it is typically a database
// rowid that does not exceed the UINT32_MAX limit.
//
// On a 64-bit system, a std::pair<int64_t, int32_t> consumes 16 bytes due to
// memory alignment. By using a unsigned 32-bit integer for the ResId by default
// (ResId32), the pair becomes <uint32_t, int32_t>, which consumes only 8 bytes.
// This effectively halves the memory footprint of the maps, which is
// significant as the index can contain over 100,000 entries.
//
// To handle the rare case of a ResId exceeding UINT32_MAX, this class uses two
// separate maps.
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

  size_t size() const;

 private:
  using ResId32 = base::StrongAlias<class ResId32, uint32_t>;

  template <class ResIdType>
  class Impl {
   public:
    using ResIdToHashMap = absl::flat_hash_map<ResIdType, CacheEntryKey::Hash>;

    Impl() = default;
    ~Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&& other) noexcept = default;
    Impl& operator=(Impl&& other) noexcept = default;

    bool Insert(CacheEntryKey::Hash hash, ResIdType res_id) {
      if (res_id_to_hash_map_.contains(res_id)) {
        return false;
      }
      if (hash_res_id_set_.Insert(hash, res_id)) {
        res_id_to_hash_map_[res_id] = hash;
        return true;
      }
      return false;
    }

    bool Contains(CacheEntryKey::Hash hash) const {
      return hash_res_id_set_.Contains(hash);
    }

    bool Remove(ResIdType res_id) {
      auto it = res_id_to_hash_map_.find(res_id);
      if (it == res_id_to_hash_map_.end()) {
        return false;
      }
      RemoveInternal(it);
      return true;
    }

    bool Remove(CacheEntryKey::Hash hash, ResIdType res_id) {
      auto it = res_id_to_hash_map_.find(res_id);
      if (it == res_id_to_hash_map_.end()) {
        return false;
      }
      if (it->second != hash) {
        return false;
      }
      RemoveInternal(it);
      return true;
    }

    void Clear() {
      hash_res_id_set_.Clear();
      res_id_to_hash_map_.clear();
    }

    size_t size() const { return hash_res_id_set_.size(); }

    const ResIdToHashMap& res_id_to_hash_map() const {
      return res_id_to_hash_map_;
    }

   private:
    using HashResIdSet = IndexedPairSet<CacheEntryKey::Hash, ResIdType>;

    void RemoveInternal(ResIdToHashMap::iterator it) {
      DCHECK(it != res_id_to_hash_map_.end());
      CHECK(hash_res_id_set_.Remove(it->second, it->first));
      res_id_to_hash_map_.erase(it);
    }

    HashResIdSet hash_res_id_set_;
    ResIdToHashMap res_id_to_hash_map_;
  };

  static std::optional<ResId32> ToResId32(SqlPersistentStore::ResId res_id);

  Impl<ResId32> impl32_;
  std::optional<Impl<SqlPersistentStore::ResId>> impl64_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_IN_MEMORY_INDEX_H_
