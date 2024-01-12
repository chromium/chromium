// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CODE_CACHE_SIMPLE_LRU_CACHE_H_
#define CONTENT_BROWSER_CODE_CACHE_SIMPLE_LRU_CACHE_H_

#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "content/common/content_export.h"
#include "content/common/features.h"
#include "mojo/public/cpp/base/big_buffer.h"

namespace content {

// A simple LRU cache, to measure the potential performance impact of
// memory-backed code cache.
class CONTENT_EXPORT SimpleLruCache {
 public:
  explicit SimpleLruCache(uint64_t capacity);
  ~SimpleLruCache();

  SimpleLruCache(const SimpleLruCache&) = delete;
  SimpleLruCache& operator=(const SimpleLruCache&) = delete;

  struct CONTENT_EXPORT GetResult {
    GetResult(base::Time response_time, mojo_base::BigBuffer data);
    ~GetResult();

    GetResult(const GetResult&) = delete;
    GetResult& operator=(const GetResult&) = delete;

    GetResult(GetResult&&);
    GetResult& operator=(GetResult&&);

    base::Time response_time;
    mojo_base::BigBuffer data;
  };

  // Returns the contents of the entry for `key`, if any. The `data` member of
  // GetResult is filled only when features::kInMemoryCodeCache is enabled.
  // This updates the entry access time.
  std::optional<GetResult> Get(const std::string& key);
  // Returns whether there is an entry for `key`. This updates the entry access
  // time.
  bool Has(const std::string& key);
  // Puts an entry.
  void Put(const std::string& key,
           base::Time response_time,
           base::span<const uint8_t> data);
  // Deletes an entry for `key` in the cache. If there is no such an entry, this
  // does nothing.
  void Delete(const std::string& key);
  // Returns the total size of the cache.
  uint64_t GetSize() const;

  // Clears all the entries.
  void Clear();

  static constexpr uint32_t kEmptyEntrySize = 1024;

 private:
  using Age = base::StrongAlias<class AgeTag, uint32_t>;
  using Key = std::string;
  struct Value final {
    Value(Age age, base::Time response_time, uint32_t size);
    Value(Age age,
          base::Time response_time,
          uint32_t size,
          base::span<const uint8_t> data);
    ~Value();

    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;
    Value(Value&&);
    Value& operator=(Value&&);

    Age age;
    base::Time response_time;
    uint32_t size;
    // This is used when features::kInMemoryCodeCache is enabled.
    std::vector<uint8_t> data;
  };

  bool GetInternal(const std::string& key,
                   base::Time* response_time,
                   mojo_base::BigBuffer* data);

  Age GetNextAge() { return Age(age_source_++); }
  void Evict();

  const uint64_t capacity_;
  std::map<Key, Value> entries_;
  std::map<Age, Key> access_list_;
  uint32_t age_source_ = 0;
  uint64_t size_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CODE_CACHE_SIMPLE_LRU_CACHE_H_
