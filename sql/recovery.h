// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SQL_RECOVERY_H_
#define SQL_RECOVERY_H_

#include <stddef.h>

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "sql/database.h"
#include "sql/internal_api_token.h"
#include "sql/sqlite_result_code_values.h"

namespace base {
struct Feature;
class FilePath;
}

namespace sql {

// WARNING: This API is still experimental. See https://crbug.com/1385500.
//
// Uses SQLite's built-in corruption recovery module to recover the database.
// See https://www.sqlite.org/recovery.html
//
// For now, feature teams should use only the `RecoverIfPossible()` method -
// which falls back to the legacy `sql::Recovery` below if necessary - in lieu
// of calling `RecoverDatabase()` directly.
class COMPONENT_EXPORT(SQL) BuiltInRecovery {
 public:
  enum class Strategy {
    // Razes the database if it could not be recovered.
    kRecoverOrRaze,

    // Razes the database if it could not be recovered, or if a valid meta table
    // with a version value could not be determined from the recovered database.
    // Use this strategy if your client makes assertions about the version of
    // the database schema.
    kRecoverWithMetaVersionOrRaze,

    // TODO(https://crbug.com/1385500): Consider exposing a way to keep around a
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

  [[nodiscard]] static bool IsSupported();

  // Returns true if `RecoverDatabase()` can plausibly fix `database` given this
  // `extended_error`. This does not guarantee that `RecoverDatabase()` will
  // successfully recover the database.
  //
  // Note that even if this method returns true, the database's error callback
  // must be reset before recovery can be attempted.
  [[nodiscard]] static bool ShouldAttemptRecovery(Database* database,
                                                  int extended_error);

  // WARNING: This API is experimental. For now, please use
  // `RecoverIfPossible()` below rather than using this method directly.
  //
  // Attempts to recover `database`, and razes the database if it could not be
  // recovered according to `strategy`. After attempting recovery, the database
  // can be re-opened and assumed to be free of corruption.
  //
  // `database_uma_name` is used to log UMA specific to the given database.
  //
  // It is not considered an error if some or all of the data cannot be
  // recovered due to database corruption, so it is possible that some records
  // could not be salvaged from the corrupted database.
  // TODO(https://crbug.com/1385500): Support the lost-and-found table if the
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
  [[nodiscard]] static SqliteResultCode RecoverDatabase(
      Database* database,
      Strategy strategy,
      std::string database_uma_name = std::string());

  // Similar to `RecoverDatabase()` above, but with a few key differences:
  //   - Uses `BuiltInRecovery` or the legacy `Recovery` to recover the
  //     database, as appropriate. This method facilitates the migration to the
  //     newer recovery module with minimal impact on feature teams. The
  //     expectation is that `Recovery` will eventually be removed entirely.
  //     See https://crbug.com/1385500.
  //   - Can be called without first checking `ShouldAttemptRecovery()`.
  //   - `database`'s error callback will be reset if recovery is attempted.
  //   - Must only be called from within a database error callback.
  //   - Includes the option to pass a per-database feature flag indicating
  //     whether `BuiltInRecovery` should be used to recover this database, if
  //     it's supported. A per-database UMA may optionally be logged, as well.
  //
  // Recommended usage from within a database error callback:
  //
  //  // Attempt to recover the database, if recovery is possible.
  //  if (sql::BuiltInRecovery::RecoverIfPossible(
  //          &db, extended_error,
  //          sql::BuiltInRecovery::Strategy::kRecoverWithMetaVersionOrRaze,
  //          &features::kMyFeatureTeamShouldUseBuiltInRecoveryIfSupported,
  //          "MyFeatureDatabase")) {
  //    // Recovery was attempted. The database handle has been poisoned and the
  //    // error callback has been reset.
  //
  //    // ...
  //  }
  //
  [[nodiscard]] static bool RecoverIfPossible(
      Database* database,
      int extended_error,
      Strategy strategy,
      const base::Feature* const use_builtin_recovery_if_supported_flag =
          nullptr,
      std::string database_uma_name = std::string());

