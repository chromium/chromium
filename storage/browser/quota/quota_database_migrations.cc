// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_database_migrations.h"

#include <string>

#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "storage/browser/quota/quota_database.h"

namespace storage {

// static
bool QuotaDatabaseMigrations::UpgradeSchema(QuotaDatabase& quota_database) {
  DCHECK_EQ(0, quota_database.db_->transaction_nesting());

  // Reset tables for versions lower than 5 since they are unsupported.
  if (quota_database.meta_table_->GetVersionNumber() < 5)
    return quota_database.ResetSchema();

  if (quota_database.meta_table_->GetVersionNumber() == 5) {
    if (!MigrateToVersion6(quota_database))
      return false;
  }

  return quota_database.meta_table_->GetVersionNumber() == 6;
}

bool QuotaDatabaseMigrations::MigrateToVersion6(QuotaDatabase& quota_database) {
  sql::Database* db = quota_database.db_.get();
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Create tables with latest schema.
  for (size_t i = 0; i < QuotaDatabase::kTableCount; ++i) {
    if (!quota_database.CreateTable(QuotaDatabase::kTables[i]))
      return false;
  }

  // Create all indexes for tables.
  for (size_t i = 0; i < QuotaDatabase::kIndexCount; ++i) {
    if (!quota_database.CreateIndex(QuotaDatabase::kIndexes[i]))
      return false;
  }

  // Copy OriginInfoTable data into new bucket table.
  const char kImportOriginInfoSql[] =
      // clang-format off
      "INSERT INTO buckets("
          "origin,"
          "type,"
          "name,"
          "use_count,"
          "last_accessed,"
          "last_modified,"
          "expiration,"
          "quota) "
        "SELECT "
          "origin,"
          "type,"
          "?,"
          "used_count,"
          "last_access_time,"
          "last_modified_time,"
          "?,"
          "0 "
        "FROM OriginInfoTable";
  // clang-format on
  sql::Statement import_origin_info_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kImportOriginInfoSql));
  import_origin_info_statement.BindString(0, QuotaDatabase::kDefaultBucket);
  import_origin_info_statement.BindTime(1, base::Time::Max());
  if (!import_origin_info_statement.Run())
    return false;

  // Delete OriginInfoTable.
  const char kDeleteOriginInfoTableSql[] = "DROP TABLE OriginInfoTable";
  if (!db->Execute(kDeleteOriginInfoTableSql))
    return false;

  // Copy HostQuotaTable data into the new quota table.
  const char kImportQuotaSql[] =
      // clang-format off
      "INSERT INTO quota(host, type, quota) "
        "SELECT host, type, quota "
        "FROM HostQuotaTable";
  // clang-format on
  sql::Statement import_quota_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kImportQuotaSql));
  if (!import_quota_statement.Run())
    return false;

  // Delete HostQuotaTable.
  const char kDeleteQuotaHostTableSql[] = "DROP TABLE HostQuotaTable";
  if (!db->Execute(kDeleteQuotaHostTableSql))
    return false;

  // Copy EvictionInfoTable data into the new eviction_info table.
  const char kImportEvictionInfoSql[] =
      // clang-format off
      "INSERT INTO eviction_info(origin, type, last_eviction_time) "
        "SELECT origin, type, last_eviction_time "
        "FROM EvictionInfoTable";
  // clang-format on
  sql::Statement import_eviction_info_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kImportEvictionInfoSql));
  if (!import_eviction_info_statement.Run())
    return false;

  // Delete EvictionInfoTable.
  const char kDeleteEvictionInfoTableSql[] = "DROP TABLE EvictionInfoTable";
  if (!db->Execute(kDeleteEvictionInfoTableSql))
    return false;

  quota_database.meta_table_->SetVersionNumber(6);
  return transaction.Commit();
}

}  // namespace storage
