// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache_index_database.h"

#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "net/disk_cache/sql/sql_shared_cache_index_database_queries.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

using disk_cache_sql_queries::GetSharedCacheIndexDatabaseQuery;
using disk_cache_sql_queries::SharedCacheIndexDatabaseQuery;

namespace disk_cache {

namespace {

constexpr int kSharedCacheIndexDatabaseCurrentVersion = 1;
constexpr int kSharedCacheIndexDatabaseCompatibleVersion = 1;
constexpr std::string_view kSqlSharedCacheIndexDatabaseHistogramPrefix =
    "Net.SqlSharedCacheIndexDatabase.";

void RecordTimeAndErrorResultHistogram(
    std::string_view method_name,
    base::TimeDelta time_delta,
    SqlSharedCacheIndexDatabase::Error error) {
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat({kSqlSharedCacheIndexDatabaseHistogramPrefix, method_name,
                    error == SqlSharedCacheIndexDatabase::kNoErrorForMetrics
                        ? ".SuccessTime"
                        : ".FailureTime"}),
      time_delta);
  base::UmaHistogramEnumeration(
      base::StrCat({kSqlSharedCacheIndexDatabaseHistogramPrefix, method_name,
                    ".Result"}),
      error);
}

}  // namespace

SqlSharedCacheIndexDatabase::SqlSharedCacheIndexDatabase(
    const base::FilePath& storage_directory)
    : database_path_(
          storage_directory.Append(kSqlBackendSharedCacheIndexFileName)),
      db_(sql::DatabaseOptions()
              .set_exclusive_locking(true)
#if BUILDFLAG(IS_WIN)
              .set_exclusive_database_file_lock(true)
#endif  // IS_WIN
              .set_preload(true)
              .set_wal_mode(true),
          sql::Database::Tag("SharedCacheIndex")) {
}

SqlSharedCacheIndexDatabase::~SqlSharedCacheIndexDatabase() = default;

base::expected<void, SqlSharedCacheIndexDatabase::Error>
SqlSharedCacheIndexDatabase::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_BEGIN0("disk_cache", "SqlSharedCacheIndexDatabase::Initialize");
  base::ElapsedTimer timer;
  auto result = InitializeInternal();
  RecordTimeAndErrorResultHistogram("Initialize", timer.Elapsed(),
                                    result.error_or(kNoErrorForMetrics));
  TRACE_EVENT_END1("disk_cache", "SqlSharedCacheIndexDatabase::Initialize",
                   "result",
                   static_cast<int>(result.error_or(kNoErrorForMetrics)));
  return result;
}

base::expected<void, SqlSharedCacheIndexDatabase::Error>
SqlSharedCacheIndexDatabase::InitializeInternal() {
  if (simulate_db_failure_) {
    return base::unexpected(Error::kFailedForTesting);
  }
  if (db_.is_open()) {
    return base::ok();
  }

  // TODO(crbug.com/473666511): Set an error callback on `db_` to handle
  // catastrophic errors (like SQLITE_CORRUPT). When such an error occurs, we
  // should call `db_.RazeAndPoison()` and also trigger a cleanup of all
  // isolated shared cache databases and entries in the main database
  // (SqlPersistentStore::Backend) that reference the shared cache.

  base::FilePath directory = database_path_.DirName();
  if (!base::CreateDirectory(directory)) {
    return base::unexpected(Error::kFailedToCreateDirectory);
  }

  if (!db_.Open(database_path_)) {
    return base::unexpected(Error::kFailedToOpenDatabase);
  }

  if (sql::MetaTable::RazeIfIncompatible(
          &db_, kSharedCacheIndexDatabaseCompatibleVersion,
          kSharedCacheIndexDatabaseCurrentVersion) ==
      sql::RazeIfIncompatibleResult::kFailed) {
    return base::unexpected(Error::kFailedToRazeIncompatibleDatabase);
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToStartTransaction);
  }

  if (!sql::MetaTable::DoesTableExist(&db_)) {
    // Initialize the schema.
    if (!db_.Execute(GetSharedCacheIndexDatabaseQuery(
            SharedCacheIndexDatabaseQuery::kCreateStoragesTable))) {
      return base::unexpected(Error::kFailedToCreateStoragesTable);
    }

    if (!db_.Execute(GetSharedCacheIndexDatabaseQuery(
            SharedCacheIndexDatabaseQuery::kCreateUniqueIndex))) {
      return base::unexpected(Error::kFailedToCreateUniqueIndex);
    }
  }

  // Initialize the meta table.
  if (!meta_table_.Init(&db_, kSharedCacheIndexDatabaseCurrentVersion,
                        kSharedCacheIndexDatabaseCompatibleVersion)) {
    return base::unexpected(Error::kFailedToInitializeMetaTable);
  }

  if (!transaction.Commit()) {
    return base::unexpected(Error::kFailedToCommitTransaction);
  }

  return base::ok();
}

