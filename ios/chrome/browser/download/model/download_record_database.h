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
  bool BindRecordToInsertStatement(sql::Statement& statement,
                                   const DownloadRecord& record);

  // Binds record data to an UPDATE statement.
  bool BindRecordToUpdateStatement(sql::Statement& statement,
                                   const DownloadRecord& record);

  // Creates a DownloadRecord from a SQL statement result.
  std::optional<DownloadRecord> CreateRecordFromStatement(
      sql::Statement& statement);

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
