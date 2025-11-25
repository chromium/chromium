// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_BACKEND_ALIASES_H_
#define NET_DISK_CACHE_SQL_SQL_BACKEND_ALIASES_H_

#include "base/types/strong_alias.h"

// Defines various strong aliases used for SQL disk cache backend.
// They use base::StrongAlias to avoid type confusion.

namespace disk_cache {

// The primary key for resources managed in the SqlPersistentStore's resources
// table.
using SqlPersistentStoreResId =
    base::StrongAlias<class SqlPersistentStoreResIdTag, int64_t>;

// A unique identifier for a database shard.
using SqlPersistentStoreShardId =
    base::StrongAlias<class SqlPersistentStoreShardIdTag, uint8_t>;

// The hash value used in the CacheEntryKey.
using CacheEntryKeyHash =
    base::StrongAlias<class CacheEntryKeyHashTag, int32_t>;

// Represents hints for an entry's in-memory data, used for optimizing cache
// behavior. For example, these hints can indicate if an entry is unusable due
// to stale freshness headers, allowing for quicker optimistic deletion.
// The values correspond to the flags defined in MemoryEntryDataHints in
// net/disk_cache/memory_entry_data_hints.h.
using MemoryEntryDataHints =
    base::StrongAlias<class MemoryEntryDataHintsTag, uint8_t>;

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_BACKEND_ALIASES_H_
