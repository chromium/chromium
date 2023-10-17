// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_database.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "sql/meta_table.h"
#include "sql/statement.h"

namespace content {

namespace {

// This is the version number of the CdmStorageDatabase. We are currently on
// version one since there have been no changes to the schema from when it is
// created. Please increment `kVersionNumber` by 1 every time you change the
// schema.
static const int kVersionNumber = 1;

const char kUmaPrefix[] = "Media.EME.CdmStorageDatabaseSQLiteError.";

}  // namespace

CdmStorageDatabase::CdmStorageDatabase(const base::FilePath& path)
    : path_(path),
      // Use a smaller cache, since access will be fairly infrequent and random.
      // Given the expected record sizes (~100s of bytes) and key sizes (<100
      // bytes) and that we'll typically only be pulling one file at a time
      // (playback), specify a large page size to allow inner nodes can pack
      // many keys, to keep the index B-tree flat.
      db_(sql::DatabaseOptions{.exclusive_locking = true,
                               .page_size = 32768,
                               .cache_size = 8}) {
  // base::Unretained is safe because `db_` is owned by `this`
  db_.set_error_callback(base::BindRepeating(
      &CdmStorageDatabase::OnDatabaseError, base::Unretained(this)));
}

CdmStorageDatabase::~CdmStorageDatabase() = default;

CdmStorageOpenError CdmStorageDatabase::EnsureOpen() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If in use or open successful, returns `CdmStorageOpenError::kOk`.
  return OpenDatabase();
}

absl::optional<std::vector<uint8_t>> CdmStorageDatabase::ReadFile(
    const blink::StorageKey& storage_key,
    const media::CdmType& cdm_type,
    const std::string& file_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return absl::nullopt;
  }

  static constexpr char kSelectSql[] =
      // clang-format off
      "SELECT data FROM cdm_storage "
          "WHERE storage_key=? "
            "AND cdm_type=? "
            "AND file_name=? ";
  // clang-format on
  DCHECK(db_.IsSQLValid(kSelectSql));

  last_operation_ = "ReadFile";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindBlob(1, cdm_type.AsBytes());
  statement.BindString(2, file_name);

  if (!statement.Step()) {
    // Failing here is expected if the "file" has not yet been written to and
    // the row does not yet exist. The Cdm Storage code doesn't distinguish
    // between an empty file and a file which does not exist, so just return
    // an empty file without erroring.
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> data;
  if (!statement.ColumnBlobAsVector(0, &data)) {
    DVLOG(1) << "Error reading Cdm storage data.";
    return absl::nullopt;
  }

  return data;
}

bool CdmStorageDatabase::WriteFile(const blink::StorageKey& storage_key,
                                   const media::CdmType& cdm_type,
                                   const std::string& file_name,
                                   const std::vector<uint8_t>& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return false;
  }

  static constexpr char kInsertSql[] =
      // clang-format off
      "INSERT OR REPLACE INTO cdm_storage(storage_key,cdm_type,file_name,data) "
          "VALUES(?,?,?,?)";
  // clang-format on
  DCHECK(db_.IsSQLValid(kInsertSql));

  last_operation_ = "WriteFile";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindBlob(1, cdm_type.AsBytes());
  statement.BindString(2, file_name);
  statement.BindBlob(3, data);
  bool success = statement.Run();

  DVLOG_IF(1, !success) << "Error writing Cdm storage data.";

  return success;
}

bool CdmStorageDatabase::DeleteFile(const blink::StorageKey& storage_key,
                                    const media::CdmType& cdm_type,
                                    const std::string& file_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return false;
  }

  static constexpr char kDeleteSql[] =
      // clang-format off
      "DELETE FROM cdm_storage "
        "WHERE storage_key=? "
          "AND cdm_type=? "
          "AND file_name=? ";
  // clang-format on
  DCHECK(db_.IsSQLValid(kDeleteSql));

  last_operation_ = "DeleteFile";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindBlob(1, cdm_type.AsBytes());
  statement.BindString(2, file_name);
  bool success = statement.Run();

  DVLOG_IF(1, !success) << "Error deleting Cdm storage data.";

  return success;
}

bool CdmStorageDatabase::DeleteDataForStorageKey(
    const blink::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return false;
  }

  static constexpr char kDeleteSql[] =
      "DELETE FROM cdm_storage WHERE storage_key=?";
  DCHECK(db_.IsSQLValid(kDeleteSql));

  last_operation_ = "DeleteForStorageKey";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  statement.BindString(0, storage_key.Serialize());
  bool success = statement.Run();

  DVLOG_IF(1, !success) << "Error deleting Cdm storage data.";

  return success;
}

bool CdmStorageDatabase::ClearDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  last_operation_ = "ClearDatabase";

  db_.Close();

  if (path_.empty()) {
    // Memory associated with an in-memory database will be released when the
    // database is closed above.
    return true;
  }

  return sql::Database::Delete(path_);
}

CdmStorageOpenError CdmStorageDatabase::OpenDatabase(bool is_retry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.is_open()) {
    return CdmStorageOpenError::kOk;
  }

  bool success = false;

  last_operation_ = "OpenDatabase";

  if (path_.empty()) {
    success = db_.OpenInMemory();
  } else {
    success = db_.Open(path_);
  }

  if (!success) {
    DVLOG(1) << "Failed to open CDM database: " << db_.GetErrorMessage();
    return CdmStorageOpenError::kDatabaseOpenError;
  }

  sql::MetaTable meta_table;
  if (!meta_table.Init(&db_, kVersionNumber, kVersionNumber)) {
    DVLOG(1) << "Could not initialize Cdm Storage database metadata table.";
    // Wipe the database and start over. If we've already wiped the database
    // and are still failing, just return false.
    db_.Raze();
    return is_retry ? CdmStorageOpenError::kDatabaseRazeError
                    : OpenDatabase(/*is_retry=*/true);
  }

  if (meta_table.GetCompatibleVersionNumber() > kVersionNumber) {
    // This should only happen if the user downgrades the version. If that
    // results in an incompatible schema, we need to wipe the database and start
    // over.
    DVLOG(1) << "Cdm Storage database is too new, kVersionNumber"
             << kVersionNumber << ", GetCompatibleVersionNumber="
             << meta_table.GetCompatibleVersionNumber();
    // TODO(crbug.com/1454512) Add UMA to report if incompatible database
    // version occurs.
    db_.Raze();
    return is_retry ? CdmStorageOpenError::kDatabaseRazeError
                    : OpenDatabase(/*is_retry=*/true);
  }

  // Set up the table.
  static constexpr char kCreateTableSql[] =
      // clang-format off
      "CREATE TABLE IF NOT EXISTS cdm_storage("
          "storage_key TEXT NOT NULL,"
          "cdm_type BLOB NOT NULL,"
          "file_name TEXT NOT NULL,"
          "data BLOB NOT NULL,"
          "PRIMARY KEY(storage_key,cdm_type,file_name))";
  // clang-format on
  DCHECK(db_.IsSQLValid(kCreateTableSql));

  if (!db_.Execute(kCreateTableSql)) {
    DVLOG(1) << "Failed to execute " << kCreateTableSql;
    return CdmStorageOpenError::kSQLExecutionError;
  }

  return CdmStorageOpenError::kOk;
}

void CdmStorageDatabase::OnDatabaseError(int error, sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::UmaHistogramSqliteResult("Media.EME.CdmStorageDatabaseSQLiteError",
                                error);

  if (last_operation_) {
    sql::UmaHistogramSqliteResult(kUmaPrefix + *last_operation_, error);
    last_operation_.reset();
  }
}

}  // namespace content
