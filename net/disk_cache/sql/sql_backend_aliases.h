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

// The unique identifier for a shared cache in the SqlSharedCacheIndexDatabase.
using SqlSharedCacheDbId =
    base::StrongAlias<class SqlSharedCacheDbIdTag, int64_t>;

// The rowid of a record within the SqlSharedCache isolated database.
using SqlSharedCacheRowId =
    base::StrongAlias<class SqlSharedCacheRowIdTag, int64_t>;

// Uniquely identifies a resource entry stored within the SqlSharedCache
// isolated database.
struct SqlSharedCacheResourceId {
  SqlSharedCacheDbId db_id;
  SqlSharedCacheRowId row_id;
};

// The 32-bit hash of a resource URL, used for fast indexing and lookups within
// the SqlSharedCache databases.
using SqlSharedCacheUrlHash =
    base::StrongAlias<class SqlSharedCacheUrlHashTag, int32_t>;

// The hash value used in the CacheEntryKey.
using CacheEntryKeyHash =
    base::StrongAlias<class CacheEntryKeyHashTag, int32_t>;

// Represents hints for an entry's in-memory data, used for optimizing cache
// behavior. For example, these hints can indicate if an entry is unusable due
// to stale freshness headers, allowing for quicker optimistic deletion.
// The values correspond to the flags defined in MemoryEntryDataHintBits in
// net/disk_cache/memory_entry_data_hints.h.
using MemoryEntryDataHints =
    base::StrongAlias<class MemoryEntryDataHintsTag, uint8_t>;

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_BACKEND_ALIASES_H_
