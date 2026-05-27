// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_persistent_store_in_memory_index.h"

#include <limits>
#include <optional>

#include "net/base/features.h"

namespace disk_cache {

using Hash = CacheEntryKeyHash;
using ResId = SqlPersistentStoreResId;

SqlPersistentStoreInMemoryIndex::SqlPersistentStoreInMemoryIndex()
    : impl32_(net::features::kSqlDiskCacheConsolidatedInMemoryIndex.Get()
                  ? ImplVariant32(std::in_place_type<ConsolidatedImpl<ResId32>>)
                  : ImplVariant32(std::in_place_type<Impl<ResId32>>)) {}

SqlPersistentStoreInMemoryIndex::~SqlPersistentStoreInMemoryIndex() = default;

SqlPersistentStoreInMemoryIndex::SqlPersistentStoreInMemoryIndex(
    SqlPersistentStoreInMemoryIndex&& other) noexcept = default;
SqlPersistentStoreInMemoryIndex& SqlPersistentStoreInMemoryIndex::operator=(
    SqlPersistentStoreInMemoryIndex&& other) noexcept = default;

bool SqlPersistentStoreInMemoryIndex::Insert(Hash hash, ResId res_id) {
  std::optional<ResId32> res_id_32 = ToResId32(res_id);
  if (res_id_32.has_value()) {
    return std::visit([&](auto& impl) { return impl.Insert(hash, *res_id_32); },
                      impl32_);
  }

  if (!impl64_) {
    if (IsConsolidatedInMemoryIndexEnabled()) {
      impl64_.emplace(std::in_place_type<ConsolidatedImpl<ResId>>);
    } else {
      impl64_.emplace(std::in_place_type<Impl<ResId>>);
    }
  }
  return std::visit([&](auto& impl) { return impl.Insert(hash, res_id); },
                    *impl64_);
}

bool SqlPersistentStoreInMemoryIndex::Contains(Hash hash) const {
  if (std::visit([&](const auto& impl) { return impl.Contains(hash); },
                 impl32_)) {
    return true;
  }
  return impl64_ &&
         std::visit([&](const auto& impl) { return impl.Contains(hash); },
                    *impl64_);
}

bool SqlPersistentStoreInMemoryIndex::Remove(Hash hash, ResId res_id) {
  std::optional<ResId32> res_id_32 = ToResId32(res_id);
  if (res_id_32.has_value()) {
    return std::visit([&](auto& impl) { return impl.Remove(hash, *res_id_32); },
                      impl32_);
  }
  return impl64_ &&
         std::visit([&](auto& impl) { return impl.Remove(hash, res_id); },
                    *impl64_);
}

void SqlPersistentStoreInMemoryIndex::Clear() {
  std::visit([](auto& impl) { impl.Clear(); }, impl32_);
  if (impl64_) {
    impl64_.reset();
  }
  is_entry_metadata_ready_ = false;
}

std::optional<SqlPersistentStoreResId>
SqlPersistentStoreInMemoryIndex::TryGetSingleResId(
    CacheEntryKeyHash hash) const {
  const auto res_id_32 = std::visit(
      [&](const auto& impl) { return impl.TryGetSingleResId(hash); }, impl32_);
  const auto res_id_64 =
      impl64_
          ? std::visit(
                [&](const auto& impl) { return impl.TryGetSingleResId(hash); },
                *impl64_)
          : std::nullopt;
  if (res_id_32.has_value() && !res_id_64.has_value()) {
    return ResId(res_id_32->value());
  } else if (!res_id_32.has_value() && res_id_64.has_value()) {
    return *res_id_64;
  }
  return std::nullopt;
}

void SqlPersistentStoreInMemoryIndex::SetEntryDataHints(
    Hash hash,
    ResId res_id,
    MemoryEntryDataHints hints) {
  if (auto res_id_32 = ToResId32(res_id); res_id_32.has_value()) {
    std::visit(
        [&](auto& impl) { impl.SetEntryDataHints(hash, *res_id_32, hints); },
        impl32_);
  } else if (impl64_) {
    std::visit([&](auto& impl) { impl.SetEntryDataHints(hash, res_id, hints); },
               *impl64_);
  }
}

std::optional<MemoryEntryDataHints>
SqlPersistentStoreInMemoryIndex::GetEntryDataHints(
    CacheEntryKeyHash hash) const {
  const bool in_32 = std::visit(
      [&](const auto& impl) { return impl.Contains(hash); }, impl32_);
  const bool in_64 =
      impl64_ &&
      std::visit([&](const auto& impl) { return impl.Contains(hash); },
                 *impl64_);
  if (in_32 && in_64) {
    return std::nullopt;
  }
  if (in_32) {
    return std::visit(
        [&](const auto& impl) { return impl.GetEntryDataHints(hash); },
        impl32_);
  }
  if (in_64) {
    return std::visit(
        [&](const auto& impl) { return impl.GetEntryDataHints(hash); },
        *impl64_);
  }
  return std::nullopt;
}

std::vector<SqlPersistentStoreResId>
SqlPersistentStoreInMemoryIndex::GetResIdsWithHints(
    MemoryEntryDataHints hints_mask) const {
  std::vector<ResId> res_ids;
  auto visitor = [&](const auto& impl) {
    impl.GetResIdsWithHints(hints_mask, res_ids);
  };
  std::visit(visitor, impl32_);
  if (impl64_) {
    std::visit(visitor, *impl64_);
  }
  return res_ids;
}

// static
std::optional<SqlPersistentStoreInMemoryIndex::ResId32>
SqlPersistentStoreInMemoryIndex::ToResId32(ResId res_id) {
  if (res_id.value() < 0 ||
      res_id.value() > std::numeric_limits<uint32_t>::max()) {
    return std::nullopt;
  }
  return ResId32(static_cast<uint32_t>(res_id.value()));
}

size_t SqlPersistentStoreInMemoryIndex::size() const {
  size_t size =
      std::visit([](const auto& impl) { return impl.size(); }, impl32_);
  if (impl64_) {
    size += std::visit([](const auto& impl) { return impl.size(); }, *impl64_);
  }
  return size;
}

void SqlPersistentStoreInMemoryIndex::SetEntryMetadataReady() {
  CHECK(IsConsolidatedInMemoryIndexEnabled());
  is_entry_metadata_ready_ = true;
}

void SqlPersistentStoreInMemoryIndex::ForEach(
    base::FunctionRef<void(CacheEntryKeyHash hash,
                           SqlPersistentStoreResId res_id,
                           base::Time approximate_last_used,
                           uint64_t approximate_bytes_usage,
                           MemoryEntryDataHints hints)> fun) const {
  CHECK(IsConsolidatedInMemoryIndexEnabled());
  std::get<ConsolidatedImpl<ResId32>>(impl32_).ForEach(fun);
  if (impl64_) {
    std::get<ConsolidatedImpl<SqlPersistentStoreResId>>(*impl64_).ForEach(fun);
  }
}

void SqlPersistentStoreInMemoryIndex::ForEach(
    base::FunctionRef<void(CacheEntryKeyHash hash,
                           SqlPersistentStoreResId res_id,
                           MemoryEntryDataHints hints)> fun) const {
  CHECK(IsConsolidatedInMemoryIndexEnabled());
  std::get<ConsolidatedImpl<ResId32>>(impl32_).ForEach(fun);
  if (impl64_) {
    std::get<ConsolidatedImpl<SqlPersistentStoreResId>>(*impl64_).ForEach(fun);
  }
}

void SqlPersistentStoreInMemoryIndex::SetEntryLastUsedAndUsage(
    CacheEntryKeyHash hash,
    SqlPersistentStoreResId res_id,
    base::Time last_used,
    std::optional<uint64_t> bytes_usage) {
  CHECK(IsConsolidatedInMemoryIndexEnabled());
  if (auto res_id_32 = ToResId32(res_id); res_id_32.has_value()) {
    std::get<ConsolidatedImpl<ResId32>>(impl32_).SetEntryLastUsedAndUsage(
        hash, *res_id_32, last_used, bytes_usage);
  } else if (impl64_) {
    std::get<ConsolidatedImpl<SqlPersistentStoreResId>>(*impl64_)
        .SetEntryLastUsedAndUsage(hash, res_id, last_used, bytes_usage);
  }
}

void SqlPersistentStoreInMemoryIndex::SetEntryLastUsed(
    CacheEntryKeyHash hash,
    SqlPersistentStoreResId res_id,
    base::Time last_used) {
  SetEntryLastUsedAndUsage(hash, res_id, last_used, std::nullopt);
}

std::optional<SqlPersistentStoreInMemoryIndex::Metadata>
SqlPersistentStoreInMemoryIndex::GetEntryMetadataForTesting(  // IN-TEST
    CacheEntryKeyHash hash,
    SqlPersistentStoreResId res_id) const {
  CHECK(IsConsolidatedInMemoryIndexEnabled());
  if (auto res_id_32 = ToResId32(res_id); res_id_32.has_value()) {
    return std::get<ConsolidatedImpl<ResId32>>(impl32_)
        .GetEntryMetadataForTesting(hash, *res_id_32);  // IN-TEST
  }
  if (impl64_) {
    return std::get<ConsolidatedImpl<SqlPersistentStoreResId>>(*impl64_)
        .GetEntryMetadataForTesting(hash, res_id);  // IN-TEST
  }
  return std::nullopt;
}

}  // namespace disk_cache
