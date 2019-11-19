// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_database.h"

#include <stddef.h>
#include <stdint.h>

#include <tuple>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "url/gurl.h"

using blink::mojom::StorageType;

namespace storage {
namespace {

// Definitions for database schema.

const int kQuotaDatabaseCurrentSchemaVersion = 5;
const int kQuotaDatabaseCompatibleVersion = 2;

const char kHostQuotaTable[] = "HostQuotaTable";
const char kOriginInfoTable[] = "OriginInfoTable";
const char kEvictionInfoTable[] = "EvictionInfoTable";
const char kIsOriginTableBootstrapped[] = "IsOriginTableBootstrapped";

bool VerifyValidQuotaConfig(const char* key) {
  return (key != nullptr &&
          (!strcmp(key, QuotaDatabase::kDesiredAvailableSpaceKey) ||
           !strcmp(key, QuotaDatabase::kTemporaryQuotaOverrideKey)));
}

const int kCommitIntervalMs = 30000;

}  // anonymous namespace

// static
const char QuotaDatabase::kDesiredAvailableSpaceKey[] = "DesiredAvailableSpace";
const char QuotaDatabase::kTemporaryQuotaOverrideKey[] =
    "TemporaryQuotaOverride";

const QuotaDatabase::TableSchema QuotaDatabase::kTables[] = {
    {kHostQuotaTable,
     "(host TEXT NOT NULL,"
     " type INTEGER NOT NULL,"
     " quota INTEGER DEFAULT 0,"
     " UNIQUE(host, type))"},
    {kOriginInfoTable,
     "(origin TEXT NOT NULL,"
     " type INTEGER NOT NULL,"
     " used_count INTEGER DEFAULT 0,"
     " last_access_time INTEGER DEFAULT 0,"
     " last_modified_time INTEGER DEFAULT 0,"
     " UNIQUE(origin, type))"},
    {kEvictionInfoTable,
     "(origin TEXT NOT NULL,"
     " type INTEGER NOT NULL,"
     " last_eviction_time INTEGER DEFAULT 0,"
     " UNIQUE(origin, type))"}};

// static
const QuotaDatabase::IndexSchema QuotaDatabase::kIndexes[] = {
  { "HostIndex",
    kHostQuotaTable,
    "(host)",
    false },
  { "OriginInfoIndex",
    kOriginInfoTable,
    "(origin)",
    false },
  { "OriginLastAccessTimeIndex",
    kOriginInfoTable,
    "(last_access_time)",
    false },
  { "OriginLastModifiedTimeIndex",
    kOriginInfoTable,
    "(last_modified_time)",
    false },
};

struct QuotaDatabase::QuotaTableImporter {
  bool Append(const QuotaTableEntry& entry) {
    entries.push_back(entry);
    return true;
  }
  std::vector<QuotaTableEntry> entries;
};

// Clang requires explicit out-of-line constructors for them.
QuotaDatabase::QuotaTableEntry::QuotaTableEntry()
    : type(StorageType::kUnknown), quota(0) {}

QuotaDatabase::QuotaTableEntry::QuotaTableEntry(const std::string& host,
                                                StorageType type,
                                                int64_t quota)
    : host(host), type(type), quota(quota) {}

QuotaDatabase::OriginInfoTableEntry::OriginInfoTableEntry()
    : type(StorageType::kUnknown), used_count(0) {}

QuotaDatabase::OriginInfoTableEntry::OriginInfoTableEntry(
    const url::Origin& origin,
    StorageType type,
    int used_count,
    const base::Time& last_access_time,
    const base::Time& last_modified_time)
    : origin(origin),
      type(type),
      used_count(used_count),
      last_access_time(last_access_time),
      last_modified_time(last_modified_time) {}

// QuotaDatabase ------------------------------------------------------------
QuotaDatabase::QuotaDatabase(const base::FilePath& path)
    : db_file_path_(path),
      is_recreating_(false),
      is_disabled_(false) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

QuotaDatabase::~QuotaDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_) {
    db_->CommitTransaction();
  }
}

void QuotaDatabase::CloseDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  meta_table_.reset();
  db_.reset();
}

