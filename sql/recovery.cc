// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/recovery.h"

#include <stddef.h>

#include <string>
#include <tuple>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/internal_api_token.h"
#include "sql/meta_table.h"
#include "sql/sqlite_result_code.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql {

namespace {

constexpr char kMainDatabaseName[] = "main";

}  // namespace

// static
bool Recovery::ShouldAttemptRecovery(Database* database, int extended_error) {
  return database && database->is_open() &&
         !database->DbPath(InternalApiToken()).empty() &&
#if BUILDFLAG(IS_FUCHSIA)
         // Recovering WAL databases is not supported on Fuchsia.
         !database->UseWALMode() &&
#endif  // BUILDFLAG(IS_FUCHSIA)
         IsErrorCatastrophic(extended_error);
}

// static
SqliteResultCode Recovery::RecoverDatabase(Database* database,
                                           Strategy strategy) {
  auto recovery = Recovery(database, strategy);
  return recovery.RecoverAndReplaceDatabase();
}

// static
bool Recovery::RecoverIfPossible(Database* database,
                                 int extended_error,
                                 Strategy strategy) {
  if (!ShouldAttemptRecovery(database, extended_error)) {
    return false;
  }

  // Recovery should be attempted. Since recovery must only be attempted from
  // within a database error callback, reset the error callback to prevent
  // re-entry.
  database->reset_error_callback();

  auto result = Recovery::RecoverDatabase(database, strategy);
  if (!IsSqliteSuccessCode(result)) {
    DLOG(ERROR) << "Database recovery failed with result code: " << result;
  }

  return true;
}

Recovery::Recovery(Database* database, Strategy strategy)
    : strategy_(strategy),
      db_(database),
      recover_db_(sql::DatabaseOptions{
          .page_size = database ? database->page_size() : 0,
          .cache_size = 0,
      }) {
  CHECK(db_);
  CHECK(db_->is_open());
  // Recovery is likely to be used in error handling. To prevent re-entry due to
  // errors while attempting to recover the database, the error callback must
  // not be set.
  CHECK(!db_->has_error_callback());

  auto db_path = db_->DbPath(InternalApiToken());

  // Corruption recovery for in-memory databases is not supported.
  CHECK(!db_path.empty());

  // Cache the database's histogram tag while the database is open.
  database_uma_name_ = db_->histogram_tag();

  recovery_database_path_ = db_path.AddExtensionASCII(".recovery");

  // Break any outstanding transactions on the original database, since the
  // recovery module opens a transaction on the database while recovery is in
  // progress.
  db_->RollbackAllTransactions();
}

Recovery::~Recovery() {
  // Recovery result must be set before we reach this point.
  CHECK_NE(result_, Result::kUnknown);

  base::UmaHistogramEnumeration("Sql.Recovery.Result", result_);
  UmaHistogramSqliteResult("Sql.Recovery.ResultCode",
                           static_cast<int>(sqlite_result_code_));

  if (!database_uma_name_.empty()) {
    base::UmaHistogramEnumeration(
        base::StrCat({"Sql.Recovery.Result.", database_uma_name_}), result_);
    UmaHistogramSqliteResult(
        base::StrCat({"Sql.Recovery.ResultCode.", database_uma_name_}),
        static_cast<int>(sqlite_result_code_));
  }

  if (db_) {
    if (result_ == Result::kSuccess) {
      // Poison the original handle, but don't raze the database.
      db_->Poison();
    } else {
      db_->RazeAndPoison();
    }
  }

  db_ = nullptr;

  if (recover_db_.is_open()) {
    recover_db_.Close();
  }
  // TODO(crbug.com/40061775): Don't always delete the recovery db if we
  // are ever to keep around successfully-recovered, but unsuccessfully-restored
  // databases.
  sql::Database::Delete(recovery_database_path_);
}

void Recovery::SetRecoverySucceeded() {
  // Recovery result must only be set once.
  CHECK_EQ(result_, Result::kUnknown);

  result_ = Result::kSuccess;
}

void Recovery::SetRecoveryFailed(Result failure_result,
                                 SqliteResultCode result_code) {
  // Recovery result must only be set once.
  CHECK_EQ(result_, Result::kUnknown);

  switch (failure_result) {
    case Result::kUnknown:
    case Result::kSuccess:
      NOTREACHED();
    case Result::kFailedRecoveryInit:
    case Result::kFailedRecoveryRun:
    case Result::kFailedToOpenRecoveredDatabase:
    case Result::kFailedMetaTableDoesNotExist:
    case Result::kFailedMetaTableInit:
    case Result::kFailedMetaTableVersionWasInvalid:
    case Result::kFailedBackupInit:
    case Result::kFailedBackupRun:
      break;
  }

  result_ = failure_result;
  sqlite_result_code_ = result_code;
}

