// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_RECOVERY_H_
#define SQL_RECOVERY_H_

#include <stddef.h>

#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "sql/database.h"
#include "sql/internal_api_token.h"
#include "sql/sqlite_result_code_values.h"

namespace base {
class FilePath;
}

namespace sql {

// Recovery module for sql/. Please see the `RecoverIfPossible()` method for how
// to use this class.
//
// This module is capable of recovering databases which the legacy recovery
// module could not recover. These include:
//   - tables with the WITHOUT ROWID optimization
//   - databases which use Write-Ahead Log (i.e. WAL mode)
//     - NOTE: as WAL mode is still experimental (see https://crbug.com/1416213)
//       recovery should not be attempted on WAL databases for now.
//
// Uses SQLite's recovery extension: https://www.sqlite.org/recovery.html
class COMPONENT_EXPORT(SQL) Recovery {
 public:
  enum class Strategy {
    // Razes the database if it could not be recovered.
    kRecoverOrRaze,

    // Razes the database if it could not be recovered, or if a valid meta table
    // with a version value could not be determined from the recovered database.
    // Use this strategy if your client makes assertions about the version of
    // the database schema.
    kRecoverWithMetaVersionOrRaze,

    // TODO(crbug.com/40061775): Consider exposing a way to keep around a
    // successfully-recovered, but unsuccessfully-restored database if needed.
  };

  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  enum class Result {
    // Outcome not yet known. This value should never be logged.
    kUnknown = 0,

    // Successfully completed the full database recovery process.
    kSuccess = 1,

    // Failed to initialize and configure the sqlite3_recover object.
    kFailedRecoveryInit = 2,

    // Failed to run recovery with the sqlite3_recover object.
    kFailedRecoveryRun = 3,

    // The database was successfully recovered to a backup, but we could not
    // open the newly-recovered database in order to copy it to the original
    // database.
    kFailedToOpenRecoveredDatabase = 4,

    // The database was successfully recovered to a backup, but a meta table
    // could not be found in the recovered database.
    // Only valid when using Strategy::kRecoverWithMetaVersionOrRaze.
    kFailedMetaTableDoesNotExist = 5,

    // The database was successfully recovered to a backup, but the meta table
    // could not be initialized.
    // Only valid when using Strategy::kRecoverWithMetaVersionOrRaze.
    kFailedMetaTableInit = 6,

    // The database was successfully recovered to a backup, but a valid
    // (meaning, positive) version number could not be read from the meta table.
    // Only valid when using Strategy::kRecoverWithMetaVersionOrRaze.
    kFailedMetaTableVersionWasInvalid = 7,

    // Failed to initialize and configure the sqlite3_backup object.
    kFailedBackupInit = 8,

    // Failed to run backup with the sqlite3_backup object.
    kFailedBackupRun = 9,
    kMaxValue = kFailedBackupRun,
  };

  // Returns true if `RecoverDatabase()` can plausibly fix `database` given this
  // `extended_error`. This does not guarantee that `RecoverDatabase()` will
  // successfully recover the database.
  //
  // Note that even if this method returns true, the database's error callback
  // must be reset before recovery can be attempted.
  [[nodiscard]] static bool ShouldAttemptRecovery(Database* database,
                                                  int extended_error);

  // Use `RecoverIfPossible()` below rather than using this method directly.
  //
  // Attempts to recover `database`, and razes the database if it could not be
  // recovered according to `strategy`. After attempting recovery, the database
  // can be re-opened and assumed to be free of corruption.
  //
  // Use Database::set_histogram_tag() to log UMA for recovery results specific
  // to the given feature database.
  //
  // It is not considered an error if some or all of the data cannot be
  // recovered due to database corruption, so it is possible that some records
  // could not be salvaged from the corrupted database.
  // TODO(crbug.com/40061775): Support the lost-and-found table if the
  // need arises to try to restore all these records.
  //
  // It is illegal to attempt recovery if:
  //   - `database` is null,
  //   - `database` is not open,
  //   - `database` is an in-memory or temporary database, or
  //   - `database` has an error callback set
  //
  // During the recovery process, `database` is poisoned so that operations on
  // the stack do not accidentally disrupt the restored data.
  //
  // Returns a SQLite error code specifying whether the database was
  // successfully recovered.
  [[nodiscard]] static SqliteResultCode RecoverDatabase(Database* database,
                                                        Strategy strategy);

  // Similar to `RecoverDatabase()` above, but with a couple key differences:
  //   - Can be called without first checking `ShouldAttemptRecovery()`.
  //   - `database`'s error callback will be reset if recovery is attempted.
  //   - Must only be called from within a database error callback.
  //
  // Recommended usage from within a database error callback:
  //
  //  // Attempt to recover the database, if recovery is possible.
  //  if (sql::Recovery::RecoverIfPossible(
  //          &db, extended_error,
  //          sql::Recovery::Strategy::kRecoverWithMetaVersionOrRaze)) {
  //    // Recovery was attempted. The database handle has been poisoned and the
  //    // error callback has been reset.
  //
  //    // ...
  //  }
  //
  [[nodiscard]] static bool RecoverIfPossible(Database* database,
                                              int extended_error,
                                              Strategy strategy);

  Recovery(const Recovery&) = delete;
  Recovery& operator=(const Recovery&) = delete;

 private:
  Recovery(Database* database, Strategy strategy);
  ~Recovery();

  // Entry point.
  SqliteResultCode RecoverAndReplaceDatabase();

  // Use SQLite's corruption recovery module to store the recovered content in
  // `recover_db_`. See https://www.sqlite.org/recovery.html
  SqliteResultCode AttemptToRecoverDatabaseToBackup();

  bool RecoveredDbHasValidMetaTable();

  // Use SQLite's Online Backup API to replace the original database with
  // `recover_db_`. See https://www.sqlite.org/backup.html
  SqliteResultCode ReplaceOriginalWithRecoveredDb();

  void SetRecoverySucceeded();
  void SetRecoveryFailed(Result failure_result, SqliteResultCode result_code);

  const Strategy strategy_;

  // If non-empty, UMA will be logged with the result of the recovery for this
  // specific database.
  std::string database_uma_name_;

  // Result of the recovery. This value must be set to something other than
  // `kUnknown` before this object is destroyed.
  Result result_ = Result::kUnknown;
  SqliteResultCode sqlite_result_code_ = SqliteResultCode::kOk;

  raw_ptr<Database> db_;  // Original Database connection.
  Database recover_db_;   // Recovery Database connection.

  base::FilePath recovery_database_path_;
};

}  // namespace sql

#endif  // SQL_RECOVERY_H_
