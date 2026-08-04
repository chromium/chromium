// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/sql_shared_cache_isolated_database.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected_macros.h"
#include "components/sqlite_vfs/client.h"
#include "components/sqlite_vfs/sqlite_sandboxed_vfs.h"
#include "components/sqlite_vfs/vfs_utils.h"
#include "net/base/features.h"
#include "net/disk_cache/sql/sql_backend_constants.h"
#include "net/disk_cache/sql/sql_shared_cache_isolated_database_queries.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/streaming_blob_handle.h"
#include "sql/transaction.h"

using disk_cache_sql_queries::GetSharedCacheIsolatedDatabaseQuery;
using disk_cache_sql_queries::SharedCacheIsolatedDatabaseQuery;

namespace disk_cache {

namespace {

constexpr int kSharedCacheIsolatedDatabaseCurrentVersion = 1;
constexpr int kSharedCacheIsolatedDatabaseCompatibleVersion = 1;

}  // namespace

// static
std::unique_ptr<SqlSharedCacheIsolatedDatabase::DatabaseAssets>
SqlSharedCacheIsolatedDatabase::DatabaseAssets::MaybeCreate(
    const base::FilePath& directory,
    SqlSharedCacheDbId shared_cache_db_id) {
  ASSIGN_OR_RETURN(
      auto pending_file_set,
      sqlite_vfs::MakePendingFileSet(
          sqlite_vfs::Client::kSharedCacheIsolated, directory,
          base::FilePath::FromASCII(
              base::StrCat({kSqlBackendSharedCacheIsolatedFileNamePrefix,
                            base::NumberToString(*shared_cache_db_id)})),
          /*single_connection=*/false,
          net::features::kRendererAccessibleHttpCacheWalMode.Get()),
      [](auto) { return nullptr; });
  ASSIGN_OR_RETURN(auto vfs_file_set,
                   sqlite_vfs::SqliteVfsFileSet::Bind(
                       sqlite_vfs::Client::kSharedCacheIsolated,
                       std::move(pending_file_set)),
                   [] { return nullptr; });
  return std::make_unique<DatabaseAssets>(directory, shared_cache_db_id,
                                          std::move(vfs_file_set),
                                          base::PassKey<DatabaseAssets>());
}

SqlSharedCacheIsolatedDatabase::DatabaseAssets::DatabaseAssets(
    const base::FilePath& directory,
    SqlSharedCacheDbId shared_cache_db_id,
    sqlite_vfs::SqliteVfsFileSet vfs_file_set,
    base::PassKey<DatabaseAssets>)
    : directory_(directory),
      shared_cache_db_id_(shared_cache_db_id),
      vfs_file_set_(std::move(vfs_file_set)),
      unregister_runner_(sqlite_vfs::SqliteSandboxedVfsDelegate::GetInstance()
                             ->RegisterSandboxedFiles(vfs_file_set_)),
      db_(sql::DatabaseOptions()
              .set_read_only(vfs_file_set_.read_only())
              .set_exclusive_locking(vfs_file_set_.is_single_connection())
              .set_wal_mode(vfs_file_set_.wal_mode())
              .set_vfs_name_discouraged(
                  sqlite_vfs::SqliteSandboxedVfsDelegate::kSqliteVfsName)
              .set_mmap_enabled(false),
          sql::Database::Tag("SharedCacheIsolated")) {}

SqlSharedCacheIsolatedDatabase::DatabaseAssets::~DatabaseAssets() = default;

base::FilePath
SqlSharedCacheIsolatedDatabase::DatabaseAssets::GetDbVirtualFilePath() const {
  return vfs_file_set_.GetDbVirtualFilePath();
}

base::expected<sqlite_vfs::PendingFileSet, sqlite_vfs::FileSetError>
SqlSharedCacheIsolatedDatabase::DatabaseAssets::ShareConnection() {
  return sqlite_vfs::ShareConnection(
      directory_,
      base::FilePath::FromASCII(
          base::StrCat({kSqlBackendSharedCacheIsolatedFileNamePrefix,
                        base::NumberToString(*shared_cache_db_id_)})),
      vfs_file_set_, /*read_write=*/false);
}

void SqlSharedCacheIsolatedDatabase::DatabaseAssets::AbandonAndDeleteFiles() {
  db_.Close();
  vfs_file_set_.Abandon();
  base::FilePath base_name = base::FilePath::FromASCII(
      base::StrCat({kSqlBackendSharedCacheIsolatedFileNamePrefix,
                    base::NumberToString(shared_cache_db_id_.value())}));
  sqlite_vfs::DeleteFiles(sqlite_vfs::Client::kSharedCacheIsolated, directory_,
                          base_name);
}

SqlSharedCacheIsolatedDatabase::SqlSharedCacheIsolatedDatabase(
    std::string nik_string,
    const base::FilePath& directory,
    SqlSharedCacheDbId shared_cache_db_id)
    : nik_string_(std::move(nik_string)),
      db_assets_(DatabaseAssets::MaybeCreate(directory, shared_cache_db_id)) {}

SqlSharedCacheIsolatedDatabase::~SqlSharedCacheIsolatedDatabase() = default;

base::expected<sqlite_vfs::PendingFileSet,
               SqlSharedCacheIsolatedDatabase::Error>
SqlSharedCacheIsolatedDatabase::GetSharedReadOnlyConnection() {
  if (!db_assets_) {
    return base::unexpected(Error::kFailedToOpenVfsFileSet);
  }
  auto pending_file_set = db_assets_->ShareConnection();
  if (!pending_file_set.has_value()) {
    return base::unexpected(Error::kFailedToShareConnection);
  }
  return std::move(*pending_file_set);
}

void SqlSharedCacheIsolatedDatabase::
    SetSimulateDbFailureCallbackForTesting(  // IN-TEST
        SimFailedCallback callback) {
  simulate_db_failure_callback_ = std::move(callback);
}

bool SqlSharedCacheIsolatedDatabase::ShouldSimulateFailure(
    OperationForTesting op) const {
  return simulate_db_failure_callback_ && simulate_db_failure_callback_.Run(op);
}

base::expected<void, SqlSharedCacheIsolatedDatabase::Error>
SqlSharedCacheIsolatedDatabase::Init() {
  if (ShouldSimulateFailure(OperationForTesting::kInit)) {
    return base::unexpected(Error::kFailedForTesting);
  }
  if (!db_assets_) {
    return base::unexpected(Error::kFailedToOpenVfsFileSet);
  }
  sql::Database& db = db_assets_->db();
  if (db.is_open()) {
    return base::ok();
  }
  if (!db.Open(db_assets_->GetDbVirtualFilePath())) {
    return base::unexpected(Error::kFailedToOpenDatabase);
  }

  if (sql::MetaTable::RazeIfIncompatible(
          &db, kSharedCacheIsolatedDatabaseCompatibleVersion,
          kSharedCacheIsolatedDatabaseCurrentVersion) ==
      sql::RazeIfIncompatibleResult::kFailed) {
    return base::unexpected(Error::kFailedToRazeIncompatibleVersionDatabase);
  }

  sql::MetaTable meta_table;
  if (sql::MetaTable::DoesTableExist(&db)) {
    std::string saved_nik;
    if (!meta_table.Init(&db, kSharedCacheIsolatedDatabaseCurrentVersion,
                         kSharedCacheIsolatedDatabaseCompatibleVersion) ||
        !meta_table.GetValue(kSqlBackendSharedCacheIsolatedMetaTableKeyNik,
                             &saved_nik) ||
        saved_nik != nik_string_) {
      if (!db.Raze()) {
        return base::unexpected(Error::kFailedToRazeIncompatibleNikDatabase);
      }
      meta_table.Reset();
    } else {
      return base::ok();
    }
  }

  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToStartTransaction);
  }

  if (!db.Execute(GetSharedCacheIsolatedDatabaseQuery(
          SharedCacheIsolatedDatabaseQuery::kCreateResourcesTable))) {
    return base::unexpected(Error::kFailedToCreateResourcesTable);
  }

  if (!db.Execute(GetSharedCacheIsolatedDatabaseQuery(
          SharedCacheIsolatedDatabaseQuery::kCreateResourcesTableIndex))) {
    return base::unexpected(Error::kFailedToCreateResourcesTableIndex);
  }

  if (!meta_table.Init(&db, kSharedCacheIsolatedDatabaseCurrentVersion,
                       kSharedCacheIsolatedDatabaseCompatibleVersion)) {
    return base::unexpected(Error::kFailedToInitMetaTable);
  }

  if (!meta_table.SetValue(kSqlBackendSharedCacheIsolatedMetaTableKeyNik,
                           nik_string_)) {
    return base::unexpected(Error::kFailedToSetNikInMetaTable);
  }

  if (!transaction.Commit()) {
    return base::unexpected(Error::kFailedToCommitTransaction);
  }

  return base::ok();
}

