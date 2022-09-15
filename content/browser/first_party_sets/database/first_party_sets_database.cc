// Copyright 2022 The Chromium Authors
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
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
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
  static constexpr char kPublicSetsSql[] =
      "CREATE TABLE IF NOT EXISTS public_sets("
      "site TEXT NOT NULL,"
      "primary_site TEXT NOT NULL,"
      "site_type INTEGER NOT NULL,"
      "PRIMARY KEY(site)"
      ")WITHOUT ROWID";
  if (!db.Execute(kPublicSetsSql))
    return false;

  static constexpr char kBrowserContextSitesToClearSql[] =
      "CREATE TABLE IF NOT EXISTS browser_context_sites_to_clear("
      "browser_context_id TEXT NOT NULL,"
      "site TEXT NOT NULL,"
      "marked_at_run INTEGER NOT NULL,"
      "PRIMARY KEY(browser_context_id,site)"
      ")WITHOUT ROWID";
  if (!db.Execute(kBrowserContextSitesToClearSql))
    return false;

  static constexpr char kMarkedAtRunSitesSql[] =
      "CREATE INDEX IF NOT EXISTS idx_marked_at_run_sites "
      "ON browser_context_sites_to_clear(marked_at_run)";
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

  static constexpr char kPolicyModificationsSql[] =
      "CREATE TABLE IF NOT EXISTS policy_modifications("
      "browser_context_id TEXT NOT NULL,"
      "site TEXT NOT NULL,"
      "site_owner TEXT,"  // May be NULL if this row represents a deletion.
      "PRIMARY KEY(browser_context_id,site)"
      ")WITHOUT ROWID";
  if (!db.Execute(kPolicyModificationsSql))
    return false;

  return true;
}

void RecordInitializationStatus(FirstPartySetsDatabase::InitStatus status) {
  base::UmaHistogramEnumeration("FirstPartySets.Database.InitStatus", status);
}

absl::optional<net::SiteType> DeserializeSiteType(int value) {
  switch (value) {
    case static_cast<int>(net::SiteType::kPrimary):
      return net::SiteType::kPrimary;
    case static_cast<int>(net::SiteType::kAssociated):
      return net::SiteType::kAssociated;
  }
  return absl::nullopt;
}

}  // namespace

FirstPartySetsDatabase::FirstPartySetsDatabase(base::FilePath db_path)
    : db_path_(std::move(db_path)) {
  DCHECK(db_path_.IsAbsolute());
}

FirstPartySetsDatabase::~FirstPartySetsDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool FirstPartySetsDatabase::SetPublicSets(
    const FirstPartySetsDatabase::FlattenedSets& sets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit())
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteSql[] = "DELETE FROM public_sets";
  sql::Statement delete_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  if (!delete_statement.Run())
    return false;

  for (const auto& [site, entry] : sets) {
    DCHECK(!site.opaque());
    DCHECK(!entry.primary().opaque());
    static constexpr char kInsertSql[] =
        "INSERT INTO public_sets(site,primary_site,site_type)"
        "VALUES(?,?,?)";
    sql::Statement insert_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kInsertSql));
    insert_statement.BindString(0, site.Serialize());
    insert_statement.BindString(1, entry.primary().Serialize());
    insert_statement.BindInt(2, static_cast<int>(entry.site_type()));

    if (!insert_statement.Run())
      return false;
  }
  return transaction.Commit();
}

bool FirstPartySetsDatabase::InsertSitesToClear(
    const std::string& browser_context_id,
    const base::flat_set<net::SchemefulSite>& sites) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit())
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  for (const auto& site : sites) {
    DCHECK(!site.opaque());
    static constexpr char kInsertSql[] =
        // clang-format off
        "INSERT OR REPLACE INTO browser_context_sites_to_clear"
        "(browser_context_id,site,marked_at_run)"
        "VALUES(?,?,?)";
    // clang-format on
    sql::Statement statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kInsertSql));
    statement.BindString(0, browser_context_id);
    statement.BindString(1, site.Serialize());
    statement.BindInt64(2, run_count_);

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
      "INSERT OR REPLACE INTO browser_contexts_cleared(browser_context_id,cleared_at_run)"
      "VALUES(?,?)";
  // clang-format on
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertBrowserContextsClearedSql));
  statement.BindString(0, browser_context_id);
  statement.BindInt64(1, run_count_);

  return statement.Run();
}

bool FirstPartySetsDatabase::InsertPolicyModifications(
    const std::string& browser_context_id,
    const base::flat_map<net::SchemefulSite,
                         absl::optional<net::FirstPartySetEntry>>&
        modificatons) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit())
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteSql[] =
      "DELETE FROM policy_modifications WHERE browser_context_id=?";
  sql::Statement delete_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteSql));
  delete_statement.BindString(0, browser_context_id);
  if (!delete_statement.Run())
    return false;

  for (const auto& [site, owner] : modificatons) {
    DCHECK(!site.opaque());
    static constexpr char kInsertSql[] =
        "INSERT INTO policy_modifications(browser_context_id,site,site_owner)"
        "VALUES(?,?,?)";
    sql::Statement insert_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kInsertSql));
    insert_statement.BindString(0, browser_context_id);
    insert_statement.BindString(1, site.Serialize());
    if (owner.has_value()) {
      insert_statement.BindString(2, owner.value().primary().Serialize());
    } else {
      insert_statement.BindNull(2);
    }

    if (!insert_statement.Run())
      return false;
  }
  return transaction.Commit();
}

