// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/first_party_sets/database/first_party_sets_database.h"

#include <inttypes.h>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "content/browser/first_party_sets/first_party_set_parser.h"
#include "net/base/schemeful_site.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace content {

namespace {

// Version number of the database.
const int kCurrentVersionNumber = 1;

const char kRunCountKey[] = "run_count";

[[nodiscard]] bool InitSchema(sql::Database& db) {
  static constexpr char kSitesToClearSql[] =
      "CREATE TABLE IF NOT EXISTS sites_to_clear("
      "site TEXT PRIMARY KEY NOT NULL,"
      "marked_at_run INTEGER NOT NULL"
      ")WITHOUT ROWID";
  if (!db.Execute(kSitesToClearSql))
    return false;

  static constexpr char kMarkedAtRunSitesSql[] =
      "CREATE INDEX IF NOT EXISTS idx_marked_at_run_sites "
      "ON sites_to_clear(marked_at_run)";
  if (!db.Execute(kMarkedAtRunSitesSql))
    return false;

  static constexpr char kBrowserContextsClearedSql[] =
      "CREATE TABLE IF NOT EXISTS browser_contexts_cleared("
      "browser_context_id TEXT PRIMARY KEY NOT NULL,"
      "cleared_at_run INTEGER NOT NULL"
      ")WITHOUT ROWID";
  if (!db.Execute(kBrowserContextsClearedSql))
    return false;

  static constexpr char kClearedAtRunBrowserContextsSql[] =
      "CREATE INDEX IF NOT EXISTS idx_cleared_at_run_browser_contexts "
      "ON browser_contexts_cleared(cleared_at_run)";
  if (!db.Execute(kClearedAtRunBrowserContextsSql))
    return false;

  return true;
}

void RecordInitializationStatus(FirstPartySetsDatabase::InitStatus status) {
  base::UmaHistogramEnumeration("FirstPartySets.Database.InitStatus", status);
}

}  // namespace

FirstPartySetsDatabase::FirstPartySetsDatabase(base::FilePath db_path)
    : db_path_(std::move(db_path)) {
  DCHECK(db_path_.IsAbsolute());
}

FirstPartySetsDatabase::~FirstPartySetsDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool FirstPartySetsDatabase::InsertSitesToClear(
    const std::vector<net::SchemefulSite>& sites) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit())
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  for (const auto& site : sites) {
    DCHECK(!site.opaque());
    static constexpr char kInsertSitesToClearSql[] =
        // clang-format off
        "INSERT OR REPLACE INTO sites_to_clear(site,marked_at_run) "
        "VALUES(?,?)";
    // clang-format on
    sql::Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kInsertSitesToClearSql));
    statement.BindString(0, site.Serialize());
    statement.BindInt64(1, run_count_);

    if (!statement.Run())
      return false;
  }
  return transaction.Commit();
}

bool FirstPartySetsDatabase::InsertBrowserContextCleared(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!browser_context_id.empty());

  if (!LazyInit())
    return false;

  static constexpr char kInsertBrowserContextsClearedSql[] =
      // clang-format off
      "INSERT OR REPLACE INTO browser_contexts_cleared(browser_context_id,cleared_at_run) "
      "VALUES(?,?)";
  // clang-format on
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertBrowserContextsClearedSql));
  statement.BindString(0, browser_context_id);
  statement.BindInt64(1, run_count_);

  return statement.Run();
}

std::vector<net::SchemefulSite> FirstPartySetsDatabase::FetchSitesToClear(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!browser_context_id.empty());

  if (!LazyInit())
    return {};

  // No-op if the `browser_context_id` does not exist before.
  if (!HasEntryFor(browser_context_id))
    return {};

  std::vector<net::SchemefulSite> results;
  static constexpr char kSelectSitesToClearSql[] =
      // clang-format off
      "SELECT site FROM sites_to_clear "
      "WHERE marked_at_run>"
        "(SELECT cleared_at_run FROM browser_contexts_cleared "
         "WHERE browser_context_id=?)";
  // clang-format on

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectSitesToClearSql));
  statement.BindString(0, browser_context_id);

  while (statement.Step()) {
    absl::optional<net::SchemefulSite> site =
        FirstPartySetParser::CanonicalizeRegisteredDomain(
            statement.ColumnString(0), /*emit_errors=*/false);
    // TODO(crbug/1314039): Invalid sites should be rare case but possible.
    // Consider deleting them from DB.
    if (site.has_value()) {
      results.push_back(std::move(site.value()));
    }
  }

  if (!statement.Succeeded())
    return {};

  return results;
}