base::expected<SqlSharedCacheRowId, SqlSharedCacheIsolatedDatabase::Error>
SqlSharedCacheIsolatedDatabase::Insert(const CacheEntryKey& entry_key,
                                       scoped_refptr<net::IOBuffer> headers,
                                       uint32_t total_body_size,
                                       scoped_refptr<net::IOBuffer> body) {
  if (ShouldSimulateFailure(OperationForTesting::kInsert)) {
    return base::unexpected(Error::kFailedForTesting);
  }
  if (!db_assets_ || !db_assets_->db().is_open()) {
    return base::unexpected(Error::kDatabaseNotOpen);
  }
  if (total_body_size >
      static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
    return base::unexpected(Error::kBodyTooLarge);
  }

  sql::Transaction transaction(&db_assets_->db());
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToStartTransaction);
  }

  SqlSharedCacheRowId shared_cache_row_id;
  const bool is_ready =
      (total_body_size == 0) ||
      (body && total_body_size == static_cast<uint32_t>(body->size()));
  constexpr base::span<uint8_t, 0> kEmptySpan;

  {
    sql::Statement statement(db_assets_->db().GetCachedStatement(
        SQL_FROM_HERE, GetSharedCacheIsolatedDatabaseQuery(
                           SharedCacheIsolatedDatabaseQuery::kInsertResource)));
    statement.BindInt64(0, entry_key.resource_url_hash().value());
    statement.BindString(1, entry_key.resource_url());
    // Wrapping the span with a RefCountedStaticMemory to avoid memory copy.
    // SAFETY: The memory referenced by `headers->span()`/`kEmptySpan` must
    // outlive `statement`.
    statement.BindBlob(2, base::MakeRefCounted<base::RefCountedStaticMemory>(
                              headers ? headers->span() : kEmptySpan));
    if (is_ready) {
      // SAFETY: The memory referenced by `body->span()`/`kEmptySpan` must
      // outlive `statement`.
      statement.BindBlob(
          3, base::MakeRefCounted<base::RefCountedStaticMemory>(
                 body ? body->span().first(total_body_size) : kEmptySpan));
    } else {
      statement.BindBlobForStreaming(3, total_body_size);
    }
    statement.BindBool(4, is_ready);

    if (!statement.Run()) {
      return base::unexpected(Error::kFailedToExecuteStatement);
    }
    shared_cache_row_id =
        SqlSharedCacheRowId(db_assets_->db().GetLastInsertRowId());
  }

  if (body && !is_ready) {
    RETURN_IF_ERROR(WriteBodyInternal(entry_key, shared_cache_row_id, 0, body,
                                      /*set_ready=*/false,
                                      /*in_transaction=*/true));
  }

  if (!transaction.Commit()) {
    return base::unexpected(Error::kFailedToCommitTransaction);
  }

  return shared_cache_row_id;
}

