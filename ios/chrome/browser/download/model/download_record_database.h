// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_DATABASE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_DATABASE_H_

#import <memory>
#import <optional>
#import <string>
#import <vector>

#import "base/files/file_path.h"
#import "base/sequence_checker.h"
#import "base/time/time.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/chrome/browser/download/model/download_record.h"
#import "ios/chrome/browser/download/model/download_record_query.h"
#import "sql/database.h"
#import "sql/init_status.h"

namespace sql {
class MetaTable;
class Statement;
}  // namespace sql

// Manages download records storage using a SQLite database. Not thread-safe;
// must be used from a single sequence.
class DownloadRecordDatabase {
 public:
  explicit DownloadRecordDatabase(const base::FilePath& db_path);

  DownloadRecordDatabase(const DownloadRecordDatabase&) = delete;
  DownloadRecordDatabase& operator=(const DownloadRecordDatabase&) = delete;

  ~DownloadRecordDatabase();

  // Initializes the database connection and schema.
  sql::InitStatus Init();

  // Inserts a new download record into the database.
  bool InsertDownloadRecord(const DownloadRecord& record);

  // Updates an existing download record in the database.
  bool UpdateDownloadRecord(const DownloadRecord& record);

  // Updates state for multiple records in a single transaction.
  bool UpdateDownloadRecordsState(const std::vector<std::string>& download_ids,
                                  web::DownloadTask::State new_state);

  // Deletes a record by ID.
  bool DeleteDownloadRecord(const std::string& download_id);

  // Retrieves a single record by ID.
  std::optional<DownloadRecord> GetDownloadRecord(
      const std::string& download_id);

  // Retrieves all records ordered by `created_time` (newest first).
  std::vector<DownloadRecord> GetAllDownloadRecords();

  // Retrieves one page of records using keyset pagination. Results are
  // ordered by `(created_time DESC, download_id DESC)` and contain at most
  // `kDownloadRecordsPageSize` rows (defined in `download_record_query.h`;
  // not overridable via `query`). Pass the `(created_time, download_id)` of
  // the last row from the previous page in `cursor_*` to continue.
  std::vector<DownloadRecord> GetDownloadRecordsPage(
      const DownloadRecordQuery& query);

  // Returns the total number of records matching the non-cursor filter
  // portion of `query`. Cursor fields are ignored — this returns the total
  // across all pages.
  int GetDownloadRecordsCount(const DownloadRecordQuery& query);

  // Single SQL `UPDATE` that flips any record in `kInProgress` or
  // `kNotStarted` to `kFailed`. Intended for one-shot startup cleanup of
  // downloads interrupted by app termination.
  bool MarkUnfinishedDownloadsAsFailed();

  // True once the schema is fully initialized.
  bool IsInitialized() const;

 private:
  // Creates the database schema.
  bool CreateSchema();

  // Upgrades the schema to the current version.
  bool UpgradeDatabase();

  bool DoesTableExist(const std::string& table_name);

  void BindRecordToInsertStatement(sql::Statement& statement,
                                   const DownloadRecord& record);

  void BindRecordToUpdateStatement(sql::Statement& statement,
                                   const DownloadRecord& record);

  // Builds a `DownloadRecord` from a row in `statement`.
  DownloadRecord CreateRecordFromStatement(sql::Statement& statement);

  void DatabaseErrorCallback(int error, sql::Statement* stmt);

  const base::FilePath db_path_;

  sql::Database db_;

  // Tracks schema version.
  std::unique_ptr<sql::MetaTable> meta_table_;

  // Result of the last `Init()`.
  sql::InitStatus init_status_ = sql::INIT_FAILURE;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_DATABASE_H_
