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

// The name for the implicit/default bucket before V10.
constexpr char kDefaultNamePreV10[] = "default";

// Overwrites the buckets table with the new_buckets table after data has been
// copied from the former into the latter.
bool OverwriteBucketsTableSetUpIndexes(sql::Database* db) {
  // Replace buckets table with new table.
  static constexpr char kDeleteBucketTableSql[] = "DROP TABLE buckets";
  if (!db->Execute(kDeleteBucketTableSql))
    return false;

  static constexpr char kRenameBucketTableSql[] =
      "ALTER TABLE new_buckets RENAME to buckets";
  if (!db->Execute(kRenameBucketTableSql))
    return false;

  // Create indices on new table.
  // clang-format off
  static constexpr char kStorageKeyIndexSql[] =
      "CREATE UNIQUE INDEX buckets_by_storage_key "
        "ON buckets(storage_key, type, name)";
  // clang-format on
  if (!db->Execute(kStorageKeyIndexSql))
    return false;

  static constexpr char kHostIndexSql[] =
      "CREATE INDEX buckets_by_host ON buckets(host, type)";
  if (!db->Execute(kHostIndexSql))
    return false;

  static constexpr char kLastAccessedIndexSql[] =
      "CREATE INDEX buckets_by_last_accessed ON buckets(type, last_accessed)";
  if (!db->Execute(kLastAccessedIndexSql))
    return false;

  static constexpr char kLastModifiedIndexSql[] =
      "CREATE INDEX buckets_by_last_modified ON buckets(type, last_modified)";
  if (!db->Execute(kLastModifiedIndexSql))
    return false;

  static constexpr char kExpirationIndexSql[] =
      "CREATE INDEX buckets_by_expiration ON buckets(expiration)";
  if (!db->Execute(kExpirationIndexSql))
    return false;

  return true;
}

}  // namespace

// static
bool QuotaDatabaseMigrations::UpgradeSchema(QuotaDatabase& quota_database) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_database.sequence_checker_);
  DCHECK_EQ(0, quota_database.db_->transaction_nesting());

  // Reset tables for versions lower than 7 since they are unsupported.
  if (quota_database.meta_table_->GetVersionNumber() < 7) {
    return false;
  }

  if (quota_database.meta_table_->GetVersionNumber() == 7) {
    bool success = MigrateFromVersion7ToVersion8(quota_database);
    RecordMigrationHistogram(/*old_version=*/7, /*new_version=*/8, success);
    if (!success)
      return false;
  }

  if (quota_database.meta_table_->GetVersionNumber() == 8) {
    bool success = MigrateFromVersion8ToVersion9(quota_database);
    RecordMigrationHistogram(/*old_version=*/8, /*new_version=*/9, success);
    if (!success)
      return false;
  }

  if (quota_database.meta_table_->GetVersionNumber() == 9) {
    bool success = MigrateFromVersion9ToVersion10(quota_database);
    RecordMigrationHistogram(/*old_version=*/9, /*new_version=*/10, success);
    if (!success) {
      return false;
    }
  }

  return quota_database.meta_table_->GetVersionNumber() == 10;
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

