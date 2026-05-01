// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_INDEX_DATABASE_QUERIES_H_
#define NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_INDEX_DATABASE_QUERIES_H_

#include "base/notreached.h"
#include "base/strings/cstring_view.h"

namespace disk_cache_sql_queries {
namespace internal {

inline constexpr char kSharedCacheIndexDatabaseCreateStoragesTable[] =
    // clang-format off
    "CREATE TABLE storages("
        "db_id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,"
        "isolation_key TEXT NOT NULL)";
// clang-format on

inline constexpr char kSharedCacheIndexDatabaseCreateUniqueIndex[] =
    "CREATE UNIQUE INDEX unique_index ON storages(isolation_key)";

inline constexpr char kSharedCacheIndexDatabaseSelectDbIdByIsolationKey[] =
    "SELECT db_id FROM storages WHERE isolation_key = ?";

inline constexpr char kSharedCacheIndexDatabaseSelectIsolationKeyByDbId[] =
    "SELECT isolation_key FROM storages WHERE db_id = ?";

inline constexpr char kSharedCacheIndexDatabaseInsertStorage[] =
    "INSERT INTO storages(isolation_key) VALUES(?)";

inline constexpr char kSharedCacheIndexDatabaseDeleteStorage[] =
    "DELETE FROM storages WHERE db_id = ?";

}  // namespace internal

enum class SharedCacheIndexDatabaseQuery {
  kCreateStoragesTable,
  kCreateUniqueIndex,
  kSelectDbIdByIsolationKey,
  kSelectIsolationKeyByDbId,
  kInsertStorage,
  kDeleteStorage,

  kMaxValue = kDeleteStorage,
};

inline base::cstring_view GetSharedCacheIndexDatabaseQuery(
    SharedCacheIndexDatabaseQuery query) {
  switch (query) {
    case SharedCacheIndexDatabaseQuery::kCreateStoragesTable:
      return internal::kSharedCacheIndexDatabaseCreateStoragesTable;
    case SharedCacheIndexDatabaseQuery::kCreateUniqueIndex:
      return internal::kSharedCacheIndexDatabaseCreateUniqueIndex;
    case SharedCacheIndexDatabaseQuery::kSelectDbIdByIsolationKey:
      return internal::kSharedCacheIndexDatabaseSelectDbIdByIsolationKey;
    case SharedCacheIndexDatabaseQuery::kSelectIsolationKeyByDbId:
      return internal::kSharedCacheIndexDatabaseSelectIsolationKeyByDbId;
    case SharedCacheIndexDatabaseQuery::kInsertStorage:
      return internal::kSharedCacheIndexDatabaseInsertStorage;
    case SharedCacheIndexDatabaseQuery::kDeleteStorage:
      return internal::kSharedCacheIndexDatabaseDeleteStorage;
  }
  NOTREACHED();
}

}  // namespace disk_cache_sql_queries

#endif  // NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_INDEX_DATABASE_QUERIES_H_