  BuiltInRecovery(const BuiltInRecovery&) = delete;
  BuiltInRecovery& operator=(const BuiltInRecovery&) = delete;

 private:
  BuiltInRecovery(Database* database,
                  Strategy strategy,
                  std::string database_uma_name);
  ~BuiltInRecovery();

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
  const std::string database_uma_name_;

  // Result of the recovery. This value must be set to something other than
  // `kUnknown` before this object is destroyed.
  Result result_ = Result::kUnknown;
  SqliteResultCode sqlite_result_code_ = SqliteResultCode::kOk;

  raw_ptr<Database> db_;  // Original Database connection.
  Database recover_db_;   // Recovery Database connection.

  base::FilePath recovery_database_path_;
};

// Recovery module for sql/.  The basic idea is to create a fresh database and
// populate it with the recovered contents of the original database.  If
// recovery is successful, the recovered database is backed up over the original
// database.  If recovery is not successful, the original database is razed.  In
// either case, the original handle is poisoned so that operations on the stack
// do not accidentally disrupt the restored data.
//
// RecoverDatabase() automates this, including recoverying the schema of from
// the suspect database.  If a database requires special handling, such as
// recovering between different schema, or tables requiring post-processing,
// then the module can be used manually like:
//
// {
//   std::unique_ptr<sql::Recovery> r =
//       sql::Recovery::Begin(orig_db, orig_db_path);
//   if (r) {
//     // Create the schema to recover to.  On failure, clear the
//     // database.
//     if (!r.db()->Execute(kCreateSchemaSql)) {
//       sql::Recovery::Unrecoverable(std::move(r));
//       return;
//     }
//
//     // Recover data in "mytable".
//     size_t rows_recovered = 0;
//     if (!r.AutoRecoverTable("mytable", 0, &rows_recovered)) {
//       sql::Recovery::Unrecoverable(std::move(r));
//       return;
//     }
//
//     // Manually cleanup additional constraints.
//     if (!r.db()->Execute(kCleanupSql)) {
//       sql::Recovery::Unrecoverable(std::move(r));
//       return;
//     }
//
//     // Commit the recovered data to the original database file.
//     sql::Recovery::Recovered(std::move(r));
//   }
// }
//
// If Recovered() is not called, then RazeAndPoison() is called on
// orig_db.
class COMPONENT_EXPORT(SQL) Recovery {
 public:
  Recovery(const Recovery&) = delete;
  Recovery& operator=(const Recovery&) = delete;
  ~Recovery();

  // Begin the recovery process by opening a temporary database handle
  // and attach the existing database to it at "corrupt".  To prevent
  // deadlock, all transactions on |database| are rolled back.
  //
  // Returns nullptr in case of failure, with no cleanup done on the
  // original database (except for breaking the transactions).  The
  // caller should Raze() or otherwise cleanup as appropriate.
  //
  // TODO(shess): Later versions of SQLite allow extracting the path
  // from the database.
  // TODO(shess): Allow specifying the connection point?
  [[nodiscard]] static std::unique_ptr<Recovery> Begin(
      Database* database,
      const base::FilePath& db_path);

  // Mark recovery completed by replicating the recovery database over
  // the original database, then closing the recovery database.  The
  // original database handle is poisoned, causing future calls
  // against it to fail.
  //
  // If Recovered() is not called, the destructor will call
  // Unrecoverable().
  //
  // TODO(shess): At this time, this function can fail while leaving
  // the original database intact.  Figure out which failure cases
  // should go to RazeAndPoison() instead.
  [[nodiscard]] static bool Recovered(std::unique_ptr<Recovery> r);

  // Indicate that the database is unrecoverable.  The original
  // database is razed, and the handle poisoned.
  static void Unrecoverable(std::unique_ptr<Recovery> r);

  // When initially developing recovery code, sometimes the possible
  // database states are not well-understood without further
  // diagnostics.  Abandon recovery but do not raze the original
  // database.
  // NOTE(shess): Only call this when adding recovery support.  In the
  // steady state, all databases should progress to recovered or razed.
  static void Rollback(std::unique_ptr<Recovery> r);