FirstPartySetsDatabase::FlattenedSets FirstPartySetsDatabase::GetPublicSets() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit())
    return {};

  std::vector<std::pair<net::SchemefulSite, net::FirstPartySetEntry>> results;
  static constexpr char kSelectSql[] =
      "SELECT site,primary_site,site_type FROM public_sets";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSelectSql));

  while (statement.Step()) {
    absl::optional<net::SchemefulSite> site =
        FirstPartySetParser::CanonicalizeRegisteredDomain(
            statement.ColumnString(0), /*emit_errors=*/false);

    absl::optional<net::SchemefulSite> primary =
        FirstPartySetParser::CanonicalizeRegisteredDomain(
            statement.ColumnString(1), /*emit_errors=*/false);

    absl::optional<net::SiteType> site_type =
        DeserializeSiteType(statement.ColumnInt(2));

    // TODO(crbug.com/1314039): Invalid entries should be rare case but
    // possible. Consider deleting them from DB.
    if (site.has_value() && primary.has_value() && site_type.has_value()) {
      results.emplace_back(
          std::move(site.value()),
          net::FirstPartySetEntry(primary.value(), site_type.value(),
                                  /*site_index=*/absl::nullopt));
    }
  }
  if (!statement.Succeeded())
    return {};

  return results;
}

std::vector<net::SchemefulSite> FirstPartySetsDatabase::FetchSitesToClear(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!browser_context_id.empty());

  if (!LazyInit())
    return {};

  // Gets the sites that were marked to clear but haven't been cleared yet for
  // the given `browser_context_id`. Use 0 as the default
  // `browser_contexts_cleared.cleared_at_run` value if the `browser_context_id`
  // does not exist in the browser_contexts_cleared table.
  std::vector<net::SchemefulSite> results;
  static constexpr char kSelectSql[] =
      // clang-format off
      "SELECT p.site FROM browser_context_sites_to_clear p "
      "LEFT JOIN browser_contexts_cleared c ON p.browser_context_id=c.browser_context_id "
      "WHERE p.marked_at_run>COALESCE(c.cleared_at_run,0)"
      "AND p.browser_context_id=?";
  // clang-format on

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSelectSql));
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

base::flat_map<net::SchemefulSite, int64_t>
FirstPartySetsDatabase::FetchAllSitesToClearFilter(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!browser_context_id.empty());

  if (!LazyInit())
    return {};

  std::vector<std::pair<net::SchemefulSite, int64_t>> results;
  static constexpr char kSelectSql[] =
      // clang-format off
      "SELECT site,marked_at_run FROM browser_context_sites_to_clear "
      "WHERE browser_context_id=?";
  // clang-format on

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, browser_context_id);

  while (statement.Step()) {
    absl::optional<net::SchemefulSite> site =
        FirstPartySetParser::CanonicalizeRegisteredDomain(
            statement.ColumnString(0), /*emit_errors=*/false);
    // TODO(crbug/1314039): Invalid sites should be rare case but possible.
    // Consider deleting them from DB.
    if (site.has_value()) {
      results.emplace_back(std::move(site.value()), statement.ColumnInt(1));
    }
  }

  if (!statement.Succeeded())
    return {};

  return results;
}

base::flat_map<net::SchemefulSite, absl::optional<net::FirstPartySetEntry>>
FirstPartySetsDatabase::FetchPolicyModifications(
    const std::string& browser_context_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit())
    return {};

  std::vector<
      std::pair<net::SchemefulSite, absl::optional<net::FirstPartySetEntry>>>
      results;
  static constexpr char kSelectSql[] =
      // clang-format off
      "SELECT site,site_owner FROM policy_modifications "
      "WHERE browser_context_id=?";
  // clang-format on
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, browser_context_id);

  while (statement.Step()) {
    absl::optional<net::SchemefulSite> site =
        FirstPartySetParser::CanonicalizeRegisteredDomain(
            statement.ColumnString(0), /*emit_errors=*/false);

    absl::optional<net::SchemefulSite> maybe_site_owner;
    if (statement.ColumnString(1) != "") {
      maybe_site_owner = FirstPartySetParser::CanonicalizeRegisteredDomain(
          statement.ColumnString(1), /*emit_errors=*/false);
    }

    // TODO(crbug/1314039): Invalid sites should be rare case but possible.
    // Consider deleting them from DB.
    if (site.has_value()) {
      results.emplace_back(
          std::move(site.value()),
          maybe_site_owner.has_value()
              ? absl::make_optional(net::FirstPartySetEntry(
                    maybe_site_owner.value(),
                    // TODO(https://crbug.com/1219656): May change to use the
                    // real site_type and site_index in the future, depending on
                    // the design details. Use kAssociated as default site type
                    // and null site index for now.
                    net::SiteType::kAssociated, absl::nullopt))
              : absl::nullopt);
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
