// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_database.h"

#include <algorithm>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "sql/statement.h"

namespace content {

namespace {

// This is the version number of the CdmStorageDatabase. We are currently on
// version two since there has been one change to the schema from when it is
// created. Please increment `kVersionNumber` by 1 every time you change the
// schema, and update the cdm_storage_database_unittest.cc code to keep version
// 2's schema to test the transition between v2 and v3, the same way we are
// testing v1 to v2.
// Change to version 2:
// We have introduced file_size and last_modified to the version Schema, to keep
// track of file size and when a file has been touched, whether it be written or
// read.
static const int kVersionNumber = 2;

const char kUmaPrefix[] = "Media.EME.CdmStorageDatabaseSQLiteError.";

const char kDeleteForTimeFrameError[] = "DeleteForTimeFrameError.";
const char kDeleteForStorageKeyError[] = "DeleteForStorageKeyError.";
const char kDeleteForFilterError[] = "DeleteForFilterError.";
const char kDeleteFileError[] = "DeleteFileError.";

static bool DatabaseIsEmpty(sql::Database* db) {
  static constexpr char kSelectCountSql[] = "SELECT COUNT(*) FROM cdm_storage";
  DCHECK(db->IsSQLValid(kSelectCountSql));

  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kSelectCountSql));
  return (statement.Step() && !statement.ColumnInt(0));
}

}  // namespace

CdmStorageDatabase::CdmStorageDatabase(const base::FilePath& path)
    : path_(path),
      // Use a smaller cache, since access will be fairly infrequent and random.
      // Given the expected record sizes (~100s of bytes) and key sizes (<100
      // bytes) and that we'll typically only be pulling one file at a time
      // (playback), specify a large page size to allow inner nodes can pack
      // many keys, to keep the index B-tree flat.
      db_(sql::DatabaseOptions{.page_size = 32768, .cache_size = 8}) {
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

std::optional<std::vector<uint8_t>> CdmStorageDatabase::ReadFile(
    const blink::StorageKey& storage_key,
    const media::CdmType& cdm_type,
    const std::string& file_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return std::nullopt;
  }

  // clang-format off
  static constexpr char kSelectSql[] =
      "SELECT data FROM cdm_storage "
          "WHERE storage_key = ? "
            "AND cdm_type = ? "
            "AND file_name = ? ";
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
    return std::nullopt;
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

  // clang-format off
  static constexpr char kInsertSql[] =
      "INSERT OR REPLACE INTO "
         "cdm_storage(storage_key,cdm_type,file_name,data,file_size,last_modified) "
         "VALUES(?,?,?,?,?,?) ";
  // clang-format on
  DCHECK(db_.IsSQLValid(kInsertSql));

  last_operation_ = "WriteFile";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindBlob(1, cdm_type.AsBytes());
  statement.BindString(2, file_name);
  statement.BindBlob(3, data);
  statement.BindInt64(4, data.size());
  statement.BindTime(5, base::Time::Now());

  bool success = statement.Run();

  DVLOG_IF(1, !success) << "Error writing Cdm storage data.";

  if (success) {
    bool large_write = data.size() > (15 * 1024);
    base::UmaHistogramBoolean(
        "Media.EME.CdmStorageDatabase.WriteFileForBigData", large_write);
  }

  return success;
}

std::optional<uint64_t> CdmStorageDatabase::GetSizeForFile(
    const blink::StorageKey& storage_key,
    const media::CdmType& cdm_type,
    const std::string& file_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return std::nullopt;
  }

  // clang-format off
  static constexpr char kSelectSql[] =
      "SELECT file_size FROM cdm_storage "
          "WHERE storage_key = ? "
            "AND cdm_type = ? "
            "AND file_name = ? ";
  // clang-format on
  DCHECK(db_.IsSQLValid(kSelectSql));

  last_operation_ = "GetSizeForFile";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindBlob(1, cdm_type.AsBytes());
  statement.BindString(2, file_name);

  if (!statement.Step()) {
    // Failing here is expected if the "file" has not yet been written to and
    // the row does not yet exist. The Cdm Storage code doesn't distinguish
    // between an empty file and a file which does not exist, so just return
    // an empty file size without erroring.
    return 0;
  }

  return statement.ColumnInt64(0);
}

std::optional<uint64_t> CdmStorageDatabase::GetSizeForStorageKey(
    const blink::StorageKey& storage_key,
    const base::Time begin,
    const base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return std::nullopt;
  }

  // clang-format off
  static constexpr char kSelectSql[] =
      "SELECT SUM(file_size) FROM cdm_storage "
         "WHERE storage_key = ? "
         "AND last_modified >= ? "
         "AND last_modified <= ? ";
  // clang-format on

  DCHECK(db_.IsSQLValid(kSelectSql));

  last_operation_ = "GetSizeForStorageKey";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindTime(1, begin);
  statement.BindTime(2, end);

  if (!statement.Step()) {
    // Failing here is expected if the "files" are not found in the
    // time frame for the storage key. The Cdm Storage code doesn't distinguish
    // between an empty file and a file which does not exist, so just this
    // returns the Sum() of the file sizes, which are all empty, without
    // erroring.
    return 0;
  }

  return statement.ColumnInt64(0);
}