bool QuotaDatabase::GetHostQuota(const std::string& host,
                                 StorageType type,
                                 int64_t* quota) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(quota);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT quota"
      " FROM HostQuotaTable"
      " WHERE host = ? AND type = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, host);
  statement.BindInt(1, static_cast<int>(type));

  if (!statement.Step())
    return false;

  *quota = statement.ColumnInt64(0);
  return true;
}

bool QuotaDatabase::SetHostQuota(const std::string& host,
                                 StorageType type,
                                 int64_t quota) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(quota, 0);
  if (!LazyOpen(true))
    return false;
  if (quota == 0)
    return DeleteHostQuota(host, type);
  if (!InsertOrReplaceHostQuota(host, type, quota))
    return false;
  ScheduleCommit();
  return true;
}

bool QuotaDatabase::SetOriginLastAccessTime(const url::Origin& origin,
                                            StorageType type,
                                            base::Time last_access_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(true))
    return false;

  sql::Statement statement;

  OriginInfoTableEntry entry;
  if (GetOriginInfo(origin, type, &entry)) {
    ++entry.used_count;
    const char* kSql =
        "UPDATE OriginInfoTable"
        " SET used_count = ?, last_access_time = ?"
        " WHERE origin = ? AND type = ?";
    statement.Assign(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  } else  {
    entry.used_count = 1;
    const char* kSql =
        "INSERT INTO OriginInfoTable"
        " (used_count, last_access_time, origin, type, last_modified_time)"
        " VALUES (?, ?, ?, ?, ?)";
    statement.Assign(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindInt64(4, TimeToSqlValue(last_access_time));
  }
  statement.BindInt(0, entry.used_count);
  statement.BindInt64(1, TimeToSqlValue(last_access_time));
  statement.BindString(2, origin.GetURL().spec());
  statement.BindInt(3, static_cast<int>(type));

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::SetOriginLastModifiedTime(const url::Origin& origin,
                                              StorageType type,
                                              base::Time last_modified_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(true))
    return false;

  sql::Statement statement;

  OriginInfoTableEntry entry;
  if (GetOriginInfo(origin, type, &entry)) {
    const char* kSql =
        "UPDATE OriginInfoTable"
        " SET last_modified_time = ?"
        " WHERE origin = ? AND type = ?";
    statement.Assign(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  } else {
    const char* kSql =
        "INSERT INTO OriginInfoTable"
        " (last_modified_time, origin, type, last_access_time)  VALUES (?, ?, ?, ?)";
    statement.Assign(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindInt64(3, TimeToSqlValue(last_modified_time));
  }
  statement.BindInt64(0, TimeToSqlValue(last_modified_time));

  statement.BindString(1, origin.GetURL().spec());
  statement.BindInt(2, static_cast<int>(type));

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::GetOriginLastEvictionTime(const url::Origin& origin,
                                              StorageType type,
                                              base::Time* last_modified_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(last_modified_time);
  if (!LazyOpen(false))
    return false;

  static const char kSql[] =
      "SELECT last_eviction_time"
      " FROM EvictionInfoTable"
      " WHERE origin = ? AND type = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, origin.GetURL().spec());
  statement.BindInt(1, static_cast<int>(type));

  if (!statement.Step())
    return false;

  *last_modified_time = TimeFromSqlValue(statement.ColumnInt64(0));
  return true;
}

bool QuotaDatabase::SetOriginLastEvictionTime(const url::Origin& origin,
                                              StorageType type,
                                              base::Time last_modified_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(true))
    return false;

  static const char kSql[] =
      "INSERT OR REPLACE INTO EvictionInfoTable"
      " (last_eviction_time, origin, type)"
      " VALUES (?, ?, ?)";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, TimeToSqlValue(last_modified_time));
  statement.BindString(1, origin.GetURL().spec());
  statement.BindInt(2, static_cast<int>(type));

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::DeleteOriginLastEvictionTime(const url::Origin& origin,
                                                 StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(false))
    return false;

  static const char kSql[] =
      "DELETE FROM EvictionInfoTable"
      " WHERE origin = ? AND type = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, origin.GetURL().spec());
  statement.BindInt(1, static_cast<int>(type));

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::RegisterInitialOriginInfo(
    const std::set<url::Origin>& origins,
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(true))
    return false;

  for (const auto& origin : origins) {
    const char* kSql =
        "INSERT OR IGNORE INTO OriginInfoTable"
        " (origin, type) VALUES (?, ?)";
    sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindString(0, origin.GetURL().spec());
    statement.BindInt(1, static_cast<int>(type));

    if (!statement.Run())
      return false;
  }

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::GetOriginInfo(const url::Origin& origin,
                                  StorageType type,
                                  QuotaDatabase::OriginInfoTableEntry* entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "SELECT * FROM OriginInfoTable"
      " WHERE origin = ? AND type = ?";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, origin.GetURL().spec());
  statement.BindInt(1, static_cast<int>(type));

  if (!statement.Step())
    return false;

  // TODO(crbug.com/889590): Use helper for url::Origin creation from string.
  *entry = OriginInfoTableEntry(
      url::Origin::Create(GURL(statement.ColumnString(0))),
      static_cast<StorageType>(statement.ColumnInt(1)), statement.ColumnInt(2),
      TimeFromSqlValue(statement.ColumnInt64(3)),
      TimeFromSqlValue(statement.ColumnInt64(4)));

  return true;
}

bool QuotaDatabase::DeleteHostQuota(
    const std::string& host, StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "DELETE FROM HostQuotaTable"
      " WHERE host = ? AND type = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, host);
  statement.BindInt(1, static_cast<int>(type));

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::DeleteOriginInfo(const url::Origin& origin,
                                     StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(false))
    return false;

  const char* kSql =
      "DELETE FROM OriginInfoTable"
      " WHERE origin = ? AND type = ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, origin.GetURL().spec());
  statement.BindInt(1, static_cast<int>(type));

  if (!statement.Run())
    return false;

  ScheduleCommit();
  return true;
}

bool QuotaDatabase::GetQuotaConfigValue(const char* key, int64_t* value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(false))
    return false;
  DCHECK(VerifyValidQuotaConfig(key));
  return meta_table_->GetValue(key, value);
}

bool QuotaDatabase::SetQuotaConfigValue(const char* key, int64_t value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(true))
    return false;
  DCHECK(VerifyValidQuotaConfig(key));
  return meta_table_->SetValue(key, value);
}

