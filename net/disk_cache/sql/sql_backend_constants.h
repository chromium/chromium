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

inline constexpr base::FilePath::CharType kSqlBackendDatabaseFileName[] =
    FILE_PATH_LITERAL("sqldb");

// The name of the fake index file. This file is created to signal the presence
// of the SQL backend and to prevent other backends from trying to use the same
// directory.
inline constexpr base::FilePath::CharType kSqlBackendFakeIndexFileName[] =
    FILE_PATH_LITERAL("index");

// The magic number for the fake index file. This is "SQLCache" in
// little-endian.
inline constexpr uint64_t kSqlBackendFakeIndexMagicNumber =
    UINT64_C(0x65686361434c5153);

// The oldest database schema version that the current code can read.
// A database with a version older than this will be razed as it's considered
// obsolete and the code no longer supports migrating from it.
inline constexpr int kSqlBackendLowestSupportedDatabaseVersion = 1;

// The current version of the database schema. This should be incremented for
// any schema change.
inline constexpr int kSqlBackendCurrentDatabaseVersion = 1;

// The oldest application version that can use a database with the current
// schema. If a schema change is not backward-compatible, this must be set to
// the same value as `kSqlBackendCurrentDatabaseVersion`.
inline constexpr int kSqlBackendCompatibleDatabaseVersion = 1;

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

// Divisor used to calculate the high and low watermarks for cache eviction.
// The high watermark is `max_size - (max_size / divisor)`, and the low
// watermark is `max_size - 2 * (max_size / divisor)`. Eviction is triggered
// when the cache size exceeds the high watermark and continues until it is
// below the low watermark.
inline constexpr int kSqlBackendEvictionMarginDivisor = 20;

// The delay after backend initialization before running a one-time cleanup task
// to delete doomed entries. This task removes entries that were doomed in a
// previous session but not fully deleted (e.g., due to a crash), ensuring
// that their disk space is reclaimed.
// Note: This value is set assuming use with HTTP Cache, but if the SQL backend
// is used with Cache Storage, it should be a shorter value.
inline constexpr base::TimeDelta kSqlBackendDeleteDoomedEntriesDelay =
    base::Minutes(10);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_BACKEND_CONSTANTS_H_
