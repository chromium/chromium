// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_IN_MEMORY_INDEX_H_
#define NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_IN_MEMORY_INDEX_H_

#include <algorithm>
#include <optional>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "net/base/net_export.h"
#include "net/disk_cache/sql/indexed_pair_set.h"
#include "net/disk_cache/sql/sql_backend_aliases.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

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
  bool Remove(CacheEntryKeyHash hash, SqlPersistentStoreResId res_id);
  void Clear();

  // Tries to retrieve a single resource ID for the given hash.
  // Returns std::nullopt if the entry is not found or if there are collisions.
  std::optional<SqlPersistentStoreResId> TryGetSingleResId(
      CacheEntryKeyHash hash) const;

  // Updates the in-memory hints for the entry identified by `hash` and
  // `res_id`.
  void SetEntryDataHints(CacheEntryKeyHash hash,
                         SqlPersistentStoreResId res_id,
                         MemoryEntryDataHints hints);

  // Retrieves the in-memory hints for the entry identified by `hash`, if
  // available and unique.
  std::optional<MemoryEntryDataHints> GetEntryDataHints(
      CacheEntryKeyHash hash) const;

  // Returns a vector of resource IDs for entries that have all of the hints
  // specified in `hints_mask`. The result is not sorted.
  std::vector<SqlPersistentStoreResId> GetResIdsWithHints(
      MemoryEntryDataHints hints_mask) const;

  size_t size() const;

  // Returns true if the consolidated in-memory index is enabled.
  bool IsConsolidatedInMemoryIndexEnabled() const {
    return std::holds_alternative<ConsolidatedImpl<ResId32>>(impl32_);
  }

  // Returns true if the metadata (last used time and usage) of all entries
  // are ready in the consolidated in-memory index.
  // This must only be called when the consolidated in-memory index is enabled.
  bool is_entry_metadata_ready() const { return is_entry_metadata_ready_; }

  // Sets the metadata ready flag.
  // This must only be called when the consolidated in-memory index is enabled.
  void SetEntryMetadataReady();

  // Iterates over all entries in the index and calls `fun` for each entry.
  // This version includes approximate metadata (last used time and usage).
  // This must only be called when the consolidated in-memory index is enabled.
  void ForEach(base::FunctionRef<void(CacheEntryKeyHash hash,
                                      SqlPersistentStoreResId res_id,
                                      base::Time approximate_last_used,
                                      uint64_t approximate_bytes_usage,
                                      MemoryEntryDataHints hints)> fun) const;
  // Iterates over all entries in the index and calls `fun` for each entry.
  // This version does not include metadata.
  // This must only be called when the consolidated in-memory index is enabled.
  void ForEach(base::FunctionRef<void(CacheEntryKeyHash hash,
                                      SqlPersistentStoreResId res_id,
                                      MemoryEntryDataHints hints)> fun) const;

  // Sets the approximate last used time and usage for the entry.
  // This must only be called when the consolidated in-memory index is enabled.
  void SetEntryLastUsedAndUsage(CacheEntryKeyHash hash,
                                SqlPersistentStoreResId res_id,
                                base::Time last_used,
                                std::optional<uint64_t> bytes_usage);
  // Sets the approximate last used time for the entry.
  // This must only be called when the consolidated in-memory index is enabled.
  void SetEntryLastUsed(CacheEntryKeyHash hash,
                        SqlPersistentStoreResId res_id,
                        base::Time last_used);

  struct Metadata {
    base::Time last_used;
    uint64_t bytes_usage;
    MemoryEntryDataHints hints;
  };

  // Retrieves the metadata (last_used, bytes_usage, and hints) for the entry.
  // This must only be called when the consolidated in-memory index is enabled
  // and is_entry_metadata_ready() is true. Returns std::nullopt if the entry
  // is not found.
  std::optional<Metadata> GetEntryMetadataForTesting(
      CacheEntryKeyHash hash,
      SqlPersistentStoreResId res_id) const;

 private:
  using ResId32 = base::StrongAlias<class ResId32, uint32_t>;

  template <class ResIdType>
  class Impl {
   public:
    using ResIdToEntryDataHintsMap =
        absl::flat_hash_map<ResIdType, MemoryEntryDataHints>;

    Impl() = default;
    ~Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&& other) noexcept = default;
    Impl& operator=(Impl&& other) noexcept = default;

    bool Insert(CacheEntryKeyHash hash, ResIdType res_id) {
      return hash_res_id_set_.Insert(hash, res_id);
    }

    bool Contains(CacheEntryKeyHash hash) const {
      return hash_res_id_set_.Contains(hash);
    }

    bool Remove(CacheEntryKeyHash hash, ResIdType res_id) {
      if (!hash_res_id_set_.Remove(hash, res_id)) {
        return false;
      }
      res_id_to_hints_map_.erase(res_id);
      return true;
    }

    void Clear() {
      hash_res_id_set_.Clear();
      res_id_to_hints_map_.clear();
    }

    // Tries to retrieve a single resource ID for the given hash.
    std::optional<ResIdType> TryGetSingleResId(CacheEntryKeyHash hash) const {
      return hash_res_id_set_.TryGetSingleSubKey(hash);
    }

    // Updates the in-memory hints for the entry identified by `hash` and
    // `res_id`.
    void SetEntryDataHints(CacheEntryKeyHash hash,
                           ResIdType res_id,
                           MemoryEntryDataHints hints) {
      if (hash_res_id_set_.Contains(hash, res_id)) {
        res_id_to_hints_map_[res_id] = hints;
      }
    }

    // Retrieves the in-memory hints for the entry identified by `hash`, if
    // available and unique.
    std::optional<MemoryEntryDataHints> GetEntryDataHints(
        CacheEntryKeyHash hash) const {
      std::optional<ResIdType> res_id =
          hash_res_id_set_.TryGetSingleSubKey(hash);
      if (!res_id) {
        return std::nullopt;
      }
      if (const auto it = res_id_to_hints_map_.find(*res_id);
          it != res_id_to_hints_map_.end()) {
        return it->second;
      }
      return std::nullopt;
    }

    // Appends resource IDs to `out_res_ids` for entries that have all of the
    // hints specified in `hints_mask`.
    void GetResIdsWithHints(
        MemoryEntryDataHints hints_mask,
        std::vector<SqlPersistentStoreResId>& out_res_ids) const {
      uint8_t mask_val = hints_mask.value();
      for (const auto& [res_id, hints] : res_id_to_hints_map_) {
        if ((hints.value() & mask_val) == mask_val) {
          out_res_ids.emplace_back(res_id.value());
        }
      }
    }

    size_t size() const { return hash_res_id_set_.size(); }

   private:
    using HashResIdSet =
        IndexedPairSet<CacheEntryKeyHash, ResIdType, ResIdType>;

    HashResIdSet hash_res_id_set_;
    ResIdToEntryDataHintsMap res_id_to_hints_map_;
  };

  template <class ResIdType>
  class ConsolidatedImpl {
   public:
    struct Entry {
      ResIdType res_id;
      // The approximate time the entry was last used, in seconds since the UNIX
      // epoch. Note that this 32-bit unsigned integer will overflow and
      // potentially cause incorrect behavior after February 2106.
      uint32_t last_used_time_seconds_since_epoch;
      // The total bytes usage of the entry in 256-byte chunks, rounded up.
      // Capped at (1<<30) - 1.
      uint32_t entry_size_256b_chunks : 30;
      // Stores MemoryEntryDataHints (e.g., HINT_HIGH_PRIORITY), which fits in 2
      // bits.
      uint32_t hints : 2;
    };
    struct EntryTraits {
      static const ResIdType& GetSubKey(const Entry& entry) {
        return entry.res_id;
      }
    };

    ConsolidatedImpl() = default;
    ~ConsolidatedImpl() = default;
    ConsolidatedImpl(const ConsolidatedImpl&) = delete;
    ConsolidatedImpl& operator=(const ConsolidatedImpl&) = delete;
    ConsolidatedImpl(ConsolidatedImpl&& other) noexcept = default;
    ConsolidatedImpl& operator=(ConsolidatedImpl&& other) noexcept = default;

    bool Insert(CacheEntryKeyHash hash, ResIdType res_id) {
      return hash_res_id_map_.Insert(
          hash, {res_id, /*last_used_time_seconds_since_epoch=*/0,
                 /*entry_size_256b_chunks=*/0, /*hints=*/0});
    }

    bool Contains(CacheEntryKeyHash hash) const {
      return hash_res_id_map_.Contains(hash);
    }

    bool Remove(CacheEntryKeyHash hash, ResIdType res_id) {
      return hash_res_id_map_.Remove(hash, res_id);
    }

    void Clear() { hash_res_id_map_.Clear(); }

    // Tries to retrieve a single resource ID for the given hash.
    std::optional<ResIdType> TryGetSingleResId(CacheEntryKeyHash hash) const {
      return hash_res_id_map_.TryGetSingleSubKey(hash);
    }

    // Updates the in-memory hints for the entry identified by `hash` and
    // `res_id`.
    void SetEntryDataHints(CacheEntryKeyHash hash,
                           ResIdType res_id,
                           MemoryEntryDataHints hints) {
      Entry* entry = hash_res_id_map_.Get(hash, res_id);
      if (entry) {
        // Only accept hints that fit in 2 bits.
        if ((hints.value() & ~0x3) == 0) {
          entry->hints = hints.value();
        }
      }
    }

    // Retrieves the in-memory hints for the entry identified by `hash`, if
    // available and unique.
    std::optional<MemoryEntryDataHints> GetEntryDataHints(
        CacheEntryKeyHash hash) const {
      std::optional<ResIdType> res_id =
          hash_res_id_map_.TryGetSingleSubKey(hash);
      if (!res_id) {
        return std::nullopt;
      }
      const Entry* entry = hash_res_id_map_.Get(hash, *res_id);
      if (entry && entry->hints != 0) {
        return MemoryEntryDataHints(entry->hints);
      }
      return std::nullopt;
    }

    // Appends resource IDs to `out_res_ids` for entries that have all of the
    // hints specified in `hints_mask`.
    void GetResIdsWithHints(
        MemoryEntryDataHints hints_mask,
        std::vector<SqlPersistentStoreResId>& out_res_ids) const {
      uint8_t mask_val = hints_mask.value() & 0x3;
      hash_res_id_map_.ForEach(
          [&out_res_ids, mask_val](CacheEntryKeyHash hash, const Entry& entry) {
            if ((entry.hints & mask_val) == mask_val) {
              out_res_ids.emplace_back(entry.res_id.value());
            }
          });
    }

    void ForEach(
        base::FunctionRef<void(CacheEntryKeyHash hash,
                               SqlPersistentStoreResId res_id,
                               MemoryEntryDataHints hints)> fun) const {
      hash_res_id_map_.ForEach([&](CacheEntryKeyHash hash, const Entry& entry) {
        fun(hash, SqlPersistentStoreResId(entry.res_id.value()),
            MemoryEntryDataHints(entry.hints));
      });
    }

    void ForEach(
        base::FunctionRef<void(CacheEntryKeyHash hash,
                               SqlPersistentStoreResId res_id,
                               base::Time approximate_last_used,
                               uint64_t approximate_bytes_usage,
                               MemoryEntryDataHints hints)> fun) const {
      hash_res_id_map_.ForEach([&](CacheEntryKeyHash hash, const Entry& entry) {
        fun(hash, SqlPersistentStoreResId(entry.res_id.value()),
            base::Time::FromSecondsSinceUnixEpoch(
                entry.last_used_time_seconds_since_epoch),
            static_cast<uint64_t>(entry.entry_size_256b_chunks) << 8,
            MemoryEntryDataHints(entry.hints));
      });
    }

    void SetEntryLastUsedAndUsage(CacheEntryKeyHash hash,
                                  ResIdType res_id,
                                  base::Time last_used,
                                  std::optional<uint64_t> bytes_usage) {
      Entry* entry = hash_res_id_map_.Get(hash, res_id);
      if (entry) {
        entry->last_used_time_seconds_since_epoch =
            static_cast<uint32_t>(last_used.InSecondsFSinceUnixEpoch());
        if (bytes_usage.has_value()) {
          entry->entry_size_256b_chunks =
              std::min<uint64_t>((*bytes_usage + 255) >> 8, (1ull << 30) - 1);
        }
      }
    }

    std::optional<Metadata> GetEntryMetadataForTesting(CacheEntryKeyHash hash,
                                                       ResIdType res_id) const {
      const Entry* entry = hash_res_id_map_.Get(hash, res_id);
      if (!entry) {
        return std::nullopt;
      }
      return Metadata{
          .last_used = base::Time::FromSecondsSinceUnixEpoch(
              entry->last_used_time_seconds_since_epoch),
          .bytes_usage = static_cast<uint64_t>(entry->entry_size_256b_chunks)
                         << 8,
          .hints = MemoryEntryDataHints(entry->hints)};
    }

    size_t size() const { return hash_res_id_map_.size(); }

   private:
    using HashResIdMap =
        IndexedPairSet<CacheEntryKeyHash, ResIdType, Entry, EntryTraits>;

    HashResIdMap hash_res_id_map_;
  };

  using ImplVariant32 = std::variant<Impl<ResId32>, ConsolidatedImpl<ResId32>>;
  using ImplVariant64 = std::variant<Impl<SqlPersistentStoreResId>,
                                     ConsolidatedImpl<SqlPersistentStoreResId>>;

  static std::optional<ResId32> ToResId32(SqlPersistentStoreResId res_id);

  ImplVariant32 impl32_;
  std::optional<ImplVariant64> impl64_;
  bool is_entry_metadata_ready_ = false;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_PERSISTENT_STORE_IN_MEMORY_INDEX_H_
