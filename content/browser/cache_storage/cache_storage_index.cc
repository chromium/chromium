// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_index.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/strings/utf_ostream_operators.h"

namespace content {

CacheStorageIndex::CacheStorageIndex()
    : doomed_cache_metadata_(std::u16string(), std::nullopt, std::nullopt) {
  ClearDoomedCache();
}

CacheStorageIndex::~CacheStorageIndex() = default;

CacheStorageIndex& CacheStorageIndex::operator=(CacheStorageIndex&& rhs) {
  CHECK(!has_doomed_cache_, base::NotFatalUntil::M158);
  ordered_cache_metadata_ = std::move(rhs.ordered_cache_metadata_);
  cache_metadata_map_ = std::move(rhs.cache_metadata_map_);
  storage_size_ = rhs.storage_size_;
  storage_padding_ = rhs.storage_padding_;
  rhs.storage_size_ = std::nullopt;
  rhs.storage_padding_ = std::nullopt;
  return *this;
}

void CacheStorageIndex::Insert(const CacheMetadata& cache_metadata) {
  CHECK(!has_doomed_cache_, base::NotFatalUntil::M158);
  // TODO(crbug.com/559142598): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK(!cache_metadata_map_.contains(cache_metadata.name));
  ordered_cache_metadata_.push_back(cache_metadata);
  cache_metadata_map_[cache_metadata.name] = --ordered_cache_metadata_.end();
  storage_size_ = std::nullopt;
  storage_padding_ = std::nullopt;
}

void CacheStorageIndex::Delete(const std::u16string& cache_name) {
  CHECK(!has_doomed_cache_, base::NotFatalUntil::M158);
  auto it = cache_metadata_map_.find(cache_name);
  CHECK(it != cache_metadata_map_.end());
  ordered_cache_metadata_.erase(it->second);
  cache_metadata_map_.erase(it);
  storage_size_ = std::nullopt;
  storage_padding_ = std::nullopt;
}

bool CacheStorageIndex::SetCacheSize(const std::u16string& cache_name,
                                     std::optional<base::ByteSize> size) {
  if (has_doomed_cache_)
    CHECK_NE(cache_name, doomed_cache_metadata_.name,
             base::NotFatalUntil::M158);

  auto it = cache_metadata_map_.find(cache_name);
  if (it == cache_metadata_map_.end()) {
    // This can happen during initialization. The cache should be added to the
    // map soon and the size will be set correctly at that point.
    return false;
  }

  if (it->second->size == size) {
    return false;
  }

  it->second->size = size;
  storage_size_ = std::nullopt;
  return true;
}

const CacheStorageIndex::CacheMetadata* CacheStorageIndex::GetMetadata(
    const std::u16string& cache_name) const {
  const auto& it = cache_metadata_map_.find(cache_name);
  if (it == cache_metadata_map_.end())
    return nullptr;
  return &*it->second;
}

std::optional<base::ByteSize> CacheStorageIndex::GetCacheSizeForTesting(
    const std::u16string& cache_name) const {
  const auto& it = cache_metadata_map_.find(cache_name);
  if (it == cache_metadata_map_.end())
    return std::nullopt;
  return it->second->size;
}

bool CacheStorageIndex::SetCachePadding(const std::u16string& cache_name,
                                        std::optional<base::ByteSize> padding) {
  DCHECK(!has_doomed_cache_ || cache_name != doomed_cache_metadata_.name)
      << "Setting padding of doomed cache: \"" << cache_name << '"';
  auto it = cache_metadata_map_.find(cache_name);
  if (it == cache_metadata_map_.end()) {
    // This can happen during initialization. The cache should be added to the
    // map soon and the padding will be set correctly at that point.
    return false;
  }

  if (it->second->padding == padding) {
    return false;
  }

  it->second->padding = padding;
  storage_padding_ = std::nullopt;
  return true;
}

std::optional<base::ByteSize> CacheStorageIndex::GetCachePaddingForTesting(
    const std::u16string& cache_name) const {
  const auto& it = cache_metadata_map_.find(cache_name);
  if (it == cache_metadata_map_.end())
    return std::nullopt;
  return it->second->padding;
}

std::optional<base::ByteSize> CacheStorageIndex::GetPaddedStorageSize() {
  if (!storage_size_) {
    UpdateStorageSize();
  }
  if (!storage_padding_) {
    CalculateStoragePadding();
  }
  if (!storage_size_ || !storage_padding_) {
    return std::nullopt;
  }
  return *storage_size_ + *storage_padding_;
}

void CacheStorageIndex::UpdateStorageSize() {
  base::ByteSize storage_size;
  storage_size_ = std::nullopt;
  for (const CacheMetadata& info : ordered_cache_metadata_) {
    if (!info.size) {
      return;
    }
    storage_size += *info.size;
  }
  storage_size_ = storage_size;
}

void CacheStorageIndex::CalculateStoragePadding() {
  base::ByteSize storage_padding;
  storage_padding_ = std::nullopt;
  for (const CacheMetadata& info : ordered_cache_metadata_) {
    if (!info.padding) {
      return;
    }
    storage_padding += *info.padding;
  }
  storage_padding_ = storage_padding;
}

void CacheStorageIndex::DoomCache(const std::u16string& cache_name) {
  CHECK(!has_doomed_cache_, base::NotFatalUntil::M158);
  auto map_it = cache_metadata_map_.find(cache_name);
  CHECK(map_it != cache_metadata_map_.end());
  doomed_cache_metadata_ = std::move(*(map_it->second));
  after_doomed_cache_metadata_ = ordered_cache_metadata_.erase(map_it->second);
  cache_metadata_map_.erase(map_it);
  storage_size_ = std::nullopt;
  storage_padding_ = std::nullopt;
  has_doomed_cache_ = true;
}

void CacheStorageIndex::FinalizeDoomedCache() {
  CHECK(has_doomed_cache_, base::NotFatalUntil::M158);
  ClearDoomedCache();
}

void CacheStorageIndex::RestoreDoomedCache() {
  CHECK(has_doomed_cache_, base::NotFatalUntil::M158);
  const auto cache_name = doomed_cache_metadata_.name;
  cache_metadata_map_[cache_name] = ordered_cache_metadata_.insert(
      after_doomed_cache_metadata_, std::move(doomed_cache_metadata_));
  after_doomed_cache_metadata_ = ordered_cache_metadata_.end();
  storage_size_ = std::nullopt;
  storage_padding_ = std::nullopt;
  ClearDoomedCache();
}

void CacheStorageIndex::ClearDoomedCache() {
  doomed_cache_metadata_.name.clear();
  after_doomed_cache_metadata_ = ordered_cache_metadata_.end();
  has_doomed_cache_ = false;
}

}  // namespace content
