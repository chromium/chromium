// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_license_database.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"

namespace content {

using MediaLicenseStorageHostOpenError =
    MediaLicenseStorageHost::MediaLicenseStorageHostOpenError;

namespace {

static const int kVersionNumber = 1;

}  // namespace

MediaLicenseDatabase::MediaLicenseDatabase(const base::FilePath& path)
    : path_(path),
      // Use a smaller cache, since access will be fairly infrequent and random.
      // Given the expected record sizes (~100s of bytes) and key sizes (<100
      // bytes) and that we'll typically only be pulling one file at a time
      // (playback), specify a large page size to allow inner nodes can pack
      // many keys, to keep the index B-tree flat.
      db_(sql::DatabaseOptions{.exclusive_locking = true,
                               .page_size = 32768,
                               .cache_size = 8}) {}

MediaLicenseStorageHostOpenError MediaLicenseDatabase::OpenFile(
    const media::CdmType& cdm_type,
    const std::string& file_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The media license code doesn't distinguish between an empty file and a
  // file which does not exist, so don't bother inserting an empty row into
  // the database.
  return OpenDatabase();
}

absl::optional<std::vector<uint8_t>> MediaLicenseDatabase::ReadFile(
    const media::CdmType& cdm_type,
    const std::string& file_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != MediaLicenseStorageHostOpenError::kOk) {
    return absl::nullopt;
  }

  static constexpr char kSelectSql[] =
      "SELECT data FROM licenses WHERE cdm_type=? AND file_name=?";
  DCHECK(db_.IsSQLValid(kSelectSql));

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, cdm_type.ToString());
  statement.BindString(1, file_name);

  if (!statement.Step()) {
    // Failing here is expected if the "file" has not yet been written to and
    // the row does not yet exist. The media license code doesn't distinguish
    // between an empty file and a file which does not exist, so just return
    // an empty file without erroring.
    return std::vector<uint8_t>();
  }

  std::vector<uint8_t> data;
  if (!statement.ColumnBlobAsVector(0, &data)) {
    DVLOG(1) << "Error reading media license data.";
    return absl::nullopt;
  }

  return data;
}

bool MediaLicenseDatabase::WriteFile(const media::CdmType& cdm_type,
                                     const std::string& file_name,
                                     const std::vector<uint8_t>& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != MediaLicenseStorageHostOpenError::kOk) {
    return false;
  }

  static constexpr char kInsertSql[] =
      // clang-format off
      "INSERT OR REPLACE INTO licenses(cdm_type,file_name,data) "
          "VALUES(?,?,?)";
  // clang-format on
  DCHECK(db_.IsSQLValid(kInsertSql));

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindString(0, cdm_type.ToString());
  statement.BindString(1, file_name);
  statement.BindBlob(2, data);
  bool success = statement.Run();

  if (!success)
    DVLOG(1) << "Error writing media license data.";

  return success;
}

bool MediaLicenseDatabase::DeleteFile(const media::CdmType& cdm_type,
                                      const std::string& file_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (OpenDatabase() != MediaLicenseStorageHostOpenError::kOk) {
    return false;
  }

  static constexpr char kDeleteSql[] =
      "DELETE FROM licenses WHERE cdm_type=? AND file_name=?";
  DCHECK(db_.IsSQLValid(kDeleteSql));

  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  statement.BindString(0, cdm_type.ToString());
  statement.BindString(1, file_name);
  bool success = statement.Run();

  if (!success)
    DVLOG(1) << "Error writing media license data.";

  return success;
}

bool MediaLicenseDatabase::ClearDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.Close();

  if (path_.empty()) {
    // Memory associated with an in-memory database will be released when the
    // database is closed above.
    return true;
  }

  return sql::Database::Delete(path_);
}

// Opens and sets up a database if one is not already set up.
MediaLicenseStorageHostOpenError MediaLicenseDatabase::OpenDatabase(
    bool is_retry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (db_.is_open())
    return MediaLicenseStorageHostOpenError::kOk;

  bool success = false;

  // If this is not the first call to `OpenDatabase()` because we are re-trying
  // initialization, then the error callback will have previously been set.
  db_.reset_error_callback();

  // base::Unretained is safe becase |db_| is owned by |this|
  db_.set_error_callback(base::BindRepeating(
      &MediaLicenseDatabase::OnDatabaseError, base::Unretained(this)));

  if (path_.empty()) {
    success = db_.OpenInMemory();
  } else {
    // Ensure `path`'s parent directory exists.
    auto error = base::File::Error::FILE_OK;
    if (!base::CreateDirectoryAndGetError(path_.DirName(), &error)) {
      DVLOG(1) << "Failed to open CDM database: "
               << base::File::ErrorToString(error);
      base::UmaHistogramExactLinear(
          "Media.EME.MediaLicenseDatabaseCreateDirectoryError", -error,
          -base::File::FILE_ERROR_MAX);
      return MediaLicenseStorageHostOpenError::kBucketNotFound;
    }
    DCHECK_EQ(error, base::File::Error::FILE_OK);

    success = db_.Open(path_);
  }

  if (!success) {
    DVLOG(1) << "Failed to open CDM database: " << db_.GetErrorMessage();
    return MediaLicenseStorageHostOpenError::kDatabaseOpenError;
  }

  sql::MetaTable meta_table;
  if (!meta_table.Init(&db_, kVersionNumber, kVersionNumber)) {
    DVLOG(1) << "Could not initialize Media License database metadata table.";
    // Wipe the database and start over. If we've already wiped the database and
    // are still failing, just return false.
    db_.Raze();
    return is_retry ? MediaLicenseStorageHostOpenError::kDatabaseRazeError
                    : OpenDatabase(/*is_retry=*/true);
  }

  if (meta_table.GetCompatibleVersionNumber() > kVersionNumber) {
    // This should only happen if the user downgrades the Chrome channel (for
    // example, from Beta to Stable). If that results in an incompatible schema,
    // we need to wipe the database and start over.
    DVLOG(1) << "Media License database is too new, kVersionNumber"
             << kVersionNumber << ", GetCompatibleVersionNumber="
             << meta_table.GetCompatibleVersionNumber();
    db_.Raze();
    return is_retry ? MediaLicenseStorageHostOpenError::kDatabaseRazeError
                    : OpenDatabase(/*is_retry=*/true);
  }

  // Set up the table.
  static constexpr char kCreateTableSql[] =
      // clang-format off
      "CREATE TABLE IF NOT EXISTS licenses("
          "cdm_type TEXT NOT NULL,"
          "file_name TEXT NOT NULL,"
          "data BLOB NOT NULL,"
          "PRIMARY KEY(cdm_type,file_name))";
  // clang-format on
  DCHECK(db_.IsSQLValid(kCreateTableSql));

  if (!db_.Execute(kCreateTableSql)) {
    DVLOG(1) << "Failed to execute " << kCreateTableSql;
    return MediaLicenseStorageHostOpenError::kSQLExecutionError;
  }

  return MediaLicenseStorageHostOpenError::kOk;
}

void MediaLicenseDatabase::OnDatabaseError(int error, sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::UmaHistogramSqliteResult("Media.EME.MediaLicenseDatabaseSQLiteError",
                                error);
}

}  // namespace content
