// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_BACKEND_IDS_H_
#define NET_DISK_CACHE_SQL_SQL_BACKEND_IDS_H_

#include "base/types/strong_alias.h"

// Defines various IDs used for SQL disk cache backend. The IDs use
// base::StrongAlias to avoid type confusion.

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

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_BACKEND_IDS_H_