std::optional<uint64_t> CdmStorageDatabase::GetSizeForTimeFrame(
    const base::Time begin,
    const base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return std::nullopt;
  }

  // clang-format off
  static constexpr char kSelectSql[] =
      "SELECT SUM(file_size) FROM cdm_storage "
         "WHERE last_modified >= ? "
         "AND last_modified <= ? ";
  // clang-format on
  DCHECK(db_.IsSQLValid(kSelectSql));

  last_operation_ = "GetSizeForTimeFrame";

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindTime(0, begin);
  statement.BindTime(1, end);

  if (!statement.Step()) {
    // Failing here is expected if the "files" are not found in the
    // time frame. The Cdm Storage code doesn't distinguish between an
    // empty file and a file which does not exist, so just this returns the
    // Sum() of the file sizes, which are all empty, without erroring.
    return 0;
  }

  return statement.ColumnInt64(0);
}

CdmStorageKeyUsageSize CdmStorageDatabase::GetUsagePerAllStorageKeys(
    const base::Time begin,
    const base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CdmStorageKeyUsageSize usage_per_storage_keys;

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return usage_per_storage_keys;
  }

  static constexpr char kSelectStorageKeySql[] =
      "SELECT DISTINCT storage_key FROM cdm_storage "
      "WHERE last_modified >= ? "
      "AND last_modified <= ? ";

  sql::Statement get_all_storage_keys_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectStorageKeySql));
  get_all_storage_keys_statement.BindTime(0, begin);
  get_all_storage_keys_statement.BindTime(1, end);

  while (get_all_storage_keys_statement.Step()) {
    std::optional<blink::StorageKey> maybe_storage_key =
        blink::StorageKey::Deserialize(
            get_all_storage_keys_statement.ColumnString(0));
    if (maybe_storage_key) {
      auto storage_key = maybe_storage_key.value();
      usage_per_storage_keys.emplace_back(
          storage_key, GetSizeForStorageKey(storage_key).value_or(0));
    }
  }

  return usage_per_storage_keys;
}

bool CdmStorageDatabase::DeleteFile(const blink::StorageKey& storage_key,
                                    const media::CdmType& cdm_type,
                                    const std::string& file_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return false;
  }

  // clang-format off
  static constexpr char kDeleteSql[] =
      "DELETE FROM cdm_storage "
         "WHERE storage_key = ? "
         "AND cdm_type = ? "
         "AND file_name = ? ";
  // clang-format on
  DCHECK(db_.IsSQLValid(kDeleteSql));

  last_operation_ = "DeleteFile";

  bool success;
  {
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
    statement.BindString(0, storage_key.Serialize());
    statement.BindBlob(1, cdm_type.AsBytes());
    statement.BindString(2, file_name);
    success = statement.Run();
  }
  DVLOG_IF(1, !success) << "Error deleting Cdm storage data.";

  base::UmaHistogramBoolean(
      GetCdmStorageManagerHistogramName(kDeleteFileError, in_memory()),
      !success);

  return DeleteIfEmptyDatabase(success);
}

bool CdmStorageDatabase::DeleteData(
    const StoragePartition::StorageKeyMatcherFunction& storage_key_matcher,
    const blink::StorageKey& storage_key,
    const base::Time begin,
    const base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!storage_key_matcher.is_null()) {
    return DeleteDataForFilter(storage_key_matcher, begin, end);
  } else if (!storage_key.origin().opaque()) {
    return DeleteDataForStorageKey(storage_key, begin, end);
  }
  return DeleteDataForTimeFrame(begin, end);
}

bool CdmStorageDatabase::DeleteDataForFilter(
    StoragePartition::StorageKeyMatcherFunction storage_key_matcher,
    const base::Time begin,
    const base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CdmStorageKeyUsageSize usage_per_storage_keys =
      GetUsagePerAllStorageKeys(begin, end);

  for (auto [storage_key, _] : usage_per_storage_keys) {
    if (storage_key_matcher.Run(storage_key)) {
      DeleteDataForStorageKey(storage_key, begin, end);
    }
  }

  base::UmaHistogramBoolean(
      GetCdmStorageManagerHistogramName(kDeleteForFilterError, in_memory()),
      false);

  return DeleteIfEmptyDatabase(true);
}