base::expected<SqlSharedCacheDbId, SqlSharedCacheIndexDatabase::Error>
SqlSharedCacheIndexDatabase::GetDbIdByNetworkIsolationKey(
    const net::NetworkIsolationKey& network_isolation_key,
    bool create_if_not_exists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_BEGIN1(
      "disk_cache", "SqlSharedCacheIndexDatabase::GetDbIdByNetworkIsolationKey",
      "nik", network_isolation_key.ToDebugString());
  base::ElapsedTimer timer;
  auto result = GetDbIdByNetworkIsolationKeyInternal(network_isolation_key,
                                                     create_if_not_exists);
  RecordTimeAndErrorResultHistogram("GetDbIdByNetworkIsolationKey",
                                    timer.Elapsed(),
                                    result.error_or(kNoErrorForMetrics));
  TRACE_EVENT_END1(
      "disk_cache", "SqlSharedCacheIndexDatabase::GetDbIdByNetworkIsolationKey",
      "result", [&](perfetto::TracedValue trace_context) {
        auto dict = std::move(trace_context).WriteDictionary();
        dict.Add("error",
                 static_cast<int>(result.error_or(kNoErrorForMetrics)));
        if (result.has_value()) {
          dict.Add("db_id", result.value().value());
        }
      });
  return result;
}

base::expected<SqlSharedCacheDbId, SqlSharedCacheIndexDatabase::Error>
SqlSharedCacheIndexDatabase::GetDbIdByNetworkIsolationKeyInternal(
    const net::NetworkIsolationKey& network_isolation_key,
    bool create_if_not_exists) {
  if (simulate_db_failure_) {
    return base::unexpected(Error::kFailedForTesting);
  }
  if (!db_.is_open()) {
    return base::unexpected(Error::kFailedToOpenDatabase);
  }

  const auto isolation_key_str_opt = network_isolation_key.ToCacheKeyString();
  if (!isolation_key_str_opt) {
    return base::unexpected(Error::kInvalidNetworkIsolationKey);
  }

  // First try to find the existing key.
  {
    sql::Statement select_statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        GetSharedCacheIndexDatabaseQuery(
            SharedCacheIndexDatabaseQuery::kSelectDbIdByIsolationKey)));
    select_statement.BindString(0, *isolation_key_str_opt);
    if (select_statement.Step()) {
      return SqlSharedCacheDbId(select_statement.ColumnInt64(0));
    }
    if (!select_statement.Succeeded()) {
      return base::unexpected(Error::kFailedToExecute);
    }
  }

  if (!create_if_not_exists) {
    return base::unexpected(Error::kNotFound);
  }

  // If not found, insert a new one.
  sql::Statement insert_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, GetSharedCacheIndexDatabaseQuery(
                         SharedCacheIndexDatabaseQuery::kInsertStorage)));
  insert_statement.BindString(0, *isolation_key_str_opt);
  if (!insert_statement.Run()) {
    return base::unexpected(Error::kFailedToExecute);
  }
  return SqlSharedCacheDbId(db_.GetLastInsertRowId());
}

