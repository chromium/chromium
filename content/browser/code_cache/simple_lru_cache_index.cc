// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/code_cache/simple_lru_cache_index.h"

#include <limits>

#include "net/base/url_util.h"

namespace content {

SimpleLruCacheIndex::SimpleLruCacheIndex(uint64_t capacity)
    : capacity_(capacity) {}
SimpleLruCacheIndex::~SimpleLruCacheIndex() = default;

bool SimpleLruCacheIndex::Get(const std::string& key) {
  const auto it = entries_.find(key);
  if (it == entries_.end()) {
    return false;
  }
  const Age age = GetNextAge();
  access_list_.erase(it->second.age);
  it->second.age = age;
  access_list_.emplace(age, it->first);
  return true;
}

void SimpleLruCacheIndex::Put(const std::string& key, uint32_t payload_size) {
  Delete(key);

  const Age age = GetNextAge();
  const uint32_t size =
      std::min(payload_size,
               std::numeric_limits<uint32_t>::max() - kEmptyEntrySize) +
      kEmptyEntrySize;

  entries_.emplace(key, Value(age, size));
  access_list_.emplace(age, std::move(key));
  size_ += size;
  Evict();
}

void SimpleLruCacheIndex::Delete(const std::string& key) {
  const auto it = entries_.find(key);
  if (it == entries_.end()) {
    return;
  }

  DCHECK_GE(size_, it->second.size);
  size_ -= it->second.size;
  access_list_.erase(it->second.age);
  entries_.erase(it);
}

uint64_t SimpleLruCacheIndex::GetSize() const {
  return size_;
}

void SimpleLruCacheIndex::Evict() {
  while (capacity_ < size_) {
    auto it = access_list_.begin();
    DCHECK(it != access_list_.end());
    DCHECK(entries_.find(it->second) != entries_.end());

    Delete(it->second);
  }
}

}  // namespace content
