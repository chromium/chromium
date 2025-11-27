// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_BACKEND_CONSTANTS_H_
#define NET_DISK_CACHE_SQL_SQL_BACKEND_CONSTANTS_H_

#include <cstdint>
#include <string_view>

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace disk_cache {

// This constant defines the denominator for the maximum fraction of the cache
// that a single entry can consume. For example, if this value is 8, a single
// entry can consume at most 1/8th of the total cache size.
// Maximum fraction of the cache that one entry can consume.
inline constexpr int kSqlBackendMaxFileRatioDenominator = 8;

// A maximum file size when the overall cache size is very small, meaning there
// is very little free disk space.
inline constexpr int64_t kSqlBackendMinFileSizeLimit = 5 * 1024 * 1024;

// Keys for the key-value pairs stored in the `meta` table.
inline constexpr std::string_view kSqlBackendMetaTableKeyEntryCount =
    "EntryCount";
inline constexpr std::string_view kSqlBackendMetaTableKeyTotalSize =
    "TotalSize";

// The file name prefix of the SQL backend database shards.
inline constexpr std::string_view kSqlBackendDatabaseFileNamePrefix = "sqldb";

// The file name of the first shard of the SQL backend database.
inline constexpr base::FilePath::CharType kSqlBackendDatabaseShard0FileName[] =
    FILE_PATH_LITERAL("sqldb0");

// The name of the fake index file. This file is created to signal the presence
// of the SQL backend and to prevent other backends from trying to use the same
// directory.
inline constexpr base::FilePath::CharType kSqlBackendFakeIndexFileName[] =
    FILE_PATH_LITERAL("index");

// The prefix of the fake index file.
// The full content is the prefix followed by the number of shards.
inline constexpr std::string_view kSqlBackendFakeIndexPrefix = "SQLCache";

// ----------------------------------------------------------------------------
// Database Scheme Version history:
// Version 1: Initial schema. The first field trial experiment started on
//            Dev/Canary with this version.
// Version 2: https://crrev.com/c/6917159 added `cache_key_hash` column and an
//            index on `(cache_key_hash, doomed)` to the `resources` table.
// Version 3: https://crrev.com/c/6940353 replaced `(token_high, token_low)`
//            with `res_id` in `resources` and `blobs` tables.
// Version 4: https://crrev.com/c/7005549 changed the eviction logic to use
//            `res_id` instead of `cache_key` and added a covering index on
//            `(last_used, bytes_usage)` to the `resources` table.
// Version 5: https://crrev.com/c/7005917 changed how doomed entries are
//            cleaned up. Instead of a delayed task, cleanup is now triggered
//            during browser idle periods. Also, the index on `res_id` for
//            doomed entries was removed as it's no longer needed.
// Version 6: https://crrev.com/c/7006231 changed the hash function for cache
//            keys to base::PersistentHash, which uses a 32-bit hash. This is a
//            breaking change as the previous version used a 64-bit hash.
// Version 7: https://crrev.com/c/7023771 added `check_sum` column in both of
//            the `resources` table and the `blobs` table.
// Version 8: https://crrev.com/c/7171346 added a `hints` column to the
//            `resources` table to store in-memory data hints.
// ----------------------------------------------------------------------------

// The oldest database schema version that the current code can read.
// A database with a version older than this will be razed as it's considered
// obsolete and the code no longer supports migrating from it.
inline constexpr int kSqlBackendLowestSupportedDatabaseVersion = 8;

// The current version of the database schema. This should be incremented for
// any schema change.
inline constexpr int kSqlBackendCurrentDatabaseVersion = 8;

// The oldest application version that can use a database with the current
// schema. If a schema change is not backward-compatible, this must be set to
// the same value as `kSqlBackendCurrentDatabaseVersion`.
inline constexpr int kSqlBackendCompatibleDatabaseVersion = 8;

// Estimated static size overhead for a resource entry in the database,
// excluding the key and any blob data. This is a conservative estimate based on
// empirical testing and is intended to account for the overhead of the row in
// the `resources` table, SQLite's B-tree overhead per entry, and other
// miscellaneous metadata. The
// `SqlPersistentStoreTest.StaticResourceSizeEstimation` test provides a basic
// validation of this constant against the actual file size.
inline constexpr int kSqlBackendStaticResourceSize = 300;

// Defines the number of streams supported by the SQL backend.
// The SQL backend only supports stream 0 and stream 1.
static const int kSqlBackendStreamCount = 2;

// High watermark for cache eviction, in thousandths (permille) of the max size.
// Eviction is triggered when the cache size exceeds this.
inline constexpr int kSqlBackendEvictionHighWaterMarkPermille = 950;

// High watermark for cache eviction during idle time, in thousandths (permille)
// of the max size. This is lower than the regular high watermark to allow for
// more proactive eviction when the browser is not busy.
inline constexpr int kSqlBackendIdleTimeEvictionHighWaterMarkPermille = 925;

// Low watermark for cache eviction, in thousandths (permille) of the max size.
// Eviction continues until the cache size is below this.
inline constexpr int kSqlBackendEvictionLowWaterMarkPermille = 900;

// The delay after backend initialization before running post-initialization
// tasks. These tasks, such as cleaning up doomed entries from previous
// sessions and loading the in-memory index, are deferred to avoid impacting
// startup performance.
inline constexpr base::TimeDelta kSqlBackendPostInitializationTasksDelay =
    base::Minutes(1);

// The prefix for histograms related to the SQL disk cache backend.
inline constexpr std::string_view kSqlDiskCacheBackendHistogramPrefix =
    "Net.SqlDiskCache.Backend.";

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_BACKEND_CONSTANTS_H_