bool FirstPartySetsDatabase::LazyInit() {
  // Early return in case of previous failure, to prevent an unbounded
  // number of re-attempts.
  if (db_status_ != InitStatus::kUnattempted)
    return db_status_ == InitStatus::kSuccess;

  DCHECK_EQ(db_.get(), nullptr);
  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions{
      .exclusive_locking = true, .page_size = 4096, .cache_size = 32});
  db_->set_histogram_tag("FirstPartySets");
  // base::Unretained is safe here because this FirstPartySetsDatabase owns
  // the sql::Database instance that stores and uses the callback. So,
  // `this` is guaranteed to outlive the callback.
  db_->set_error_callback(base::BindRepeating(
      &FirstPartySetsDatabase::DatabaseErrorCallback, base::Unretained(this)));
  db_status_ = InitializeTables();

  if (db_status_ != InitStatus::kSuccess) {
    db_.reset();
    meta_table_.Reset();
  } else {
    IncreaseRunCount();
  }

  RecordInitializationStatus(db_status_);
  return db_status_ == InitStatus::kSuccess;
}

bool FirstPartySetsDatabase::OpenDatabase() {
  DCHECK(db_);
  if (db_->is_open() || db_->Open(db_path_)) {
    db_->Preload();
    return true;
  }
  return false;
}

void FirstPartySetsDatabase::DatabaseErrorCallback(int extended_error,
                                                   sql::Statement* stmt) {
  DCHECK(db_);
  // Attempt to recover a corrupt database.
  if (sql::Recovery::ShouldRecover(extended_error)) {
    // Prevent reentrant calls.
    db_->reset_error_callback();

    // After this call, the |db_| handle is poisoned so that future calls will
    // return errors until the handle is re-opened.
    sql::Recovery::RecoverDatabaseWithMetaVersion(db_.get(), db_path_);

    // The DLOG(FATAL) below is intended to draw immediate attention to errors
    // in newly-written code. Database corruption is generally a result of OS or
    // hardware issues, not coding errors at the client level, so displaying the
    // error would probably lead to confusion. The ignored call signals the
    // test-expectation framework that the error was handled.
    std::ignore = sql::Database::IsExpectedSqliteError(extended_error);
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db_->GetErrorMessage();

  // Consider the database closed if we did not attempt to recover so we did not
  // produce further errors.
  db_status_ = InitStatus::kError;
}

FirstPartySetsDatabase::InitStatus FirstPartySetsDatabase::InitializeTables() {
  if (!OpenDatabase())
    return InitStatus::kError;

  // Database should now be open.
  DCHECK(db_->is_open());

  // Scope initialization in a transaction so we can't be partially initialized.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    LOG(WARNING) << "First-Party Sets database begin initialization failed.";
    db_->RazeAndClose();
    return InitStatus::kError;
  }

  // Create the tables.
  if (!meta_table_.Init(db_.get(), kCurrentVersionNumber,
                        kCurrentVersionNumber) ||
      !InitSchema(*db_)) {
    return InitStatus::kError;
  }

  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "First-Party Sets database is too new.";
    return InitStatus::kTooNew;
  }

  if (meta_table_.GetVersionNumber() < kCurrentVersionNumber) {
    LOG(WARNING) << "First-Party Sets database is too old to be compatible.";
    return InitStatus::kTooOld;
  }

  if (!transaction.Commit()) {
    LOG(WARNING) << "First-Party Sets database initialization commit failed.";
    return InitStatus::kError;
  }

  return InitStatus::kSuccess;
}

void FirstPartySetsDatabase::IncreaseRunCount() {
  DCHECK_EQ(db_status_, InitStatus::kSuccess);
  // 0 is the default value, `run_count_` should only be set once.
  DCHECK_EQ(run_count_, 0);

  int64_t count = 0;
  // `count` should be positive if the value exists in the meta table. Consider
  // db data is corrupted and delete db file if that's not the case.
  if (meta_table_.GetValue(kRunCountKey, &count) && count <= 0) {
    db_status_ = InitStatus::kCorrupted;
    // TODO(crbug/1316090): Need to resolve how the restarted `run_count_` could
    // affect cache clearing.
    if (!Destroy()) {
      LOG(ERROR) << "First-Party Sets database destruction failed.";
    }
    return;
  }

  run_count_ = count + 1;
  // TODO(crbug/1314039): Figure out how to handle run_count update failure.
  if (!meta_table_.SetValue(kRunCountKey, run_count_)) {
    LOG(ERROR) << "First-Party Sets database updating run_count failed.";
  }
}

bool FirstPartySetsDatabase::HasEntryFor(
    const std::string& browser_context_id) const {
  DCHECK_EQ(db_status_, InitStatus::kSuccess);
  DCHECK(!browser_context_id.empty());

  static constexpr char kSelectBrowserContextSql[] =
      "SELECT 1 FROM browser_contexts_cleared "
      "WHERE browser_context_id=?"
      "LIMIT 1";

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectBrowserContextSql));
  statement.BindString(0, browser_context_id);

  return statement.Step();
}

bool FirstPartySetsDatabase::Destroy() {
  // Reset the value.
  run_count_ = 0;

  if (db_ && db_->is_open() && !db_->RazeAndClose())
    return false;

  // The file already doesn't exist.
  if (db_path_.empty())
    return true;

  return base::DeleteFile(db_path_);
}

}  // namespace content
