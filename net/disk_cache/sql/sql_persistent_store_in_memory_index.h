// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_IN_MEMORY_INDEX_H_
#define NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_IN_MEMORY_INDEX_H_

#include <optional>

#include "base/check.h"
#include "base/types/strong_alias.h"
#include "net/base/net_export.h"
#include "net/disk_cache/sql/indexed_pair_set.h"
#include "net/disk_cache/sql/sql_backend_aliases.h"

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

  bool Insert(CacheEntryKeyHash hash, SqlPersistentStoreResId res_id);
  bool Contains(CacheEntryKeyHash hash) const;
  bool Remove(SqlPersistentStoreResId res_id);
  bool Remove(CacheEntryKeyHash hash, SqlPersistentStoreResId res_id);
  void Clear();

  // Tries to retrieve a single resource ID for the given hash.
  // Returns std::nullopt if the entry is not found or if there are collisions.
  std::optional<SqlPersistentStoreResId> TryGetSingleResId(
      CacheEntryKeyHash hash) const;

  // Updates the in-memory hints for the entry identified by `res_id`.
  void SetEntryDataHints(SqlPersistentStoreResId res_id,
                         MemoryEntryDataHints hints);

  // Retrieves the in-memory hints for the entry identified by `hash`, if
  // available and unique.
  std::optional<MemoryEntryDataHints> GetEntryDataHints(
      CacheEntryKeyHash hash) const;

  size_t size() const;

 private:
  using ResId32 = base::StrongAlias<class ResId32, uint32_t>;

  template <class ResIdType>
  class Impl {
   public:
    using ResIdToHashMap = absl::flat_hash_map<ResIdType, CacheEntryKeyHash>;
    using ResIdToEntryDataHintsMap =
        absl::flat_hash_map<ResIdType, MemoryEntryDataHints>;

    Impl() = default;
    ~Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&& other) noexcept = default;
    Impl& operator=(Impl&& other) noexcept = default;

    bool Insert(CacheEntryKeyHash hash, ResIdType res_id) {
      if (res_id_to_hash_map_.contains(res_id)) {
        return false;
      }
      if (hash_res_id_set_.Insert(hash, res_id)) {
        res_id_to_hash_map_[res_id] = hash;
        return true;
      }
      return false;
    }

    bool Contains(CacheEntryKeyHash hash) const {
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

    bool Remove(CacheEntryKeyHash hash, ResIdType res_id) {
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
      res_id_to_hash_map_.clear();
    }

    // Tries to retrieve a single resource ID for the given hash.
    std::optional<ResIdType> TryGetSingleResId(CacheEntryKeyHash hash) const {
      return hash_res_id_set_.TryGetSingleValue(hash);
    }

    // Updates the in-memory hints for the entry identified by `res_id`.
    void SetEntryDataHints(ResIdType res_id, MemoryEntryDataHints hints) {
      if (res_id_to_hash_map_.contains(res_id)) {
        res_id_to_hints_map_[res_id] = hints;
      }
    }

    // Retrieves the in-memory hints for the entry identified by `res_id`, if
    // available.
    std::optional<MemoryEntryDataHints> GetEntryDataHints(
        ResIdType res_id) const {
      if (const auto it = res_id_to_hints_map_.find(res_id);
          it != res_id_to_hints_map_.end()) {
        return it->second;
      }
      return std::nullopt;
    }

    size_t size() const { return hash_res_id_set_.size(); }

    const ResIdToHashMap& res_id_to_hash_map() const {
      return res_id_to_hash_map_;
    }

   private:
    using HashResIdSet = IndexedPairSet<CacheEntryKeyHash, ResIdType>;

    void RemoveInternal(ResIdToHashMap::iterator it) {
      DCHECK(it != res_id_to_hash_map_.end());
      CHECK(hash_res_id_set_.Remove(it->second, it->first));
      res_id_to_hints_map_.erase(it->first);
      res_id_to_hash_map_.erase(it);
    }

    HashResIdSet hash_res_id_set_;
    ResIdToHashMap res_id_to_hash_map_;
    ResIdToEntryDataHintsMap res_id_to_hints_map_;
  };

  static std::optional<ResId32> ToResId32(SqlPersistentStoreResId res_id);

  Impl<ResId32> impl32_;
  std::optional<Impl<SqlPersistentStoreResId>> impl64_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_IN_MEMORY_INDEX_H_