SqliteResultCode Recovery::RecoverAndReplaceDatabase() {
  auto sqlite_result_code = AttemptToRecoverDatabaseToBackup();
  if (sqlite_result_code != SqliteResultCode::kOk) {
    return sqlite_result_code;
  }

  // Open a connection to the newly-created recovery database.
  if (!recover_db_.Open(recovery_database_path_)) {
    DVLOG(1) << "Unable to open recovery database.";

    // TODO(crbug.com/40061775): It's unfortunate to give up now, after
    // we've successfully recovered the database to a backup. Consider falling
    // back to base::Move().
    SetRecoveryFailed(Result::kFailedToOpenRecoveredDatabase,
                      ToSqliteResultCode(recover_db_.GetErrorCode()));
    return SqliteResultCode::kError;
  }

  if (strategy_ == Strategy::kRecoverWithMetaVersionOrRaze &&
      !RecoveredDbHasValidMetaTable()) {
    DVLOG(1) << "Could not read valid version number from recovery database.";
    return SqliteResultCode::kError;
  }

  return ReplaceOriginalWithRecoveredDb();
}

SqliteResultCode Recovery::AttemptToRecoverDatabaseToBackup() {
  CHECK(db_->is_open());
  CHECK(!recover_db_.is_open());

  // See full documentation for the corruption recovery module in
  // https://sqlite.org/src/file/ext/recover/sqlite3recover.h

  // sqlite3_recover_init() create a new sqlite3_recover handle, with data being
  // recovered into a new database. This should very rarely fail - e.g. if
  // memory for the recovery object itself could not be allocated. If it does
  // fail, `recover` will be nullptr and an error code will surface when
  // attempting to configure the recovery object below.
  sqlite3_recover* recover =
      sqlite3_recover_init(db_->db(InternalApiToken()), kMainDatabaseName,
                           recovery_database_path_.AsUTF8Unsafe().c_str());

  // sqlite3_recover_config() configures the sqlite3_recover object.
  //
  // These functions should only fail if the above initialization failed, or if
  // invalid parameters are passed.

  // Don't bother creating a lost-and-found table.
  sqlite3_recover_config(recover, SQLITE_RECOVER_LOST_AND_FOUND, nullptr);
  // Do not attempt to recover records from pages that appear to be linked to
  // the freelist, to avoid "recovering" deleted records.
  int kRecoverFreelist = 0;
  sqlite3_recover_config(recover, SQLITE_RECOVER_FREELIST_CORRUPT,
                         static_cast<void*>(&kRecoverFreelist));
  // Attempt to recover ROWID values that are not INTEGER PRIMARY KEY.
  int kRecoverRowIds = 1;
  sqlite3_recover_config(recover, SQLITE_RECOVER_ROWIDS,
                         static_cast<void*>(&kRecoverRowIds));

  auto sqlite_result_code =
      ToSqliteResultCode(sqlite3_recover_errcode(recover));
  if (sqlite_result_code != SqliteResultCode::kOk) {
    CHECK_NE(sqlite_result_code, SqliteResultCode::kApiMisuse);

    // The recovery could not be configured.
    // TODO(crbug.com/40061775): This is likely a transient issue, so we
    // could consider keeping the database intact in case the caller wants to
    // try again later. For now, we'll always raze.
    SetRecoveryFailed(Result::kFailedRecoveryInit, sqlite_result_code);

    DVLOG(1) << "recovery config error: " << sqlite_result_code
             << sqlite3_recover_errcode(recover);

    // Clean up the recovery object.
    sqlite3_recover_finish(recover);
    return sqlite_result_code;
  }

  // sqlite3_recover_run() attempts to construct an copy of the database with
  // data corruption handled. It returns SQLITE_OK if recovery was successful.
  sqlite_result_code = ToSqliteResultCode(sqlite3_recover_run(recover));

  // sqlite3_recover_finish() cleans up the recovery object. It should return
  // the same error code as from sqlite3_recover_run().
  auto finish_result_code = ToSqliteResultCode(sqlite3_recover_finish(recover));
  CHECK_EQ(finish_result_code, sqlite_result_code);

  if (sqlite_result_code != SqliteResultCode::kOk) {
    // Could not recover the database.
    SetRecoveryFailed(Result::kFailedRecoveryRun, sqlite_result_code);

    DVLOG(1) << "recovery error: " << sqlite_result_code
             << sqlite3_recover_errmsg(recover);
  }

  return sqlite_result_code;
}

