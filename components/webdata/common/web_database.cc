// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_database.h"

#include <algorithm>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "sql/transaction.h"

const base::FilePath::CharType WebDatabase::kInMemoryPath[] =
    FILE_PATH_LITERAL(":memory");

namespace {

// Limits the duration of transaction to the scope of their modifications. Avoid
// keeping pending transactions and pending modifications outside of their
// scope.
//
// TODO(6175955): When this is launched, replace
// `WebDatabase::AcquireTransaction()` with the typical pattern:
//     sql::Transaction transaction(db());
//     if (!transaction.Begin()) {...}
BASE_FEATURE(kSqlScopedTransactionWebDatabase,
             "SqlScopedTransactionWebDatabase",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSqlWALModeOnWebDatabase,
             "SqlWALModeOnWebDatabase",
             base::FEATURE_DISABLED_BY_DEFAULT);

// These values are logged as histogram buckets and most not be changed nor
// reused.
enum class WebDatabaseInitResult {
  kSuccess = 0,
  kCouldNotOpen = 1,
  kDatabaseLocked = 2,
  kCouldNotRazeIncompatibleVersion = 3,
  kFailedToBeginInitTransaction = 4,
  kMetaTableInitFailed = 5,
  kCurrentVersionTooNew = 6,
  kMigrationError = 7,
  kFailedToCreateTable = 8,
  kFailedToCommitInitTransaction = 9,
  kMaxValue = kFailedToCommitInitTransaction
};

void LogInitResult(WebDatabaseInitResult result) {
  base::UmaHistogramEnumeration("WebDatabase.InitResult", result);
}

// Version 139 migrates valuables tables to a new format, changing the column
// names. It is thus is no longer compatible with version 138.
constexpr int kCompatibleVersionNumber = 139;

// Change the version number and possibly the compatibility version of
// |meta_table_|.
[[nodiscard]] bool ChangeVersion(sql::MetaTable* meta_table,
                                 int version_num,
                                 bool update_compatible_version_num) {
  return meta_table->SetVersionNumber(version_num) &&
         (!update_compatible_version_num ||
          meta_table->SetCompatibleVersionNumber(
              std::min(version_num, kCompatibleVersionNumber)));
}

// Outputs the failed version number as a warning and always returns
// |sql::INIT_FAILURE|.
sql::InitStatus FailedMigrationTo(int version_num) {
  LOG(WARNING) << "Unable to update web database to version " << version_num
               << ".";
  base::UmaHistogramExactLinear("WebDatabase.FailedMigrationToVersion",
                                version_num,
                                WebDatabase::kCurrentVersionNumber + 1);
  LogInitResult(WebDatabaseInitResult::kMigrationError);
  return sql::INIT_FAILURE;
}

}  // namespace

WebDatabase::WebDatabase()
    : db_(sql::DatabaseOptions()
              .set_wal_mode(
                  base::FeatureList::IsEnabled(kSqlWALModeOnWebDatabase))
              // We don't store that much data in the tables so use a small page
              // size. This provides a large benefit for empty tables (which is
              // very likely with the tables we create).
              .set_page_size(2048)
              // We shouldn't have much data and what access we currently have
              // is quite infrequent. So we go with a small cache size.
              .set_cache_size(32),
          /*tag=*/"Web"),
      use_scoped_transaction_(
          base::FeatureList::IsEnabled(kSqlScopedTransactionWebDatabase)) {}

WebDatabase::~WebDatabase() {
  for (auto& [key, table] : tables_) {
    table->Shutdown();
  }
}

void WebDatabase::AddTable(WebDatabaseTable* table) {
  tables_[table->GetTypeKey()] = table;
}

WebDatabaseTable* WebDatabase::GetTable(WebDatabaseTable::TypeKey key) {
  WebDatabaseTable* table = tables_[key];
  CHECK(table);
  return table;
}

void WebDatabase::BeginTransaction() {
  if (!use_scoped_transaction_) {
    db_.BeginTransactionDeprecated();
  }
}

void WebDatabase::CommitTransaction() {
  if (!use_scoped_transaction_) {
    db_.CommitTransactionDeprecated();
  }
}

std::unique_ptr<sql::Transaction> WebDatabase::AcquireTransaction() {
  if (use_scoped_transaction_) {
    // Only one active transaction at the time is allowed.
    DCHECK(!db_.HasActiveTransactions());
    auto transaction = std::make_unique<sql::Transaction>(&db_);
    if (transaction->Begin()) {
      return transaction;
    }
  }

  return nullptr;
}

std::string WebDatabase::GetDiagnosticInfo(int extended_error,
                                           sql::Statement* statement) {
  return db_.GetDiagnosticInfo(extended_error, statement);
}

sql::Database* WebDatabase::GetSQLConnection() {
  return &db_;
}