bool QuotaDatabase::GetLRUOrigin(StorageType type,
                                 const std::set<url::Origin>& exceptions,
                                 SpecialStoragePolicy* special_storage_policy,
                                 base::Optional<url::Origin>* origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(origin);
  if (!LazyOpen(false))
    return false;

  static const char kSql[] =
      "SELECT origin FROM OriginInfoTable"
      " WHERE type = ?"
      " ORDER BY last_access_time ASC";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));

  while (statement.Step()) {
    url::Origin read_origin =
        url::Origin::Create(GURL(statement.ColumnString(0)));
    if (base::Contains(exceptions, read_origin))
      continue;

    if (special_storage_policy &&
        (special_storage_policy->IsStorageDurable(read_origin.GetURL()) ||
         special_storage_policy->IsStorageUnlimited(read_origin.GetURL()))) {
      continue;
    }

    *origin = read_origin;
    return true;
  }

  origin->reset();
  return statement.Succeeded();
}

bool QuotaDatabase::GetOriginsModifiedSince(StorageType type,
                                            std::set<url::Origin>* origins,
                                            base::Time modified_since) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(origins);
  if (!LazyOpen(false))
    return false;

  const char* kSql = "SELECT origin FROM OriginInfoTable"
                     " WHERE type = ? AND last_modified_time >= ?";

  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(type));
  statement.BindInt64(1, TimeToSqlValue(modified_since));

  origins->clear();
  while (statement.Step())
    origins->insert(url::Origin::Create(GURL(statement.ColumnString(0))));

  return statement.Succeeded();
}

bool QuotaDatabase::IsOriginDatabaseBootstrapped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(true))
    return false;

  int flag = 0;
  return meta_table_->GetValue(kIsOriginTableBootstrapped, &flag) && flag;
}

bool QuotaDatabase::SetOriginDatabaseBootstrapped(bool bootstrap_flag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(true))
    return false;

  return meta_table_->SetValue(kIsOriginTableBootstrapped, bootstrap_flag);
}

void QuotaDatabase::Commit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return;

  if (timer_.IsRunning())
    timer_.Stop();

  DCHECK_EQ(1, db_->transaction_nesting());
  db_->CommitTransaction();
  DCHECK_EQ(0, db_->transaction_nesting());
  db_->BeginTransaction();
  DCHECK_EQ(1, db_->transaction_nesting());
}