bool QuotaDatabaseMigrations::MigrateFromVersion7ToVersion8(
    QuotaDatabase& quota_database) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_database.sequence_checker_);

  sql::Database* db = quota_database.db_.get();
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Create new buckets table.
  // clang-format off
  static constexpr char kNewBucketsTableSql[] =
      "CREATE TABLE IF NOT EXISTS new_buckets("
        "id INTEGER PRIMARY KEY, "
        "storage_key TEXT NOT NULL, "
        "host TEXT NOT NULL, "
        "type INTEGER NOT NULL, "
        "name TEXT NOT NULL, "
        "use_count INTEGER NOT NULL, "
        "last_accessed INTEGER NOT NULL, "
        "last_modified INTEGER NOT NULL, "
        "expiration INTEGER NOT NULL, "
        "quota INTEGER NOT NULL)";
  // clang-format on
  if (!db->Execute(kNewBucketsTableSql))
    return false;

  // clang-format off
  static constexpr char kSelectBucketSql[] =
      "SELECT "
          "id,origin,type,name,use_count,last_accessed,last_modified,"
          "expiration, quota "
        "FROM buckets "
        "WHERE id > ? "
        "ORDER BY id "
        "LIMIT 1";
  // clang-format on
  sql::Statement select_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kSelectBucketSql));

  // clang-format off
  static constexpr char kInsertBucketSql[] =
      "INSERT into new_buckets("
          "id,storage_key,host,type,name,use_count,last_accessed,"
          "last_modified,expiration,quota) "
        "VALUES(?,?,?,?,?,?,?,?,?,?)";
  // clang-format on
  sql::Statement insert_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kInsertBucketSql));

  // Transfer bucket data to the new table one at a time with new host data.
  BucketId last_bucket_id(0);
  while (true) {
    select_statement.BindInt64(0, last_bucket_id.value());
    if (!select_statement.Step())
      break;

    // Populate bucket id.
    BucketId bucket_id(select_statement.ColumnInt64(0));
    insert_statement.BindInt64(0, bucket_id.value());

    // Populate storage key and host.
    std::string storage_key_string = select_statement.ColumnString(1);
    insert_statement.BindString(1, storage_key_string);

    std::optional<blink::StorageKey> storage_key =
        blink::StorageKey::Deserialize(storage_key_string);
    const std::string& host = storage_key.has_value()
                                  ? storage_key.value().origin().host()
                                  : std::string();
    insert_statement.BindString(2, host);

    // Populate type, name, use_count, last_accessed, last_modified,
    // expiration and quota.
    insert_statement.BindInt(3, select_statement.ColumnInt(2));
    insert_statement.BindString(4, select_statement.ColumnString(3));
    insert_statement.BindInt(5, select_statement.ColumnInt(4));
    insert_statement.BindTime(6, select_statement.ColumnTime(5));
    insert_statement.BindTime(7, select_statement.ColumnTime(6));
    insert_statement.BindTime(8, select_statement.ColumnTime(7));
    insert_statement.BindInt(9, select_statement.ColumnInt(8));

    if (!insert_statement.Run())
      return false;

    select_statement.Reset(/*clear_bound_vars=*/true);
    insert_statement.Reset(/*clear_bound_vars=*/true);
    last_bucket_id = bucket_id;
  }

  if (!OverwriteBucketsTableSetUpIndexes(db))
    return false;

  // Mark database as up to date.
  return quota_database.meta_table_->SetVersionNumber(8) &&
         quota_database.meta_table_->SetCompatibleVersionNumber(8) &&
         transaction.Commit();
}