  // Handle to the temporary recovery database.
  sql::Database* db() { return &recover_db_; }

  // Attempt to recover the named table from the corrupt database into
  // the recovery database using a temporary recover virtual table.
  // The virtual table schema is derived from the named table's schema
  // in database [main].  Data is copied using INSERT OR IGNORE, so
  // duplicates are dropped.
  //
  // If the source table has fewer columns than the target, the target
  // DEFAULT value will be used for those columns.
  //
  // Returns true if all operations succeeded, with the number of rows
  // recovered in |*rows_recovered|.
  //
  // NOTE(shess): Due to a flaw in the recovery virtual table, at this
  // time this code injects the DEFAULT value of the target table in
  // locations where the recovery table returns nullptr.  This is not
  // entirely correct, because it happens both when there is a short
  // row (correct) but also where there is an actual NULL value
  // (incorrect).
  //
  // TODO(shess): Flag for INSERT OR REPLACE vs IGNORE.
  // TODO(shess): Handle extended table names.
  bool AutoRecoverTable(const char* table_name, size_t* rows_recovered);

  // Setup a recover virtual table at temp.recover_meta, reading from
  // corrupt.meta.  Returns true if created.
  // TODO(shess): Perhaps integrate into Begin().
  // TODO(shess): Add helpers to fetch additional items from the meta
  // table as needed.
  bool SetupMeta();

  // Fetch the version number from temp.recover_meta.  Returns false
  // if the query fails, or if there is no version row.  Otherwise
  // returns true, with the version in |*version_number|.
  //
  // Only valid to call after successful SetupMeta().
  bool GetMetaVersionNumber(int* version_number);

  // Attempt to recover the database by creating a new database with schema from
  // |db|, then copying over as much data as possible.  If successful, the
  // recovery handle is returned to allow the caller to make additional changes,
  // such as validating constraints not expressed in the schema.
  //
  // In case of SQLITE_NOTADB, the database is deemed unrecoverable and deleted.
  [[nodiscard]] static std::unique_ptr<Recovery> BeginRecoverDatabase(
      Database* db,
      const base::FilePath& db_path);

  // Call BeginRecoverDatabase() to recover the database, then commit the
  // changes using Recovered().  After this call, the |db| handle will be
  // poisoned (though technically remaining open) so that future calls will
  // return errors until the handle is re-opened.
  static void RecoverDatabase(Database* db, const base::FilePath& db_path);

  // Variant on RecoverDatabase() which requires that the database have a valid
  // meta table with a version value.  The meta version value is used by some
  // clients to make assertions about the database schema.  If this information
  // cannot be determined, the database is considered unrecoverable.
  static void RecoverDatabaseWithMetaVersion(Database* db,
                                             const base::FilePath& db_path);

  // Returns true for SQLite errors which RecoverDatabase() can plausibly fix.
  // This does not guarantee that RecoverDatabase() will successfully recover
  // the database.
  static bool ShouldRecover(int extended_error);

  // Enables the "recover" SQLite extension for a database connection.
  //
  // Returns a SQLite error code.
  static int EnableRecoveryExtension(Database* db, InternalApiToken);

 private:
  explicit Recovery(Database* database);

  // Setup the recovery database handle for Begin().  Returns false in
  // case anything failed.
  [[nodiscard]] bool Init(const base::FilePath& db_path);

  // Copy the recovered database over the original database.
  [[nodiscard]] bool Backup();

  // Close the recovery database, and poison the original handle.
  // |raze| controls whether the original database is razed or just
  // poisoned.
  enum Disposition {
    RAZE_AND_POISON,
    POISON,
  };
  void Shutdown(Disposition raze);

  raw_ptr<Database> db_;  // Original Database connection.
  Database recover_db_;  // Recovery Database connection.
};

}  // namespace sql

#endif  // SQL_RECOVERY_H_