void QuotaDatabase::ScheduleCommit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (timer_.IsRunning())
    return;
  timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kCommitIntervalMs),
               this, &QuotaDatabase::Commit);
}

bool QuotaDatabase::LazyOpen(bool create_if_needed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_)
    return true;

  // If we tried and failed once, don't try again in the same session
  // to avoid creating an incoherent mess on disk.
  if (is_disabled_)
    return false;

  bool in_memory_only = db_file_path_.empty();
  if (!create_if_needed &&
      (in_memory_only || !base::PathExists(db_file_path_))) {
    return false;
  }

  db_.reset(new sql::Database);
  meta_table_.reset(new sql::MetaTable);

  db_->set_histogram_tag("Quota");

  bool opened = false;
  if (in_memory_only) {
    opened = db_->OpenInMemory();
  } else if (!base::CreateDirectory(db_file_path_.DirName())) {
      LOG(ERROR) << "Failed to create quota database directory.";
  } else {
    opened = db_->Open(db_file_path_);
    if (opened)
      db_->Preload();
  }

  if (!opened || !EnsureDatabaseVersion()) {
    LOG(ERROR) << "Could not open the quota database, resetting.";
    if (!ResetSchema()) {
      LOG(ERROR) << "Failed to reset the quota database.";
      is_disabled_ = true;
      db_.reset();
      meta_table_.reset();
      return false;
    }
  }

  // Start a long-running transaction.
  db_->BeginTransaction();

  return true;
}

bool QuotaDatabase::EnsureDatabaseVersion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  static const size_t kTableCount = base::size(kTables);
  static const size_t kIndexCount = base::size(kIndexes);
  if (!sql::MetaTable::DoesTableExist(db_.get()))
    return CreateSchema(db_.get(), meta_table_.get(),
                        kQuotaDatabaseCurrentSchemaVersion,
                        kQuotaDatabaseCompatibleVersion, kTables, kTableCount,
                        kIndexes, kIndexCount);

  if (!meta_table_->Init(db_.get(), kQuotaDatabaseCurrentSchemaVersion,
                         kQuotaDatabaseCompatibleVersion))
    return false;

  if (meta_table_->GetCompatibleVersionNumber() >
      kQuotaDatabaseCurrentSchemaVersion) {
    LOG(WARNING) << "Quota database is too new.";
    return false;
  }

  if (meta_table_->GetVersionNumber() < kQuotaDatabaseCurrentSchemaVersion) {
    if (!UpgradeSchema(meta_table_->GetVersionNumber()))
      return ResetSchema();
  }

#ifndef NDEBUG
  DCHECK(sql::MetaTable::DoesTableExist(db_.get()));
  for (size_t i = 0; i < kTableCount; ++i) {
    DCHECK(db_->DoesTableExist(kTables[i].table_name));
  }
#endif

  return true;
}

// static
bool QuotaDatabase::CreateSchema(sql::Database* database,
                                 sql::MetaTable* meta_table,
                                 int schema_version,
                                 int compatible_version,
                                 const TableSchema* tables,
                                 size_t tables_size,
                                 const IndexSchema* indexes,
                                 size_t indexes_size) {
  // TODO(kinuko): Factor out the common code to create databases.
  sql::Transaction transaction(database);
  if (!transaction.Begin())
    return false;

  if (!meta_table->Init(database, schema_version, compatible_version))
    return false;

  for (size_t i = 0; i < tables_size; ++i) {
    std::string sql("CREATE TABLE ");
    sql += tables[i].table_name;
    sql += tables[i].columns;
    if (!database->Execute(sql.c_str())) {
      VLOG(1) << "Failed to execute " << sql;
      return false;
    }
  }

  for (size_t i = 0; i < indexes_size; ++i) {
    std::string sql;
    if (indexes[i].unique)
      sql += "CREATE UNIQUE INDEX ";
    else
      sql += "CREATE INDEX ";
    sql += indexes[i].index_name;
    sql += " ON ";
    sql += indexes[i].table_name;
    sql += indexes[i].columns;
    if (!database->Execute(sql.c_str())) {
      VLOG(1) << "Failed to execute " << sql;
      return false;
    }
  }

  return transaction.Commit();
}