base::expected<std::string, SqlSharedCacheIndexDatabase::Error>
SqlSharedCacheIndexDatabase::GetIsolationKeyStringByDbId(
    SqlSharedCacheDbId shared_cache_db_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_BEGIN1("disk_cache",
                     "SqlSharedCacheIndexDatabase::GetIsolationKeyStringByDbId",
                     "db_id", shared_cache_db_id.value());
  base::ElapsedTimer timer;
  auto result = GetIsolationKeyStringByDbIdInternal(shared_cache_db_id);
  RecordTimeAndErrorResultHistogram("GetIsolationKeyStringByDbId",
                                    timer.Elapsed(),
                                    result.error_or(kNoErrorForMetrics));
  TRACE_EVENT_END1(
      "disk_cache", "SqlSharedCacheIndexDatabase::GetIsolationKeyStringByDbId",
      "result", [&](perfetto::TracedValue trace_context) {
        auto dict = std::move(trace_context).WriteDictionary();
        dict.Add("error",
                 static_cast<int>(result.error_or(kNoErrorForMetrics)));
        if (result.has_value()) {
          dict.Add("nik_string", result.value());
        }
      });
  return result;
}

base::expected<std::string, SqlSharedCacheIndexDatabase::Error>
SqlSharedCacheIndexDatabase::GetIsolationKeyStringByDbIdInternal(
    SqlSharedCacheDbId shared_cache_db_id) {
  if (simulate_db_failure_) {
    return base::unexpected(Error::kFailedForTesting);
  }
  if (!db_.is_open()) {
    return base::unexpected(Error::kFailedToOpenDatabase);
  }
  sql::Statement select_statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      GetSharedCacheIndexDatabaseQuery(
          SharedCacheIndexDatabaseQuery::kSelectIsolationKeyByDbId)));
  select_statement.BindInt64(0, shared_cache_db_id.value());
  if (select_statement.Step()) {
    return select_statement.ColumnString(0);
  }
  if (!select_statement.Succeeded()) {
    return base::unexpected(Error::kFailedToExecute);
  }
  return base::unexpected(Error::kNotFound);
}

base::expected<void, SqlSharedCacheIndexDatabase::Error>
SqlSharedCacheIndexDatabase::DeleteByDbId(
    SqlSharedCacheDbId shared_cache_db_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_BEGIN1("disk_cache", "SqlSharedCacheIndexDatabase::DeleteByDbId",
                     "db_id", shared_cache_db_id.value());
  base::ElapsedTimer timer;
  auto result = DeleteByDbIdInternal(shared_cache_db_id);
  RecordTimeAndErrorResultHistogram("DeleteByDbId", timer.Elapsed(),
                                    result.error_or(kNoErrorForMetrics));
  TRACE_EVENT_END1("disk_cache", "SqlSharedCacheIndexDatabase::DeleteByDbId",
                   "result",
                   static_cast<int>(result.error_or(kNoErrorForMetrics)));
  return result;
}

base::expected<void, SqlSharedCacheIndexDatabase::Error>
SqlSharedCacheIndexDatabase::DeleteByDbIdInternal(
    SqlSharedCacheDbId shared_cache_db_id) {
  if (simulate_db_failure_) {
    return base::unexpected(Error::kFailedForTesting);
  }
  if (!db_.is_open()) {
    return base::unexpected(Error::kFailedToOpenDatabase);
  }

  sql::Statement delete_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, GetSharedCacheIndexDatabaseQuery(
                         SharedCacheIndexDatabaseQuery::kDeleteStorage)));
  delete_statement.BindInt64(0, shared_cache_db_id.value());
  if (!delete_statement.Run()) {
    return base::unexpected(Error::kFailedToExecute);
  }
  if (db_.GetLastChangeCount() == 0) {
    return base::unexpected(Error::kNotFound);
  }
  return base::ok();
}

void SqlSharedCacheIndexDatabase::SetSimulateDbFailureForTesting(bool fail) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  simulate_db_failure_ = fail;
}

}  // namespace disk_cache
