// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_database_migrations.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/services/storage/public/cpp/buckets/bucket_id.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "storage/browser/quota/quota_database.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom.h"

namespace storage {

namespace {

// Quota type for deprecated temporary quota.
constexpr int kDeprecatedTemporaryQuotaType = 0;

}  // namespace

// static
bool QuotaDatabaseMigrations::UpgradeSchema(QuotaDatabase& quota_database) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_database.sequence_checker_);
  DCHECK_EQ(0, quota_database.db_->transaction_nesting());

  // Reset tables for versions lower than 10 since they are unsupported.
  if (quota_database.meta_table_->GetVersionNumber() < 10) {
    return false;
  }

  if (quota_database.meta_table_->GetVersionNumber() == 10) {
    bool success = MigrateFromVersion10ToVersion11(quota_database);
    RecordMigrationHistogram(/*old_version=*/10, /*new_version=*/11, success);
    if (!success) {
      return false;
    }
  }

  return quota_database.meta_table_->GetVersionNumber() == 11;
}

void QuotaDatabaseMigrations::RecordMigrationHistogram(int old_version,
                                                       int new_version,
                                                       bool success) {
  base::UmaHistogramBoolean(
      base::StrCat({"Quota.DatabaseMigrationFromV",
                    base::NumberToString(old_version), "ToV",
                    base::NumberToString(new_version)}),
      success);
}

bool QuotaDatabaseMigrations::MigrateFromVersion10ToVersion11(
    QuotaDatabase& quota_database) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_database.sequence_checker_);

  sql::Database* db = quota_database.db_.get();
  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }

  // Delete buckets that aren't of the temporary type (see crbug.com/40211051).
  static constexpr char kDeleteTemporaryTypeBuckets[] =
      "DELETE FROM buckets WHERE type != ? ";
  sql::Statement delete_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeleteTemporaryTypeBuckets));
  delete_statement.BindInt(0, kDeprecatedTemporaryQuotaType);
  if (!delete_statement.Run()) {
    return false;
  }

  // Drop all indices with type (see crbug.com/40211051).
  static constexpr char kDropStorageKeyIndex[] =
      "DROP INDEX buckets_by_storage_key";
  if (!db->Execute(kDropStorageKeyIndex)) {
    return false;
  }
  static constexpr char kDropHostIndex[] = "DROP INDEX buckets_by_host";
  if (!db->Execute(kDropHostIndex)) {
    return false;
  }
  static constexpr char kDropLastAccessedIndex[] =
      "DROP INDEX buckets_by_last_accessed";
  if (!db->Execute(kDropLastAccessedIndex)) {
    return false;
  }
  static constexpr char kDropLastModifiedIndex[] =
      "DROP INDEX buckets_by_last_modified";
  if (!db->Execute(kDropLastModifiedIndex)) {
    return false;
  }

  // Drop type column from buckets table (see crbug.com/40211051).
  static constexpr char kDropTypeColumn[] =
      "ALTER TABLE buckets DROP COLUMN type";
  sql::Statement drop_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDropTypeColumn));
  if (!drop_statement.Run()) {
    return false;
  }

  // Create all dropped indices without type (see crbug.com/40211051).
  static constexpr char kCreateStorageKeyIndex[] =
      "CREATE UNIQUE INDEX buckets_by_storage_key ON buckets(storage_key, "
      "name)";
  if (!db->Execute(kCreateStorageKeyIndex)) {
    return false;
  }
  static constexpr char kCreateHostIndex[] =
      "CREATE INDEX buckets_by_host ON buckets(host)";
  if (!db->Execute(kCreateHostIndex)) {
    return false;
  }
  static constexpr char kCreateLastAccessedIndex[] =
      "CREATE INDEX buckets_by_last_accessed ON buckets(last_accessed)";
  if (!db->Execute(kCreateLastAccessedIndex)) {
    return false;
  }
  static constexpr char kCreateLastModifiedIndex[] =
      "CREATE INDEX buckets_by_last_modified ON buckets(last_modified);";
  if (!db->Execute(kCreateLastModifiedIndex)) {
    return false;
  }

  // Mark database as up to date.
  return quota_database.meta_table_->SetVersionNumber(11) &&
         quota_database.meta_table_->SetCompatibleVersionNumber(11) &&
         transaction.Commit();
}

}  // namespace storage
