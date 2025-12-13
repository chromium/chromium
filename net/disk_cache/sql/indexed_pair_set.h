// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_INDEXED_PAIR_SET_H_
#define NET_DISK_CACHE_SQL_INDEXED_PAIR_SET_H_

#include <optional>
#include <vector>

#include "base/check.h"
#include "net/base/net_export.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace disk_cache {

// IndexedPairSet is a memory-efficient data structure that stores a set of
// unique (Key, Value) pairs. It is optimized for cases where keys typically
// have only one associated value, but it can accommodate multiple values per
// key.
//
// To conserve memory, this class avoids the overhead of a nested container
// (like absl::flat_hash_map<Key, absl::flat_hash_set<Value>>) for the
// common case of a single value per key. It achieves this by storing the first
// value for a key in a primary map (`primary_map_`). Subsequent, unique values
// for the same key are stored in a secondary map (`secondary_map_`) that maps
// keys to a set of additional values.
//
// This design enables a fast `Contains(key)` lookup, as it only requires
// checking the primary map. However, this optimization makes `Insert` and
// `Remove` operations more complex. For instance, if the representative value
// in the primary map is removed, a new value from the secondary map must be
// promoted to take its place, if one exists.
template <class Key, class Value>
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

  // Inserts a key-value pair if it does not already exist.
  // Returns true if the pair was inserted, false if it already existed.
  bool Insert(Key key, Value value) {
    auto primary_it = primary_map_.find(key);
    if (primary_it == primary_map_.end()) {
      // Key is new, insert into primary_map.
      primary_map_.insert({key, value});
      size_++;
      return true;
    }

    if (primary_it->second == value) {
      // Exact pair already exists in primary_map.
      return false;
    }
    auto insert_result = secondary_map_[key].insert(value);
    if (insert_result.second) {
      size_++;
      return true;
    }
    // Exact pair already exists in secondary_map_.
    return false;
  }

  // Finds all values associated with a given key.
  std::vector<Value> Find(Key key) const {
    std::vector<Value> results;
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

  // Removes a specific key-value pair. Returns true if the pair was found and
  // removed, false otherwise.
  bool Remove(Key key, Value value) {
    auto primary_it = primary_map_.find(key);
    if (primary_it == primary_map_.end()) {
      return false;  // Key does not exist.
    }

    if (primary_it->second == value) {
      // The value to remove is in the primary_map.
      auto secondary_it = secondary_map_.find(key);
      if (secondary_it != secondary_map_.end()) {
        // Promote a value from secondary_map_.
        CHECK(!secondary_it->second.empty());
        auto& secondary_set = secondary_it->second;
        Value new_base_value = *secondary_set.begin();
        secondary_set.erase(secondary_set.begin());
        primary_it->second = new_base_value;
        if (secondary_set.empty()) {
          secondary_map_.erase(secondary_it);
        }
      } else {
        // No additional values, just remove from primary_map.
        primary_map_.erase(primary_it);
      }
      size_--;
      return true;
    }

    // The value to remove is not in primary_map, check secondary_map_.
    auto secondary_it = secondary_map_.find(key);
    if (secondary_it != secondary_map_.end()) {
      if (secondary_it->second.erase(value) > 0) {
        if (secondary_it->second.empty()) {
          secondary_map_.erase(secondary_it);
        }
        size_--;
        return true;
      }
    }
    // Value not found.
    return false;
  }

  // Returns true if the given key exists. This is a fast lookup.
  bool Contains(Key key) const { return primary_map_.contains(key); }

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

  bool HasMultipleValues(const Key& key) const {
    return secondary_map_.contains(key);
  }

  // Attempts to retrieve the unique value associated with the given `key`.
  //
  // This method returns the value if and only if there is exactly one value
  // for the specified key. If the key is associated with multiple values or if
  // the key does not exist in the set, it returns `std::nullopt`.
  std::optional<Value> TryGetSingleValue(const Key& key) const {
    if (HasMultipleValues(key)) {
      return std::nullopt;
    }

    if (const auto primary_it = primary_map_.find(key);
        primary_it != primary_map_.end()) {
      return primary_it->second;
    }
    return std::nullopt;
  }

 private:
  absl::flat_hash_map<Key, Value> primary_map_;
  absl::flat_hash_map<Key, absl::flat_hash_set<Value>> secondary_map_;
  size_t size_ = 0;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_INDEXED_PAIR_SET_H_