base::expected<void, SqlSharedCacheIsolatedDatabase::Error>
SqlSharedCacheIsolatedDatabase::WriteBody(
    const CacheEntryKey& entry_key,
    SqlSharedCacheRowId shared_cache_row_id,
    int offset,
    scoped_refptr<net::IOBuffer> buffer,
    bool set_ready) {
  return WriteBodyInternal(entry_key, shared_cache_row_id, offset, buffer,
                           set_ready, /*in_transaction=*/false);
}

base::expected<void, SqlSharedCacheIsolatedDatabase::Error>
SqlSharedCacheIsolatedDatabase::WriteBodyInternal(
    const CacheEntryKey& entry_key,
    SqlSharedCacheRowId shared_cache_row_id,
    int offset,
    scoped_refptr<net::IOBuffer> buffer,
    bool set_ready,
    bool in_transaction) {
  if (ShouldSimulateFailure(OperationForTesting::kWriteBody)) {
    return base::unexpected(Error::kFailedForTesting);
  }
  if (!db_assets_ || !db_assets_->db().is_open()) {
    return base::unexpected(Error::kDatabaseNotOpen);
  }
  const int size = buffer ? buffer->size() : 0;
  CHECK_GE(size, 0);
  if (offset < 0 || size > std::numeric_limits<int32_t>::max() - offset) {
    return base::unexpected(Error::kInvalidWriteRange);
  }

  std::optional<sql::Transaction> transaction;
  if (!in_transaction) {
    transaction.emplace(&db_assets_->db());
    if (!transaction->Begin()) {
      return base::unexpected(Error::kFailedToStartTransaction);
    }
  }

  if (size > 0) {
    ASSIGN_OR_RETURN(auto blob_handle,
                     db_assets_->db().GetStreamingBlob(
                         "resources", "body", shared_cache_row_id.value(),
                         /*readonly=*/false),
                     [] { return Error::kFailedToGetBlob; });
    if (!blob_handle.Write(offset, buffer->span())) {
      return base::unexpected(Error::kFailedToWriteBlob);
    }
  }

  if (set_ready) {
    sql::Statement statement(db_assets_->db().GetCachedStatement(
        SQL_FROM_HERE,
        GetSharedCacheIsolatedDatabaseQuery(
            SharedCacheIsolatedDatabaseQuery::kSetResourceReady)));
    statement.BindInt64(0, shared_cache_row_id.value());
    if (!statement.Run()) {
      return base::unexpected(Error::kFailedToSetReady);
    }
  }

  if (transaction && !transaction->Commit()) {
    return base::unexpected(Error::kFailedToCommitTransaction);
  }

  return base::ok();
}

