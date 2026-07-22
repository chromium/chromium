// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_ISOLATED_DATABASE_QUERIES_H_
#define NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_ISOLATED_DATABASE_QUERIES_H_

#include "base/strings/cstring_view.h"

namespace disk_cache_sql_queries {

namespace internal {

// Creates the resources table.
// `hash` is the 32-bit hash of a resource URL, used for fast indexing and
// lookups within the isolated database.
inline constexpr char kSharedCacheIsolatedCreateResourcesTable[] =
    "CREATE TABLE resources ("
    "hash INTEGER NOT NULL,"
    "url TEXT NOT NULL,"
    "headers BLOB NOT NULL,"
    "body BLOB NOT NULL,"
    "is_ready INTEGER NOT NULL)";

inline constexpr char kSharedCacheIsolatedCreateResourcesTableIndex[] =
    "CREATE INDEX index_resources_hash ON resources(hash)";

inline constexpr char kSharedCacheIsolatedInsertResource[] =
    "INSERT INTO resources (hash, url, headers, body, is_ready) "
    "VALUES (?, ?, ?, ?, ?)";

inline constexpr char kSharedCacheIsolatedSetResourceReady[] =
    "UPDATE resources SET is_ready = 1 WHERE rowid = ?";

inline constexpr char kSharedCacheIsolatedSelectUrlAndReadyByRowId[] =
    "SELECT url, is_ready FROM resources WHERE rowid = ?";

inline constexpr char kSharedCacheIsolatedReaderSelectResource[] =
    "SELECT rowid FROM resources WHERE hash = ? AND url = ? AND is_ready = 1";

inline constexpr char kSharedCacheIsolatedDeleteResourceByRowId[] =
    "DELETE FROM resources WHERE rowid = ?";

}  // namespace internal

enum class SharedCacheIsolatedDatabaseQuery {
  kCreateResourcesTable = 0,
  kCreateResourcesTableIndex = 1,
  kInsertResource = 2,
  kSetResourceReady = 3,
  kSelectUrlAndReadyByRowId = 4,
  kReaderSelectResource = 5,
  kDeleteResourceByRowId = 6,
  kMaxValue = kDeleteResourceByRowId,
};

inline constexpr base::cstring_view GetSharedCacheIsolatedDatabaseQuery(
    SharedCacheIsolatedDatabaseQuery query) {
  switch (query) {
    case SharedCacheIsolatedDatabaseQuery::kCreateResourcesTable:
      return internal::kSharedCacheIsolatedCreateResourcesTable;
    case SharedCacheIsolatedDatabaseQuery::kCreateResourcesTableIndex:
      return internal::kSharedCacheIsolatedCreateResourcesTableIndex;
    case SharedCacheIsolatedDatabaseQuery::kInsertResource:
      return internal::kSharedCacheIsolatedInsertResource;
    case SharedCacheIsolatedDatabaseQuery::kSetResourceReady:
      return internal::kSharedCacheIsolatedSetResourceReady;
    case SharedCacheIsolatedDatabaseQuery::kSelectUrlAndReadyByRowId:
      return internal::kSharedCacheIsolatedSelectUrlAndReadyByRowId;
    case SharedCacheIsolatedDatabaseQuery::kReaderSelectResource:
      return internal::kSharedCacheIsolatedReaderSelectResource;
    case SharedCacheIsolatedDatabaseQuery::kDeleteResourceByRowId:
      return internal::kSharedCacheIsolatedDeleteResourceByRowId;
  }
}

}  // namespace disk_cache_sql_queries

#endif  // NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_ISOLATED_DATABASE_QUERIES_H_