bool CdmStorageDatabase::DeleteDataForStorageKey(
    const blink::StorageKey& storage_key,
    const base::Time begin,
    const base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return false;
  }

  last_operation_ = "DeleteForStorageKey";

  // clang-format off
  static constexpr char kDeleteSql[] =
      "DELETE FROM cdm_storage "
         "WHERE storage_key = ? "
         "AND last_modified >= ? "
         "AND last_modified <= ? ";
  // clang-format on
  DCHECK(db_.IsSQLValid(kDeleteSql));

  bool success;
  {
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
    statement.BindString(0, storage_key.Serialize());
    statement.BindTime(1, begin);
    statement.BindTime(2, end);
    success = statement.Run();
  }

  DVLOG_IF(1, !success) << "Error deleting Cdm storage data.";

  base::UmaHistogramBoolean(
      GetCdmStorageManagerHistogramName(kDeleteForStorageKeyError, in_memory()),
      !success);

  return DeleteIfEmptyDatabase(success);
}

bool CdmStorageDatabase::DeleteDataForTimeFrame(const base::Time begin,
                                                const base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return false;
  }

  last_operation_ = "DeleteForTimeFrame";

  // clang-format off
  static constexpr char kDeleteSql[] =
      "DELETE FROM cdm_storage "
         "WHERE last_modified >= ? "
         "AND last_modified <= ? ";
  // clang-format on
  DCHECK(db_.IsSQLValid(kDeleteSql));

  bool success;
  {
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
    statement.BindTime(0, begin);
    statement.BindTime(1, end);
    success = statement.Run();
  }

  DVLOG_IF(1, !success)
      << "Error deleting Cdm storage data for specified time frame.";

  base::UmaHistogramBoolean(GetCdmStorageManagerHistogramName(
                                kDeleteForTimeFrameError, path_.empty()),
                            !success);

  return DeleteIfEmptyDatabase(success);
}

bool CdmStorageDatabase::ClearDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  last_operation_ = "ClearDatabase";

  db_.Close();

  if (in_memory()) {
    // Memory associated with an in-memory database will be released when the
    // database is closed above.
    return true;
  }

  return sql::Database::Delete(path_);
}

uint64_t CdmStorageDatabase::GetDatabaseSize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kPageCountSql[] = "PRAGMA page_count";
  DCHECK(db_.IsSQLValid(kPageCountSql));

  last_operation_ = "QueryPageCount";

  sql::Statement statement_count(
      db_.GetCachedStatement(SQL_FROM_HERE, kPageCountSql));
  statement_count.Step();

  uint64_t page_count = statement_count.ColumnInt(0);

  static constexpr char kPageSizeSql[] = "PRAGMA page_size";
  DCHECK(db_.IsSQLValid(kPageSizeSql));

  last_operation_ = "QueryPageSize";

  sql::Statement statement_size(
      db_.GetCachedStatement(SQL_FROM_HERE, kPageSizeSql));
  statement_size.Step();

  uint64_t page_size = statement_size.ColumnInt(0);

  last_operation_.reset();

  return page_count * page_size;
}

void CdmStorageDatabase::CloseDatabaseForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.Close();
}

bool CdmStorageDatabase::DeleteIfEmptyDatabase(bool last_operation_success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!last_operation_success || OpenDatabase() != CdmStorageOpenError::kOk) {
    return false;
  }

  last_operation_ = "DeleteIfEmptyDatabase";

  if (!DatabaseIsEmpty(&db_)) {
    return true;
  }

  return ClearDatabase();
}

CdmStorageOpenError CdmStorageDatabase::OpenDatabase(bool is_retry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.is_open()) {
    return CdmStorageOpenError::kOk;
  }

  bool success = false;

  last_operation_ = "OpenDatabase";

  if (in_memory()) {
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

  // TODO(crbug.com/40272342): Remove once histogram shows that there are no
  // more incompatible databases. This scenario happens when the database has
  // the v1 schema without the 'file_size' and 'last_modified' columns.
  if (meta_table.GetCompatibleVersionNumber() < kVersionNumber) {
    return (!UpgradeDatabaseSchema(&meta_table) || is_retry)
               ? CdmStorageOpenError::kAlterTableError
               : OpenDatabase(/*is_retry=*/true);
  }

  if (meta_table.GetCompatibleVersionNumber() > kVersionNumber) {
    // This should only happen if the user downgrades the version. If that
    // results in an incompatible schema, we need to wipe the database and start
    // over.
    DVLOG(1) << "Cdm Storage database is too new, kVersionNumber"
             << kVersionNumber << ", GetCompatibleVersionNumber="
             << meta_table.GetCompatibleVersionNumber();
    // TODO(crbug.com/40272342) Add UMA to report if incompatible database
    // version occurs.
    db_.Raze();
    return is_retry ? CdmStorageOpenError::kDatabaseRazeError
                    : OpenDatabase(/*is_retry=*/true);
  }

  // Set up the table.
  // clang-format off
  static constexpr char kCreateTableSql[] =
      "CREATE TABLE IF NOT EXISTS cdm_storage("
          "storage_key TEXT NOT NULL,"
          "cdm_type BLOB NOT NULL,"
          "file_name TEXT NOT NULL,"
          "data BLOB NOT NULL,"
          "file_size INTEGER NOT NULL,"
          "last_modified INTEGER NOT NULL,"
          "PRIMARY KEY(storage_key,cdm_type,file_name))";
  // clang-format on
  DCHECK(db_.IsSQLValid(kCreateTableSql));

  if (!db_.Execute(kCreateTableSql)) {
    DVLOG(1) << "Failed to execute " << kCreateTableSql;
    return CdmStorageOpenError::kSQLExecutionError;
  }

  return CdmStorageOpenError::kOk;
}