SqlSharedCacheIsolatedDatabase::ReadResultOrError
SqlSharedCacheIsolatedDatabase::Read(const CacheEntryKey& entry_key,
                                     SqlSharedCacheRowId shared_cache_row_id,
                                     int body_size,
                                     int offset,
                                     scoped_refptr<net::IOBuffer> buffer) {
  if (ShouldSimulateFailure(OperationForTesting::kRead)) {
    return base::unexpected(Error::kFailedForTesting);
  }
  if (!db_assets_ || !db_assets_->db().is_open()) {
    return base::unexpected(Error::kDatabaseNotOpen);
  }
  CHECK(buffer);
  const int buf_len = buffer->size();
  CHECK_GE(buf_len, 0);
  CHECK_GE(body_size, 0);
  if (offset < 0 || buf_len > std::numeric_limits<int32_t>::max() - offset ||
      offset + buf_len > body_size) {
    return base::unexpected(Error::kInvalidReadRange);
  }

  {
    sql::Statement statement(db_assets_->db().GetCachedStatement(
        SQL_FROM_HERE,
        GetSharedCacheIsolatedDatabaseQuery(
            SharedCacheIsolatedDatabaseQuery::kSelectUrlAndReadyByRowId)));
    statement.BindInt64(0, shared_cache_row_id.value());
    if (!statement.Step() || !statement.ColumnBool(1) ||
        statement.ColumnString(0) != entry_key.resource_url()) {
      return base::unexpected(Error::kEntryNotFound);
    }
  }

  ASSIGN_OR_RETURN(auto blob_handle,
                   db_assets_->db().GetStreamingBlob(
                       "resources", "body", shared_cache_row_id.value(),
                       /*readonly=*/true),
                   [] { return Error::kFailedToGetBlob; });

  if (!blob_handle.Read(offset, buffer->span())) {
    return base::unexpected(Error::kFailedToReadBlob);
  }

  ReadResult read_result;
  read_result.read_bytes = buf_len;
  return read_result;
}

