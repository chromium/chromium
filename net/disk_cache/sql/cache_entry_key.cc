// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/cache_entry_key.h"

#include "base/check.h"
#include "base/hash/hash.h"

namespace disk_cache {

// static
CacheEntryKey::Hash CacheEntryKey::HashFromString(const std::string_view str) {
  return Hash(static_cast<int32_t>(base::PersistentHash(str)));
}

CacheEntryKey::CacheEntryKey(std::string str)
    : data_(base::MakeRefCounted<base::RefCountedString>(std::move(str))),
      hash_(HashFromString(string())) {}

CacheEntryKey::~CacheEntryKey() = default;

CacheEntryKey::CacheEntryKey(const CacheEntryKey& other) = default;
CacheEntryKey::CacheEntryKey(CacheEntryKey&& other) = default;
CacheEntryKey& CacheEntryKey::operator=(const CacheEntryKey& other) = default;
CacheEntryKey& CacheEntryKey::operator=(CacheEntryKey&& other) = default;

bool CacheEntryKey::operator<(const CacheEntryKey& other) const {
  return data_ != other.data_ && string() < other.string();
}

bool CacheEntryKey::operator==(const CacheEntryKey& other) const {
  return data_ == other.data_ ||
         (hash() == other.hash() && string() == other.string());
}

const std::string& CacheEntryKey::string() const {
  DCHECK(data_);
  return data_->as_string();
}

}  // namespace disk_cache