bool CdmStorageDatabase::UpgradeDatabaseSchema(sql::MetaTable* meta_table) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Histogram to track when incompatible version schema detected.

  // Previously in UpgradeDatabaseSchema, we were setting the version number for
  // the meta table, but not the compatible version number, which should have
  // been updated as well. This caused a crash, since UpgradeDatabaseSchema
  // would be called all the time since we compare meta_table's compatible
  // version number to kVersionNumber. This fixes this change by setting it
  // correctly in the cases where this was incorrectly set.
  // TODO(crbug.com/40272342): Remove in M123.
  if (meta_table->GetCompatibleVersionNumber() == 1 &&
      meta_table->GetVersionNumber() == 2) {
    return meta_table->SetCompatibleVersionNumber(2);
  }

  base::UmaHistogramBoolean(
      "Media.EME.CdmStorageDatabase.IncompatibleDatabaseDetected", true);

  static constexpr char kAlterFileSizeSql[] =
      "ALTER TABLE cdm_storage ADD COLUMN file_size INTEGER NOT NULL DEFAULT "
      "1";
  DCHECK(db_.IsSQLValid(kAlterFileSizeSql));

  last_operation_ = "AlterDatabaseForFileSize";

  sql::Statement file_size_statement(db_.GetUniqueStatement(kAlterFileSizeSql));

  if (!file_size_statement.Run()) {
    return false;
  }

  const std::string alter_last_modified_string = base::StrCat(
      {"ALTER TABLE cdm_storage ADD COLUMN last_modified INTEGER NOT NULL "
       "DEFAULT ",
       base::NumberToString(
           sql::Statement::TimeToSqlValue(base::Time::Now()))});
  DCHECK(db_.IsSQLValid(alter_last_modified_string));

  sql::Statement last_modified_statement(
      db_.GetUniqueStatement(alter_last_modified_string));

  last_operation_ = "AlterDatabaseForLastModified";

  if (!last_modified_statement.Run()) {
    return false;
  }

  return meta_table->SetVersionNumber(kVersionNumber) &&
         meta_table->SetCompatibleVersionNumber(kVersionNumber);
}

void CdmStorageDatabase::OnDatabaseError(int error, sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::UmaHistogramSqliteResult("Media.EME.CdmStorageDatabaseSQLiteError",
                                error);

  if (last_operation_) {
    sql::UmaHistogramSqliteResult(kUmaPrefix + *last_operation_, error);

    switch (sql::ToSqliteLoggedResultCode(error)) {
      case sql::SqliteLoggedResultCode::kCantOpen:
        base::UmaHistogramSparse(
            base::StrCat({kUmaPrefix, *last_operation_, ".CantOpen.Errno"}),
            db_.GetLastErrno());
        break;
      case sql::SqliteLoggedResultCode::kFullDisk:
        base::UmaHistogramSparse(
            base::StrCat({kUmaPrefix, *last_operation_, ".FullDisk.Errno"}),
            db_.GetLastErrno());
        break;
      case sql::SqliteLoggedResultCode::kGeneric:
        base::UmaHistogramSparse(
            base::StrCat({kUmaPrefix, *last_operation_, ".Generic.Errno"}),
            db_.GetLastErrno());
        break;
      case sql::SqliteLoggedResultCode::kIoTruncate:
        base::UmaHistogramSparse(
            base::StrCat({kUmaPrefix, *last_operation_, ".IoTruncate.Errno"}),
            db_.GetLastErrno());
        break;
      case sql::SqliteLoggedResultCode::kBusy:
        base::UmaHistogramSparse(
            base::StrCat({kUmaPrefix, *last_operation_, ".Busy.Errno"}),
            db_.GetLastErrno());
        break;
      default:
        // Currently, we don't care what happens with other SqliteErrors, so
        // just break.
        break;
    }

    last_operation_.reset();
  }
}

}  // namespace content