void SqlSharedCacheIsolatedDatabase::DeleteEntry(
    SqlSharedCacheRowId shared_cache_row_id) {
  if (ShouldSimulateFailure(OperationForTesting::kDeleteEntry)) {
    return;
  }
  if (!db_assets_ || !db_assets_->db().is_open()) {
    return;
  }
  sql::Statement statement(db_assets_->db().GetCachedStatement(
      SQL_FROM_HERE,
      GetSharedCacheIsolatedDatabaseQuery(
          SharedCacheIsolatedDatabaseQuery::kDeleteResourceByRowId)));
  statement.BindInt64(0, shared_cache_row_id.value());
  statement.Run();
}

base::expected<void, SqlSharedCacheIsolatedDatabase::Error>
SqlSharedCacheIsolatedDatabase::DeleteEntries(
    const std::vector<SqlSharedCacheRowId>& shared_cache_row_ids) {
  CHECK(!shared_cache_row_ids.empty());
  if (ShouldSimulateFailure(OperationForTesting::kDeleteEntries)) {
    return base::unexpected(Error::kFailedForTesting);
  }
  if (!db_assets_ || !db_assets_->db().is_open()) {
    return base::unexpected(Error::kDatabaseNotOpen);
  }
  sql::Database& db = db_assets_->db();
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return base::unexpected(Error::kFailedToStartTransaction);
  }

  for (auto row_id : shared_cache_row_ids) {
    sql::Statement statement(db.GetCachedStatement(
        SQL_FROM_HERE,
        GetSharedCacheIsolatedDatabaseQuery(
            SharedCacheIsolatedDatabaseQuery::kDeleteResourceByRowId)));
    statement.BindInt64(0, row_id.value());
    statement.Run();
  }

  if (!transaction.Commit()) {
    return base::unexpected(Error::kFailedToCommitTransaction);
  }

  // TODO(crbug.com/473666511): Autovacuum is disabled on Desktop, so we need
  // to run VACUUM manually at some point. However, running it here might
  // cause performance issues. Figure out a better timing/strategy to run it.

  return base::ok();
}

void SqlSharedCacheIsolatedDatabase::Cleanup() {
  if (db_assets_ && db_assets_->db().is_open()) {
    bool has_entries = false;
    {
      sql::Statement count_statement(db_assets_->db().GetCachedStatement(
          SQL_FROM_HERE,
          GetSharedCacheIsolatedDatabaseQuery(
              SharedCacheIsolatedDatabaseQuery::kSelectRowidLimit1)));
      has_entries = count_statement.Step();
    }
    if (!has_entries) {
      db_assets_->AbandonAndDeleteFiles();
    }
  }
  db_assets_.reset();
}

bool SqlSharedCacheIsolatedDatabase::HasRowForTesting(  // IN-TEST
    SqlSharedCacheRowId shared_cache_row_id) {
  if (!db_assets_ || !db_assets_->db().is_open()) {
    return false;
  }
  sql::Statement statement(db_assets_->db().GetCachedStatement(
      SQL_FROM_HERE,
      GetSharedCacheIsolatedDatabaseQuery(
          SharedCacheIsolatedDatabaseQuery::kSelectUrlAndReadyByRowId)));
  statement.BindInt64(0, shared_cache_row_id.value());
  return statement.Step();
}

}  // namespace disk_cache
