// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/code_cache/simple_lru_cache.h"

#include <limits>

#include "base/feature_list.h"
#include "base/not_fatal_until.h"
#include "base/numerics/clamped_math.h"
#include "content/common/features.h"
#include "net/base/url_util.h"

namespace content {

using GetResult = SimpleLruCache::GetResult;

GetResult::GetResult(base::Time response_time, mojo_base::BigBuffer data)
    : response_time(response_time), data(std::move(data)) {}
GetResult::~GetResult() = default;

GetResult::GetResult(GetResult&&) = default;
GetResult& GetResult::operator=(GetResult&&) = default;

SimpleLruCache::Value::Value(Age age, base::Time response_time, uint32_t size)
    : age(age), response_time(response_time), size(size) {
  DCHECK(!base::FeatureList::IsEnabled(features::kInMemoryCodeCache));
}

SimpleLruCache::Value::Value(Age age,
                             base::Time response_time,
                             uint32_t size,
                             base::span<const uint8_t> data)
    : age(age),
      response_time(response_time),
      size(size),
      data(data.begin(), data.end()) {
  DCHECK(base::FeatureList::IsEnabled(features::kInMemoryCodeCache));
}

SimpleLruCache::Value::~Value() = default;

SimpleLruCache::Value::Value(Value&&) = default;
SimpleLruCache::Value& SimpleLruCache::Value::operator=(Value&&) = default;

SimpleLruCache::SimpleLruCache(uint64_t capacity) : capacity_(capacity) {}
SimpleLruCache::~SimpleLruCache() = default;

std::optional<GetResult> SimpleLruCache::Get(const std::string& key) {
  base::Time response_time;
  mojo_base::BigBuffer data;
  if (!GetInternal(key, &response_time, &data)) {
    return std::nullopt;
  }
  return std::make_optional<GetResult>(response_time, std::move(data));
}

bool SimpleLruCache::Has(const std::string& key) {
  return GetInternal(key, /*response_time=*/nullptr, /*data=*/nullptr);
}

void SimpleLruCache::Put(const std::string& key,
                         base::Time response_time,
                         base::span<const uint8_t> payload) {
  Delete(key);

  const uint64_t size = base::ClampedNumeric<uint64_t>(key.size()) +
                        payload.size() + kEmptyEntrySize;

  if (size > capacity_) {
    // Ignore a too big entry.
    return;
  }

  const Age age = GetNextAge();
  if (base::FeatureList::IsEnabled(features::kInMemoryCodeCache)) {
    entries_.emplace(key, Value(age, response_time, size, payload));
  } else {
    entries_.emplace(key, Value(age, response_time, size));
  }
  access_list_.emplace(age, std::move(key));
  size_ += size;
  Evict();
}

void SimpleLruCache::Delete(const std::string& key) {
  const auto it = entries_.find(key);
  if (it == entries_.end()) {
    return;
  }

  DCHECK_GE(size_, it->second.size);
  size_ -= it->second.size;
  access_list_.erase(it->second.age);
  entries_.erase(it);
}

uint64_t SimpleLruCache::GetSize() const {
  return size_;
}

void SimpleLruCache::Clear() {
  entries_.clear();
  access_list_.clear();
  size_ = 0;
}

bool SimpleLruCache::GetInternal(const std::string& key,
                                 base::Time* response_time,
                                 mojo_base::BigBuffer* data) {
  const auto it = entries_.find(key);
  if (it == entries_.end()) {
    return false;
  }
  const Age age = GetNextAge();
  access_list_.erase(it->second.age);
  it->second.age = age;
  access_list_.emplace(age, it->first);

  if (response_time) {
    *response_time = it->second.response_time;
  }
  if (data && base::FeatureList::IsEnabled(features::kInMemoryCodeCache)) {
    *data = mojo_base::BigBuffer(it->second.data);
  }
  return true;
}

void SimpleLruCache::Evict() {
  while (capacity_ < size_) {
    auto it = access_list_.begin();
    CHECK(it != access_list_.end(), base::NotFatalUntil::M130);
    DCHECK(entries_.find(it->second) != entries_.end());

    Delete(it->second);
  }
}

}  // namespace content
