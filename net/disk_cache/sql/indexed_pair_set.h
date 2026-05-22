// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_INDEXED_PAIR_SET_H_
#define NET_DISK_CACHE_SQL_INDEXED_PAIR_SET_H_

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace disk_cache {

// Default traits that expects Entry to have a 'sub_key' member.
template <typename SubKey, typename Entry>
struct IndexedPairTraits {
  static const SubKey& GetSubKey(const Entry& entry) { return entry.sub_key; }
};

// Specialization for when Entry is SubKey itself.
template <typename SubKey>
struct IndexedPairTraits<SubKey, SubKey> {
  static const SubKey& GetSubKey(const SubKey& entry) { return entry; }
};

// IndexedPairSet is a memory-efficient data structure that stores a set of
// (Key, Entry) mapping where the Entry always contains a SubKey. It is
// optimized for cases where keys typically have only one associated sub-key,
// but it can accommodate multiple sub-keys per key.
//
// To conserve memory, this class avoids the overhead of a nested container
// (like absl::flat_hash_map<Key, std::vector<Entry>>) for the common case of a
// single sub-key per key. It achieves this by storing the first sub-key for a
// key in a primary map (`primary_map_`). Subsequent, unique sub-keys for the
// same key are stored in a secondary map (`secondary_map_`) that maps keys to a
// vector of additional sub-keys.
//
// This design enables a fast `ContainsKey(key)` lookup, as it only requires
// checking the primary map. However, this optimization makes `Insert` and
// `Remove` operations more complex. For instance, if the representative value
// in the primary map is removed, a new value from the secondary map must be
// promoted to take its place, if one exists.
//
// template parameters:
// - Key: The primary index key (e.g., CacheEntryKeyHash).
// - SubKey: The secondary index key (e.g., ResId).
// - Entry: The actual data structure stored. It must contain the SubKey.
// - Traits: Used to extract the SubKey from the Entry.
template <class Key,
          class SubKey,
          class Entry,
          class Traits = IndexedPairTraits<SubKey, Entry>>
class NET_EXPORT_PRIVATE IndexedPairSet {
 public:
  IndexedPairSet() = default;
  ~IndexedPairSet() = default;

  IndexedPairSet(const IndexedPairSet&) = delete;
  IndexedPairSet& operator=(const IndexedPairSet&) = delete;

  IndexedPairSet(IndexedPairSet&& other) noexcept
      : primary_map_(std::move(other.primary_map_)),
        secondary_map_(std::move(other.secondary_map_)),
        size_(other.size_) {
    other.size_ = 0;
  }

  IndexedPairSet& operator=(IndexedPairSet&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    primary_map_ = std::move(other.primary_map_);
    secondary_map_ = std::move(other.secondary_map_);
    size_ = other.size_;
    other.size_ = 0;
    return *this;
  }

  // Inserts an entry if the (Key, SubKey) pair does not already exist.
  // Returns true if the entry was inserted, false if it already existed.
  bool Insert(Key key, Entry entry) {
    const SubKey& sub_key = Traits::GetSubKey(entry);
    auto primary_it = primary_map_.find(key);
    if (primary_it == primary_map_.end()) {
      // Key is new, insert into `primary_map_`.
      primary_map_.insert({key, std::move(entry)});
      size_++;
      return true;
    }

    if (Traits::GetSubKey(primary_it->second) == sub_key) {
      // Exact pair already exists in `primary_map_`.
      return false;
    }

    auto& secondary_entries = secondary_map_[key];
    for (const auto& e : secondary_entries) {
      if (Traits::GetSubKey(e) == sub_key) {
        // Exact pair already exists in `secondary_map_`.
        return false;
      }
    }
    secondary_entries.push_back(std::move(entry));
    size_++;
    return true;
  }

  // Finds all entries associated with a given key.
  std::vector<Entry> Find(Key key) const {
    std::vector<Entry> results;
    auto primary_it = primary_map_.find(key);
    if (primary_it == primary_map_.end()) {
      return results;
    }

    results.push_back(primary_it->second);

    auto secondary_it = secondary_map_.find(key);
    if (secondary_it != secondary_map_.end()) {
      results.insert(results.end(), secondary_it->second.begin(),
                     secondary_it->second.end());
    }
    return results;
  }

  // Removes a specific entry identified by Key and SubKey.
  // Returns true if the entry was found and removed, false otherwise.
  bool Remove(Key key, const SubKey& sub_key) {
    auto primary_it = primary_map_.find(key);
    if (primary_it == primary_map_.end()) {
      return false;  // Key does not exist.
    }

    if (Traits::GetSubKey(primary_it->second) == sub_key) {
      // The entry to remove is in the primary_map.
      auto secondary_it = secondary_map_.find(key);
      if (secondary_it != secondary_map_.end()) {
        // Promote an entry from secondary_map_.
        auto& secondary_entries = secondary_it->second;
        CHECK(!secondary_entries.empty());
        primary_it->second = std::move(secondary_entries.back());
        secondary_entries.pop_back();
        if (secondary_entries.empty()) {
          secondary_map_.erase(secondary_it);
        }
      } else {
        // No additional entries, just remove from `primary_map_`.
        primary_map_.erase(primary_it);
      }
      size_--;
      return true;
    }

    // The entry to remove is not in `primary_map_`, check `secondary_map_`.
    auto secondary_it = secondary_map_.find(key);
    if (secondary_it != secondary_map_.end()) {
      auto& secondary_entries = secondary_it->second;
      for (auto it = secondary_entries.begin(); it != secondary_entries.end();
           ++it) {
        if (Traits::GetSubKey(*it) == sub_key) {
          secondary_entries.erase(it);
          if (secondary_entries.empty()) {
            secondary_map_.erase(secondary_it);
          }
          size_--;
          return true;
        }
      }
    }
    // Entry not found.
    return false;
  }

  // Returns a pointer to the entry identified by Key and SubKey, if it exists.
  Entry* Get(Key key, const SubKey& sub_key) {
    return const_cast<Entry*>(std::as_const(*this).Get(key, sub_key));
  }

  const Entry* Get(Key key, const SubKey& sub_key) const {
    auto primary_it = primary_map_.find(key);
    if (primary_it == primary_map_.end()) {
      return nullptr;
    }
    if (Traits::GetSubKey(primary_it->second) == sub_key) {
      return &primary_it->second;
    }
    auto secondary_it = secondary_map_.find(key);
    if (secondary_it != secondary_map_.end()) {
      for (const auto& entry : secondary_it->second) {
        if (Traits::GetSubKey(entry) == sub_key) {
          return &entry;
        }
      }
    }
    return nullptr;
  }

  // Returns true if the given key exists. This is a fast lookup.
  bool Contains(Key key) const { return primary_map_.contains(key); }

  // Returns true if the given key-subkey pair exists.
  bool Contains(Key key, const SubKey& sub_key) const {
    return Get(key, sub_key) != nullptr;
  }

  // Returns the total number of elements in the set.
  size_t size() const { return size_; }

  // Returns true if the set is empty.
  bool empty() const { return size_ == 0; }

  // Removes all elements from the set.
  void Clear() {
    primary_map_.clear();
    secondary_map_.clear();
    size_ = 0;
  }

  bool HasMultipleEntries(const Key& key) const {
    return secondary_map_.contains(key);
  }

  // Returns the unique sub-key associated with the given `key`.
  //
  // This method returns the sub-key if and only if there is exactly one entry
  // for the specified key. If the key is associated with multiple entries or if
  // the key does not exist in the set, it returns `std::nullopt`.
  std::optional<SubKey> TryGetSingleSubKey(const Key& key) const {
    if (HasMultipleEntries(key)) {
      return std::nullopt;
    }

    auto it = primary_map_.find(key);
    if (it != primary_map_.end()) {
      return Traits::GetSubKey(it->second);
    }
    return std::nullopt;
  }

  // Iterates over all entries.
  template <typename Callback>
  void ForEach(Callback callback) const {
    for (const auto& [key, entry] : primary_map_) {
      callback(key, entry);
    }
    for (const auto& [key, secondary_entries] : secondary_map_) {
      for (const auto& entry : secondary_entries) {
        callback(key, entry);
      }
    }
  }

 private:
  absl::flat_hash_map<Key, Entry> primary_map_;
  absl::flat_hash_map<Key, std::vector<Entry>> secondary_map_;
  size_t size_ = 0;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_INDEXED_PAIR_SET_H_
