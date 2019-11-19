// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_database.h"

#include <algorithm>

#include "base/stl_util.h"
#include "sql/transaction.h"

// Current version number.  Note: when changing the current version number,
// corresponding changes must happen in the unit tests, and new migration test
// added.  See |WebDatabaseMigrationTest::kCurrentTestedVersionNumber|.
// static
const int WebDatabase::kCurrentVersionNumber = 82;

const int WebDatabase::kDeprecatedVersionNumber = 51;

const base::FilePath::CharType WebDatabase::kInMemoryPath[] =
    FILE_PATH_LITERAL(":memory");

namespace {

const int kCompatibleVersionNumber = 79;

// Change the version number and possibly the compatibility version of
// |meta_table_|.
void ChangeVersion(sql::MetaTable* meta_table,
                   int version_num,
                   bool update_compatible_version_num) {
  meta_table->SetVersionNumber(version_num);
  if (update_compatible_version_num) {
    meta_table->SetCompatibleVersionNumber(
        std::min(version_num, kCompatibleVersionNumber));
  }
}

// Outputs the failed version number as a warning and always returns
// |sql::INIT_FAILURE|.
sql::InitStatus FailedMigrationTo(int version_num) {
  LOG(WARNING) << "Unable to update web database to version " << version_num
               << ".";
  NOTREACHED();
  return sql::INIT_FAILURE;
}

}  // namespace

WebDatabase::WebDatabase() {}

WebDatabase::~WebDatabase() {}

void WebDatabase::AddTable(WebDatabaseTable* table) {
  tables_[table->GetTypeKey()] = table;
}

WebDatabaseTable* WebDatabase::GetTable(WebDatabaseTable::TypeKey key) {
  return tables_[key];
}

void WebDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void WebDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

std::string WebDatabase::GetDiagnosticInfo(int extended_error,
                                           sql::Statement* statement) {
  return db_.GetDiagnosticInfo(extended_error, statement);
}

sql::Database* WebDatabase::GetSQLConnection() {
  return &db_;
}

sql::InitStatus WebDatabase::Init(const base::FilePath& db_name) {
  db_.set_histogram_tag("Web");

  // We don't store that much data in the tables so use a small page size.
  // This provides a large benefit for empty tables (which is very likely with
  // the tables we create).
  db_.set_page_size(2048);

  // We shouldn't have much data and what access we currently have is quite
  // infrequent. So we go with a small cache size.
  db_.set_cache_size(32);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  db_.set_exclusive_locking();

  if ((db_name.value() == kInMemoryPath) ? !db_.OpenInMemory()
                                         : !db_.Open(db_name)) {
    return sql::INIT_FAILURE;
  }

  // Clobber really old databases.
  static_assert(kDeprecatedVersionNumber < kCurrentVersionNumber,
                "Deprecation version must be less than current");
  sql::MetaTable::RazeIfDeprecated(&db_, kDeprecatedVersionNumber);

  // Scope initialization in a transaction so we can't be partially
  // initialized.
  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return sql::INIT_FAILURE;

  // Version check.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber, kCompatibleVersionNumber))
    return sql::INIT_FAILURE;
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Web database is too new.";
    return sql::INIT_TOO_NEW;
  }

  // Initialize the tables.
  for (auto it = tables_.begin(); it != tables_.end(); ++it) {
    it->second->Init(&db_, &meta_table_);
  }

  // If the file on disk is an older database version, bring it up to date.
  // If the migration fails we return an error to caller and do not commit
  // the migration.
  sql::InitStatus migration_status = MigrateOldVersionsAsNeeded();
  if (migration_status != sql::INIT_OK)
    return migration_status;

  // Create the desired SQL tables if they do not already exist.
  // It's important that this happen *after* the migration code runs.
  // Otherwise, the migration code would have to explicitly check for empty
  // tables created in the new format, and skip the migration in that case.
  for (auto it = tables_.begin(); it != tables_.end(); ++it) {
    if (!it->second->CreateTablesIfNecessary()) {
      LOG(WARNING) << "Unable to initialize the web database.";
      return sql::INIT_FAILURE;
    }
  }

  return transaction.Commit() ? sql::INIT_OK : sql::INIT_FAILURE;
}

sql::InitStatus WebDatabase::MigrateOldVersionsAsNeeded() {
  // Some malware used to lower the version number, causing migration to
  // fail. Ensure the version number is at least as high as the compatible
  // version number.
  int current_version = std::max(meta_table_.GetVersionNumber(),
                                 meta_table_.GetCompatibleVersionNumber());
  if (current_version > meta_table_.GetVersionNumber())
    ChangeVersion(&meta_table_, current_version, false);

  DCHECK_GT(current_version, kDeprecatedVersionNumber);

  for (int next_version = current_version + 1;
       next_version <= kCurrentVersionNumber; ++next_version) {
    // Do any database-wide migrations.
    bool update_compatible_version = false;
    if (!MigrateToVersion(next_version, &update_compatible_version))
      return FailedMigrationTo(next_version);

    ChangeVersion(&meta_table_, next_version, update_compatible_version);

    // Give each table a chance to migrate to this version.
    for (auto it = tables_.begin(); it != tables_.end(); ++it) {
      // Any of the tables may set this to true, but by default it is false.
      update_compatible_version = false;
      if (!it->second->MigrateToVersion(next_version,
                                        &update_compatible_version)) {
        return FailedMigrationTo(next_version);
      }

      ChangeVersion(&meta_table_, next_version, update_compatible_version);
    }
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
