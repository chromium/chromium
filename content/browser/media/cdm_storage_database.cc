// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_database.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "sql/meta_table.h"
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

const char kDeleteForTimeFrameError[] = "DeleteForTimeFrameError.";
const char kDeleteForStorageKeyError[] = "DeleteForStorageKeyError.";
const char kDeleteFileError[] = "DeleteFileError.";

static bool DatabaseIsEmpty(sql::Database* db) {
  static constexpr char kSelectCountSql[] = "SELECT COUNT(*) FROM cdm_storage";

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
      db_(sql::DatabaseOptions().set_page_size(32768).set_cache_size(8),
          /*tag=*/"CdmStorage") {
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

  return statement.ColumnBlobAsVector(0);
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
  // TODO(crbug.com/577198003): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK(db_.IsSQLValid(kInsertSql));

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindString(0, storage_key.Serialize());
  statement.BindBlob(1, cdm_type.AsBytes());
  statement.BindString(2, file_name);
  statement.BindBlob(3, data);
  statement.BindInt64(4, data.size());
  statement.BindTime(5, base::Time::Now());

  bool success = statement.Run();

  DVLOG_IF(1, !success) << "Error writing Cdm storage data.";

  return success;
}

CdmStorageKeyUsageSize CdmStorageDatabase::GetUsagePerAllStorageKeys(
    const base::Time begin,
    const base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CdmStorageKeyUsageSize usage_per_storage_keys;

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return usage_per_storage_keys;
  }

  static constexpr char kSelectUsageSql[] =
      "SELECT storage_key, SUM(file_size) FROM cdm_storage "
      "WHERE last_modified >= ? "
      "AND last_modified <= ? "
      "GROUP BY storage_key ";

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectUsageSql));
  statement.BindTime(0, begin);
  statement.BindTime(1, end);

  while (statement.Step()) {
    std::optional<blink::StorageKey> maybe_storage_key =
        blink::StorageKey::Deserialize(statement.ColumnStringView(0));
    if (maybe_storage_key) {
      usage_per_storage_keys.emplace_back(maybe_storage_key.value(),
                                          statement.ColumnInt64(1));
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

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return false;
  }

  CdmStorageKeyUsageSize usage_per_storage_keys =
      GetUsagePerAllStorageKeys(begin, end);

  // clang-format off
  static constexpr char kDeleteSql[] =
      "DELETE FROM cdm_storage "
         "WHERE storage_key = ? "
         "AND last_modified >= ? "
         "AND last_modified <= ? ";
  // clang-format on

  bool success = true;
  {
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
    for (const auto& [storage_key, _] : usage_per_storage_keys) {
      if (storage_key_matcher.Run(storage_key)) {
        statement.Reset(/*clear_bound_vars=*/true);
        statement.BindString(0, storage_key.Serialize());
        statement.BindTime(1, begin);
        statement.BindTime(2, end);
        success = statement.Run() && success;
      }
    }
  }

  return DeleteIfEmptyDatabase(success);
}

bool CdmStorageDatabase::DeleteDataForStorageKey(
    const blink::StorageKey& storage_key,
    const base::Time begin,
    const base::Time end) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != CdmStorageOpenError::kOk) {
    return false;
  }

  // clang-format off
  static constexpr char kDeleteSql[] =
      "DELETE FROM cdm_storage "
         "WHERE storage_key = ? "
         "AND last_modified >= ? "
         "AND last_modified <= ? ";
  // clang-format on

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

  // clang-format off
  static constexpr char kDeleteSql[] =
      "DELETE FROM cdm_storage "
         "WHERE last_modified >= ? "
         "AND last_modified <= ? ";
  // clang-format on

  bool success;
  {
    sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
    statement.BindTime(0, begin);
    statement.BindTime(1, end);
    success = statement.Run();
  }

  DVLOG_IF(1, !success)
      << "Error deleting Cdm storage data for specified time frame.";

  base::UmaHistogramBoolean(
      GetCdmStorageManagerHistogramName(kDeleteForTimeFrameError, in_memory()),
      !success);

  return DeleteIfEmptyDatabase(success);
}

bool CdmStorageDatabase::ClearDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
  sql::Statement statement_count(
      db_.GetCachedStatement(SQL_FROM_HERE, kPageCountSql));
  statement_count.Step();

  uint64_t page_count = statement_count.ColumnInt(0);

  static constexpr char kPageSizeSql[] = "PRAGMA page_size";
  sql::Statement statement_size(
      db_.GetCachedStatement(SQL_FROM_HERE, kPageSizeSql));
  statement_size.Step();

  uint64_t page_size = statement_size.ColumnInt(0);

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

  if (!db_.Execute(kCreateTableSql)) {
    DVLOG(1) << "Failed to execute " << kCreateTableSql;
    return CdmStorageOpenError::kSQLExecutionError;
  }

  return CdmStorageOpenError::kOk;
}

bool CdmStorageDatabase::UpgradeDatabaseSchema(sql::MetaTable* meta_table) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Previously in UpgradeDatabaseSchema, we were setting the version number for
  // the meta table, but not the compatible version number, which should have
  // been updated as well. This caused a crash, since UpgradeDatabaseSchema
  // would be called all the time since we compare meta_table's compatible
  // version number to kVersionNumber. This fixes this change by setting it
  // correctly in the cases where this was incorrectly set.
  if (meta_table->GetCompatibleVersionNumber() == 1 &&
      meta_table->GetVersionNumber() == 2) {
    return meta_table->SetCompatibleVersionNumber(2);
  }

  // Histogram to track when incompatible version schema detected.
  base::UmaHistogramBoolean(
      "Media.EME.CdmStorageDatabase.IncompatibleDatabaseDetected", true);

  static constexpr char kAlterFileSizeSql[] =
      "ALTER TABLE cdm_storage ADD COLUMN file_size INTEGER NOT NULL DEFAULT "
      "1";

  sql::Statement file_size_statement(db_.GetUniqueStatement(kAlterFileSizeSql));

  if (!file_size_statement.Run()) {
    return false;
  }

  const std::string alter_last_modified_string = base::StrCat(
      {"ALTER TABLE cdm_storage ADD COLUMN last_modified INTEGER NOT NULL "
       "DEFAULT ",
       base::NumberToString(
           sql::Statement::TimeToSqlValue(base::Time::Now()))});

  sql::Statement last_modified_statement(
      db_.GetUniqueStatement(alter_last_modified_string));

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
}

}  // namespace content
