// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_INDEX_DATABASE_H_
#define NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_INDEX_DATABASE_H_

#include <cstdint>
#include <string>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/types/expected.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/disk_cache/sql/sql_backend_aliases.h"
#include "sql/database.h"
#include "sql/meta_table.h"

namespace disk_cache {

// Manages the Index Database for the SQL Shared Cache.
// This database maps NetworkIsolationKeys to unique integer IDs (db_ids),
// which are used to name the per-NIK database files.
class NET_EXPORT_PRIVATE SqlSharedCacheIndexDatabase {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(SqlSharedCacheIndexDatabaseResult)
  enum class Error {
    // Reserved for histograms.
    // kSuccess = 0,
    kNotFound = 1,
    kFailedForTesting = 2,
    kFailedToOpenDatabase = 3,
    kFailedToRazeIncompatibleDatabase = 4,
    kFailedToStartTransaction = 5,
    kFailedToInitializeMetaTable = 6,
    kFailedToCommitTransaction = 7,
    kFailedToExecute = 8,
    kInvalidNetworkIsolationKey = 9,
    kFailedToCreateDirectory = 10,
    kFailedToCreateStoragesTable = 11,
    kFailedToCreateUniqueIndex = 12,
    kMaxValue = kFailedToCreateUniqueIndex,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:SqlSharedCacheIndexDatabaseResult)

  static constexpr Error kNoErrorForMetrics = static_cast<Error>(0);

  explicit SqlSharedCacheIndexDatabase(const base::FilePath& storage_directory);
  ~SqlSharedCacheIndexDatabase();

  // Initializes the database.
  base::expected<void, Error> Initialize();

  // Retrieves the cache key string representation of the NetworkIsolationKey
  // associated with the given `shared_cache_db_id`.
  // If the `shared_cache_db_id` does not exist, returns an error (kNotFound).
  base::expected<std::string, Error> GetIsolationKeyStringByDbId(
      SqlSharedCacheDbId shared_cache_db_id);

  // Retrieves the db_id (SqlSharedCacheDbId) for the given
  // `network_isolation_key`.
  // If `create_if_not_exists` is true and the key does not exist, it is
  // created. If `create_if_not_exists` is false and the key does not exist,
  // returns an error (kNotFound). If `network_isolation_key` is transient,
  // returns an error (kInvalidNetworkIsolationKey).
  base::expected<SqlSharedCacheDbId, Error> GetDbIdByNetworkIsolationKey(
      const net::NetworkIsolationKey& network_isolation_key,
      bool create_if_not_exists);

  // Deletes the entry for the given `shared_cache_db_id`.
  // If the `shared_cache_db_id` does not exist, returns an error (kNotFound).
  base::expected<void, Error> DeleteByDbId(
      SqlSharedCacheDbId shared_cache_db_id);

  void SetSimulateDbFailureForTesting(bool fail);

 private:
  base::expected<void, Error> InitializeInternal();
  base::expected<std::string, Error> GetIsolationKeyStringByDbIdInternal(
      SqlSharedCacheDbId shared_cache_db_id);
  base::expected<SqlSharedCacheDbId, Error>
  GetDbIdByNetworkIsolationKeyInternal(
      const net::NetworkIsolationKey& network_isolation_key,
      bool create_if_not_exists);
  base::expected<void, Error> DeleteByDbIdInternal(
      SqlSharedCacheDbId shared_cache_db_id);

  const base::FilePath database_path_;
  sql::Database db_;
  sql::MetaTable meta_table_;
  bool simulate_db_failure_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SQL_SHARED_CACHE_INDEX_DATABASE_H_