sql::InitStatus WebDatabase::Init(const base::FilePath& db_name,
                                  const os_crypt_async::Encryptor* encryptor) {
  // Only unit tests whose tables don't use any crypto for their tables pass in
  // a null encryptor.
  if (!encryptor) {
    CHECK_IS_TEST();
  }

  if ((db_name.value() == kInMemoryPath) ? !db_.OpenInMemory()
                                         : !db_.Open(db_name)) {
    LogInitResult(WebDatabaseInitResult::kCouldNotOpen);
    return sql::INIT_FAILURE;
  }
  DCHECK(db_.is_open());

  // Dummy transaction to check whether the database is writeable and bail
  // early if that's not the case.
  if (!db_.Execute("BEGIN EXCLUSIVE") || !db_.Execute("COMMIT")) {
    LogInitResult(WebDatabaseInitResult::kDatabaseLocked);
    return sql::INIT_FAILURE;
  }

  // Clobber really old databases.
  static_assert(kDeprecatedVersionNumber < kCurrentVersionNumber,
                "Deprecation version must be less than current");
  if (sql::MetaTable::RazeIfIncompatible(
          &db_, /*lowest_supported_version=*/kDeprecatedVersionNumber + 1,
          kCurrentVersionNumber) == sql::RazeIfIncompatibleResult::kFailed) {
    LogInitResult(WebDatabaseInitResult::kCouldNotRazeIncompatibleVersion);
    return sql::INIT_FAILURE;
  }

  // Scope initialization in a transaction so we can't be partially
  // initialized.
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    LogInitResult(WebDatabaseInitResult::kFailedToBeginInitTransaction);
    return sql::INIT_FAILURE;
  }

  // Version check.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    LogInitResult(WebDatabaseInitResult::kMetaTableInitFailed);
    return sql::INIT_FAILURE;
  }
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LogInitResult(WebDatabaseInitResult::kCurrentVersionTooNew);
    LOG(WARNING) << "Web database is too new.";
    return sql::INIT_TOO_NEW;
  }

  // Initialize the tables.
  for (const auto& table : tables_) {
    table.second->Init(&db_, &meta_table_, encryptor);
  }

  // If the file on disk is an older database version, bring it up to date.
  // If the migration fails we return an error to caller and do not commit
  // the migration.
  sql::InitStatus migration_status = MigrateOldVersionsAsNeeded();
  if (migration_status != sql::INIT_OK) {
    return migration_status;
  }

  // Create the desired SQL tables if they do not already exist.
  // It's important that this happen *after* the migration code runs.
  // Otherwise, the migration code would have to explicitly check for empty
  // tables created in the new format, and skip the migration in that case.
  for (const auto& table : tables_) {
    if (!table.second->CreateTablesIfNecessary()) {
      LOG(WARNING) << "Unable to initialize the web database.";
      LogInitResult(WebDatabaseInitResult::kFailedToCreateTable);
      return sql::INIT_FAILURE;
    }
  }

  bool result = transaction.Commit();
  if (!result) {
    LogInitResult(WebDatabaseInitResult::kFailedToCommitInitTransaction);
    return sql::INIT_FAILURE;
  }

  LogInitResult(WebDatabaseInitResult::kSuccess);
  DCHECK(db_.is_open());
  return sql::INIT_OK;
}

sql::InitStatus WebDatabase::MigrateOldVersionsAsNeeded() {
  // Some malware used to lower the version number, causing migration to
  // fail. Ensure the version number is at least as high as the compatible
  // version number.
  int current_version = std::max(meta_table_.GetVersionNumber(),
                                 meta_table_.GetCompatibleVersionNumber());
  if (current_version > meta_table_.GetVersionNumber() &&
      !ChangeVersion(&meta_table_, current_version, false)) {
    return FailedMigrationTo(current_version);
  }

  DCHECK_GT(current_version, kDeprecatedVersionNumber);

  for (int next_version = current_version + 1;
       next_version <= kCurrentVersionNumber; ++next_version) {
    // Do any database-wide migrations.
    bool update_compatible_version = false;
    if (!MigrateToVersion(next_version, &update_compatible_version) ||
        !ChangeVersion(&meta_table_, next_version, update_compatible_version)) {
      return FailedMigrationTo(next_version);
    }

    // Give each table a chance to migrate to this version.
    for (const auto& table : tables_) {
      // Any of the tables may set this to true, but by default it is false.
      update_compatible_version = false;
      if (!table.second->MigrateToVersion(next_version,
                                          &update_compatible_version) ||
          !ChangeVersion(&meta_table_, next_version,
                         update_compatible_version)) {
        return FailedMigrationTo(next_version);
      }
    }
    base::UmaHistogramExactLinear("WebDatabase.SucceededMigrationToVersion",
                                  next_version,
                                  WebDatabase::kCurrentVersionNumber + 1);
  }
  return sql::INIT_OK;
}

bool WebDatabase::MigrateToVersion(int version,
                                   bool* update_compatible_version) {
  // Migrate if necessary.
  switch (version) {
    case 58:
      *update_compatible_version = true;
      return MigrateToVersion58DropWebAppsAndIntents();
    case 79:
      *update_compatible_version = true;
      return MigrateToVersion79DropLoginsTable();
    case 105:
      *update_compatible_version = true;
      return MigrateToVersion105DropIbansTable();
  }

  return true;
}

bool WebDatabase::MigrateToVersion58DropWebAppsAndIntents() {
  sql::Transaction transaction(&db_);
  return transaction.Begin() && db_.Execute("DROP TABLE IF EXISTS web_apps") &&
         db_.Execute("DROP TABLE IF EXISTS web_app_icons") &&
         db_.Execute("DROP TABLE IF EXISTS web_intents") &&
         db_.Execute("DROP TABLE IF EXISTS web_intents_defaults") &&
         transaction.Commit();
}

bool WebDatabase::MigrateToVersion79DropLoginsTable() {
  sql::Transaction transaction(&db_);
  return transaction.Begin() &&
         db_.Execute("DROP TABLE IF EXISTS ie7_logins") &&
         db_.Execute("DROP TABLE IF EXISTS logins") && transaction.Commit();
}

bool WebDatabase::MigrateToVersion105DropIbansTable() {
  return db_.Execute("DROP TABLE IF EXISTS ibans");
}