bool QuotaDatabase::ResetSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!db_file_path_.empty());
  DCHECK(base::PathExists(db_file_path_));
  DCHECK(!db_ || !db_->transaction_nesting());
  VLOG(1) << "Deleting existing quota data and starting over.";

  db_.reset();
  meta_table_.reset();

  if (!sql::Database::Delete(db_file_path_))
    return false;

  // So we can't go recursive.
  if (is_recreating_)
    return false;

  base::AutoReset<bool> auto_reset(&is_recreating_, true);
  return LazyOpen(true);
}

bool QuotaDatabase::UpgradeSchema(int current_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(0, db_->transaction_nesting());

  if (current_version == 2) {
    QuotaTableImporter importer;
    if (!DumpQuotaTable(base::BindRepeating(&QuotaTableImporter::Append,
                                            base::Unretained(&importer)))) {
      return false;
    }
    ResetSchema();

    sql::Transaction transaction(db_.get());
    if (!transaction.Begin())
      return false;
    for (const auto& entry : importer.entries) {
      if (!InsertOrReplaceHostQuota(entry.host, entry.type, entry.quota))
        return false;
    }
    return transaction.Commit();
  } else if (current_version < 5) {
    sql::Transaction transaction(db_.get());
    if (!transaction.Begin())
      return false;

    const QuotaDatabase::TableSchema& eviction_table_schema = kTables[2];
    DCHECK_EQ(strcmp(kEvictionInfoTable, eviction_table_schema.table_name), 0);

    std::string sql("CREATE TABLE ");
    sql += eviction_table_schema.table_name;
    sql += eviction_table_schema.columns;
    if (!db_->Execute(sql.c_str())) {
      VLOG(1) << "Failed to execute " << sql;
      return false;
    }

    meta_table_->SetVersionNumber(5);
    return transaction.Commit();
  }
  return false;
}

bool QuotaDatabase::InsertOrReplaceHostQuota(const std::string& host,
                                             StorageType type,
                                             int64_t quota) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_.get());
  const char* kSql =
      "INSERT OR REPLACE INTO HostQuotaTable"
      " (quota, host, type)"
      " VALUES (?, ?, ?)";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, quota);
  statement.BindString(1, host);
  statement.BindInt(2, static_cast<int>(type));
  return statement.Run();
}

bool QuotaDatabase::DumpQuotaTable(const QuotaTableCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyOpen(true))
    return false;

  const char* kSql = "SELECT * FROM HostQuotaTable";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));

  while (statement.Step()) {
    QuotaTableEntry entry = QuotaTableEntry(
      statement.ColumnString(0),
      static_cast<StorageType>(statement.ColumnInt(1)),
      statement.ColumnInt64(2));

    if (!callback.Run(entry))
      return true;
  }

  return statement.Succeeded();
}

bool QuotaDatabase::DumpOriginInfoTable(
    const OriginInfoTableCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyOpen(true))
    return false;

  const char* kSql = "SELECT * FROM OriginInfoTable";
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE, kSql));

  while (statement.Step()) {
    OriginInfoTableEntry entry(
        url::Origin::Create(GURL(statement.ColumnString(0))),
        static_cast<StorageType>(statement.ColumnInt(1)),
        statement.ColumnInt(2), TimeFromSqlValue(statement.ColumnInt64(3)),
        TimeFromSqlValue(statement.ColumnInt64(4)));

    if (!callback.Run(entry))
      return true;
  }

  return statement.Succeeded();
}

// static
base::Time QuotaDatabase::TimeFromSqlValue(int64_t time) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(time));
}

// static
int64_t QuotaDatabase::TimeToSqlValue(const base::Time& time) {
  return time.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

bool operator<(const QuotaDatabase::QuotaTableEntry& lhs,
               const QuotaDatabase::QuotaTableEntry& rhs) {
  return std::tie(lhs.host, lhs.type, lhs.quota) <
         std::tie(rhs.host, rhs.type, rhs.quota);
}

bool operator<(const QuotaDatabase::OriginInfoTableEntry& lhs,
               const QuotaDatabase::OriginInfoTableEntry& rhs) {
  return std::tie(lhs.origin, lhs.type, lhs.used_count, lhs.last_access_time) <
         std::tie(rhs.origin, rhs.type, rhs.used_count, rhs.last_access_time);
}

}  // namespace storage