bool QuotaDatabaseMigrations::MigrateFromVersion8ToVersion9(
    QuotaDatabase& quota_database) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_database.sequence_checker_);

  sql::Database* db = quota_database.db_.get();
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Create new buckets table.
  // clang-format off
  static constexpr char kNewBucketsTableSql[] =
      "CREATE TABLE IF NOT EXISTS new_buckets("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "storage_key TEXT NOT NULL, "
        "host TEXT NOT NULL, "
        "type INTEGER NOT NULL, "
        "name TEXT NOT NULL, "
        "use_count INTEGER NOT NULL, "
        "last_accessed INTEGER NOT NULL, "
        "last_modified INTEGER NOT NULL, "
        "expiration INTEGER NOT NULL, "
        "quota INTEGER NOT NULL, "
        "persistent INTEGER NOT NULL, "
        "durability INTEGER NOT NULL) "
        "STRICT";
  // clang-format on
  if (!db->Execute(kNewBucketsTableSql))
    return false;

  // clang-format off
  static constexpr char kSelectBucketSql[] =
      "SELECT "
          "id,storage_key,host,type,name,use_count,last_accessed,last_modified,"
          "expiration,quota "
        "FROM buckets "
        "WHERE id > ? "
        "ORDER BY id "
        "LIMIT 1";
  // clang-format on
  sql::Statement select_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kSelectBucketSql));

  // clang-format off
  static constexpr char kInsertBucketSql[] =
      "INSERT into new_buckets("
          "id,storage_key,host,type,name,use_count,last_accessed,last_modified,"
          "expiration,quota,persistent,durability)"
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?)";
  // clang-format on
  sql::Statement insert_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kInsertBucketSql));

  // Transfer bucket data to the new table one at a time with new
  // persistence/durability (default) values.
  BucketId last_bucket_id(0);
  while (true) {
    select_statement.BindInt64(0, last_bucket_id.value());
    if (!select_statement.Step())
      break;

    last_bucket_id = BucketId(select_statement.ColumnInt64(0));

    insert_statement.BindInt64(0, select_statement.ColumnInt64(0));
    insert_statement.BindString(1, select_statement.ColumnString(1));
    insert_statement.BindString(2, select_statement.ColumnString(2));
    insert_statement.BindInt(3, select_statement.ColumnInt(3));
    const std::string bucket_name = select_statement.ColumnString(4);
    insert_statement.BindString(4, bucket_name);
    insert_statement.BindInt(5, select_statement.ColumnInt(5));
    insert_statement.BindTime(6, select_statement.ColumnTime(6));
    insert_statement.BindTime(7, select_statement.ColumnTime(7));
    // Prior to version 9, the default value for `expiration` was
    // base::Time::Max(), and the value was unused. For version 9+, the default
    // value is base::Time(). Reset old values to the new default.
    insert_statement.BindTime(8, base::Time());
    insert_statement.BindInt(9, select_statement.ColumnInt(9));
    insert_statement.BindBool(10, false);
    insert_statement.BindInt(
        11, static_cast<int>(blink::mojom::BucketDurability::kRelaxed));

    if (!insert_statement.Run())
      return false;

    select_statement.Reset(/*clear_bound_vars=*/true);
    insert_statement.Reset(/*clear_bound_vars=*/true);
  }

  if (!OverwriteBucketsTableSetUpIndexes(db))
    return false;

  // Mark database as up to date.
  return quota_database.meta_table_->SetVersionNumber(9) &&
         quota_database.meta_table_->SetCompatibleVersionNumber(9) &&
         transaction.Commit();
}

bool QuotaDatabaseMigrations::MigrateFromVersion9ToVersion10(
    QuotaDatabase& quota_database) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(quota_database.sequence_checker_);

  sql::Database* db = quota_database.db_.get();
  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return false;
  }

  // Update the default bucket name to '_default'.
  // See https://github.com/WICG/storage-buckets/issues/91
  // clang-format off
  static constexpr char kUpdateDefaultBucketNameSql[] =
      "UPDATE buckets "
        "SET name = ? "
        "WHERE name = ? ";
  // clang-format on
  sql::Statement update_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kUpdateDefaultBucketNameSql));
  update_statement.BindString(0, kDefaultBucketName);
  update_statement.BindString(1, kDefaultNamePreV10);
  if (!update_statement.Run()) {
    return false;
  }

  // Delete quota table (see crbug.com/1175113).
  static constexpr char kDeleteQuotaHostTableSql[] = "DROP TABLE quota";
  if (!db->Execute(kDeleteQuotaHostTableSql)) {
    return false;
  }

  // Delete buckets with persistent type (see crbug.com/1175113).
  static constexpr char kDeletePersistentTypeBuckets[] =
      "DELETE FROM buckets WHERE type = ? ";
  sql::Statement delete_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kDeletePersistentTypeBuckets));
  delete_statement.BindInt(
      0, static_cast<int>(blink::mojom::StorageType::kDeprecatedPersistent));
  if (!delete_statement.Run()) {
    return false;
  }

  // Mark database as up to date.
  return quota_database.meta_table_->SetVersionNumber(10) &&
         quota_database.meta_table_->SetCompatibleVersionNumber(10) &&
         transaction.Commit();
}

}  // namespace storage