SqliteResultCode Recovery::ReplaceOriginalWithRecoveredDb() {
  CHECK(db_->is_open());
  CHECK(recover_db_.is_open());

  // sqlite3_backup_init() fails if a transaction is ongoing. This should be
  // rare, since we rolled back all transactions in this object's constructor.
  sqlite3_backup* backup = sqlite3_backup_init(
      db_->db(InternalApiToken()), kMainDatabaseName,
      recover_db_.db(InternalApiToken()), kMainDatabaseName);
  if (!backup) {
    // Error code is in the destination database handle.
    DVLOG(1) << "sqlite3_backup_init() failed: "
             << sqlite3_errmsg(db_->db(InternalApiToken()));

    auto result_code =
        ToSqliteResultCode(sqlite3_errcode(db_->db(InternalApiToken())));

    // TODO(crbug.com/40061775): It's unfortunate to give up now, after
    // we've successfully recovered the database. Consider falling back to
    // base::Move().
    SetRecoveryFailed(Result::kFailedBackupInit, result_code);
    return result_code;
  }

  // sqlite3_backup_step() copies pages from the source to the destination
  // database. It returns SQLITE_DONE if copying successfully completed, or some
  // other error on failure.
  // TODO(crbug.com/40061775): Some of these errors are transient and the
  // operation could feasibly succeed at a later time. Consider keeping around
  // successfully-recovered, but unsuccessfully-restored databases or falling
  // back to base::Move().
  constexpr int kUnlimitedPageCount = -1;  // Back up entire database.
  auto sqlite_result_code =
      ToSqliteResultCode(sqlite3_backup_step(backup, kUnlimitedPageCount));

  // sqlite3_backup_remaining() returns the number of pages still to be backed
  // up, which should be zero if sqlite3_backup_step() completed successfully.
  int pages_remaining = sqlite3_backup_remaining(backup);

  // sqlite3_backup_finish() releases the sqlite3_backup object.
  //
  // It returns an error code only if the backup encountered a permanent error.
  // We use the the sqlite3_backup_step() result instead, because it also tells
  // us about temporary errors, like SQLITE_BUSY.
  //
  // We pass the sqlite3_backup_finish() result code through
  // ToSqliteResultCode() to catch codes that should never occur, like
  // SQLITE_MISUSE.
  std::ignore = ToSqliteResultCode(sqlite3_backup_finish(backup));

  if (sqlite_result_code != SqliteResultCode::kDone) {
    CHECK_NE(sqlite_result_code, SqliteResultCode::kOk)
        << "sqlite3_backup_step() returned SQLITE_OK (instead of SQLITE_DONE) "
        << "when asked to back up the entire database";

    DVLOG(1) << "sqlite3_backup_step() failed: "
             << sqlite3_errmsg(db_->db(InternalApiToken()));
    SetRecoveryFailed(Result::kFailedBackupRun, sqlite_result_code);
    return sqlite_result_code;
  }

  // The original database was successfully recovered and replaced. Hooray!
  SetRecoverySucceeded();

  CHECK_EQ(pages_remaining, 0);
  return SqliteResultCode::kOk;
}

bool Recovery::RecoveredDbHasValidMetaTable() {
  CHECK(recover_db_.is_open());

  if (!MetaTable::DoesTableExist(&recover_db_)) {
    DVLOG(1) << "Meta table does not exist in recovery database.";
    SetRecoveryFailed(Result::kFailedMetaTableDoesNotExist,
                      ToSqliteResultCode(recover_db_.GetErrorCode()));
    return false;
  }

  // MetaTable::Init will not create a meta table if one already exists.
  sql::MetaTable meta_table;
  if (!meta_table.Init(&recover_db_, /*version=*/1,
                       /*compatible_version=*/1)) {
    SetRecoveryFailed(Result::kFailedMetaTableInit,
                      ToSqliteResultCode(recover_db_.GetErrorCode()));
    return false;
  }

  // Confirm that we can read a valid version number from the recovered table.
  if (meta_table.GetVersionNumber() <= 0) {
    SetRecoveryFailed(Result::kFailedMetaTableVersionWasInvalid,
                      ToSqliteResultCode(recover_db_.GetErrorCode()));
    return false;
  }

  return true;
}

}  // namespace sql
