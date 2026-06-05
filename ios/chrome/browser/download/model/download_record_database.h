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
#import "sql/database.h"
#import "sql/init_status.h"

namespace sql {
class MetaTable;
class Statement;
}  // namespace sql

// Manages download records storage using SQLite database.
// This class is NOT thread-safe and must be used from a single sequence.
class DownloadRecordDatabase {
 public:
  // Query parameters for keyset-based pagination of download records.
  // Records are returned ordered by (created_time DESC, download_id DESC).
  // When `cursor_created_time` and `cursor_download_id` are both set, only
  // records strictly less than that tuple (in the same ordering) are returned,
  // enabling stable continuation across pages even when new rows are inserted
  // between calls.
  struct DownloadRecordQuery {
    DownloadRecordQuery();
    DownloadRecordQuery(const DownloadRecordQuery& other);
    DownloadRecordQuery& operator=(const DownloadRecordQuery& other);
    ~DownloadRecordQuery();

    // Optional filter by file category (PDF/Image/Video/...). When unset or
    // kAll, all categories are returned.
    std::optional<DownloadFilterType> filter_type;
    // Pagination cursor: created_time of the last row from the previous page.
    std::optional<base::Time> cursor_created_time;
    // Pagination cursor: download_id of the last row from the previous page.
    std::optional<std::string> cursor_download_id;
    // Optional case-insensitive substring filter on the file name. Matched
    // against the normalized (case-folded) file_name column using SQL LIKE,
    // so e.g. "Port" matches "report.pdf".
    std::optional<std::string> name_query;
  };

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

  // Updates the state of multiple download records in a single transaction.
  bool UpdateDownloadRecordsState(const std::vector<std::string>& download_ids,
                                  web::DownloadTask::State new_state);

  // Deletes a download record by its ID.
  bool DeleteDownloadRecord(const std::string& download_id);

  // Retrieves a single download record by its ID.
  std::optional<DownloadRecord> GetDownloadRecord(
      const std::string& download_id);

  // Retrieves all download records ordered by creation time (newest first).
  std::vector<DownloadRecord> GetAllDownloadRecords();

  // Retrieves one page of download records using keyset pagination.
  // Results are ordered by (created_time DESC, download_id DESC) and contain
  // at most `query.limit` rows. Pass the (created_time, download_id) of the
  // last row from the previous page in `cursor_*` to continue.
  std::vector<DownloadRecord> GetDownloadRecordsPage(
      const DownloadRecordQuery& query);

  // Returns the total number of records matching the (non-cursor) filter
  // portion of `query`. Cursor fields are ignored - this returns the total
  // count across all pages.
  int GetDownloadRecordsCount(const DownloadRecordQuery& query);

  // Single SQL UPDATE that flips any record currently in kInProgress or
  // kNotStarted to kFailed. Other states are left untouched. Intended to be
  // called once at startup to clean up downloads that were interrupted by app
  // termination.
  bool MarkUnfinishedDownloadsAsFailed();

  // Checks if the database is properly initialized and ready for operations.
  bool IsInitialized() const;

 private:
  // Creates the database schema tables.
  bool CreateSchema();

  // Upgrades the database to the current version.
  bool UpgradeDatabase();

  // Checks if a table exists in the database.
  bool DoesTableExist(const std::string& table_name);

  // Binds record data to an INSERT statement.
  void BindRecordToInsertStatement(sql::Statement& statement,
                                   const DownloadRecord& record);

  // Binds record data to an UPDATE statement.
  void BindRecordToUpdateStatement(sql::Statement& statement,
                                   const DownloadRecord& record);

  // Creates a DownloadRecord from a SQL statement result.
  DownloadRecord CreateRecordFromStatement(sql::Statement& statement);

  // Handles database errors.
  void DatabaseErrorCallback(int error, sql::Statement* stmt);

  // Path to the database file.
  const base::FilePath db_path_;

  // SQLite database connection.
  sql::Database db_;

  // Manages database versioning metadata.
  std::unique_ptr<sql::MetaTable> meta_table_;

  // Stores the initialization status from the last Init() call.
  sql::InitStatus init_status_ = sql::INIT_FAILURE;

  // Ensures single-threaded access.
  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_DOWNLOAD_RECORD_DATABASE_H_
