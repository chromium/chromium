// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CODE_CACHE_SIMPLE_LRU_CACHE_INDEX_H_
#define CONTENT_BROWSER_CODE_CACHE_SIMPLE_LRU_CACHE_INDEX_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/types/strong_alias.h"
#include "content/common/content_export.h"

namespace content {

// A simple LRU cache index, to measure the potential performance impact of
// memory-backed code cache.
class CONTENT_EXPORT SimpleLruCacheIndex {
 public:
  explicit SimpleLruCacheIndex(uint64_t capacity);
  ~SimpleLruCacheIndex();

  SimpleLruCacheIndex(const SimpleLruCacheIndex&) = delete;
  SimpleLruCacheIndex& operator=(const SimpleLruCacheIndex&) = delete;

  // Returns whether an entry for `key` exists in the cache.
  bool Get(const std::string& key);
  // Puts an entry whose payload size is `size` to the cache.
  void Put(const std::string& key, uint32_t size);
  // Deletes an entry for `key` in the cache. If there is no such an entry, this
  // does nothing.
  void Delete(const std::string& key);
  // Returns the total size of the cache.
  uint64_t GetSize() const;

  static constexpr uint32_t kEmptyEntrySize = 2048;

 private:
  using Age = base::StrongAlias<class AgeTag, uint32_t>;
  using Key = std::string;
  struct Value {
    Value(Age age, uint32_t size) : age(age), size(size) {}

    Age age;
    uint32_t size;
  };

  Age GetNextAge() { return Age(age_source_++); }
  void Evict();

  const uint64_t capacity_;
  std::map<Key, Value> entries_;
  std::map<Age, Key> access_list_;
  uint32_t age_source_ = 0;
  uint64_t size_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CODE_CACHE_SIMPLE_LRU_CACHE_INDEX_H_
