// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage_sql_migrations.h"

#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_storage_sql.h"
#include "content/browser/conversions/sql_utils.h"
#include "content/browser/conversions/storable_impression.h"
#include "net/base/schemeful_site.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/origin.h"

namespace content {

namespace {

struct ImpressionIdAndConversionOrigin {
  int64_t impression_id;
  url::Origin conversion_origin;
};

std::vector<ImpressionIdAndConversionOrigin>
GetImpressionIdAndConversionOrigins(sql::Database* db,
                                    int64_t start_impression_id) {
  const char kGetImpressionsSql[] =
      "SELECT impression_id, conversion_origin "
      "FROM impressions "
      "WHERE impression_id >= ? "
      "ORDER BY impression_id "
      "LIMIT ?";

  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kGetImpressionsSql));
  statement.BindInt64(0, start_impression_id);

  const int kNumImpressions = 100;
  statement.BindInt(1, kNumImpressions);

  std::vector<ImpressionIdAndConversionOrigin> impressions;
  while (statement.Step()) {
    int64_t impression_id = statement.ColumnInt64(0);
    url::Origin conversion_origin =
        DeserializeOrigin(statement.ColumnString(1));

    impressions.push_back({impression_id, std::move(conversion_origin)});
  }
  if (!statement.Succeeded())
    return {};
  return impressions;
}

struct ImpressionIdAndImpressionOrigin {
  int64_t impression_id;
  url::Origin impression_origin;
};

std::vector<ImpressionIdAndImpressionOrigin>
GetImpressionIdAndImpressionOrigins(sql::Database* db,
                                    int64_t start_impression_id) {
  const char kGetImpressionsSql[] =
      "SELECT impression_id, impression_origin "
      "FROM impressions "
      "WHERE impression_id >= ? "
      "ORDER BY impression_id "
      "LIMIT ?";

  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kGetImpressionsSql));
  statement.BindInt64(0, start_impression_id);

  const int kNumImpressions = 100;
  statement.BindInt(1, kNumImpressions);

  std::vector<ImpressionIdAndImpressionOrigin> impressions;
  while (statement.Step()) {
    int64_t impression_id = statement.ColumnInt64(0);
    url::Origin impression_origin =
        DeserializeOrigin(statement.ColumnString(1));

    impressions.push_back({impression_id, std::move(impression_origin)});
  }
  if (!statement.Succeeded())
    return {};
  return impressions;
}

}  // namespace

bool ConversionStorageSqlMigrations::UpgradeSchema(
    ConversionStorageSql* conversion_storage,
    sql::Database* db,
    sql::MetaTable* meta_table) {
  base::ThreadTicks start_timestamp = base::ThreadTicks::Now();

  if (meta_table->GetVersionNumber() == 1) {
    if (!MigrateToVersion2(conversion_storage, db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 2) {
    if (!MigrateToVersion3(conversion_storage, db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 3) {
    if (!MigrateToVersion4(conversion_storage, db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 4) {
    if (!MigrateToVersion5(conversion_storage, db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 5) {
    if (!MigrateToVersion6(conversion_storage, db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 6) {
    if (!MigrateToVersion7(conversion_storage, db, meta_table))
      return false;
  }
  // Add similar if () blocks for new versions here.

  base::UmaHistogramMediumTimes("Conversions.Storage.MigrationTime",
                                base::ThreadTicks::Now() - start_timestamp);
  return true;
}

bool ConversionStorageSqlMigrations::MigrateToVersion2(
    ConversionStorageSql* conversion_storage,
    sql::Database* db,
    sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. This results in smaller
  // transactions, so it's less likely that a transaction's buffer will need to
  // spill to disk. Also, if the database grows a lot and Chrome stops (user
  // quit, process kill, etc.) during the migration process, per-migration
  // transactions make it more likely that we'll make forward progress each time
  // Chrome stops.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Add a new conversion_destination column to the impressions table. This
  // follows the steps documented at
  // https://sqlite.org/lang_altertable.html#otheralter. Other approaches, like
  // using "ALTER ... ADD COLUMN" require setting a DEFAULT value for the column
  // which is undesirable.
  const char kNewImpressionTableSql[] =
      "CREATE TABLE IF NOT EXISTS new_impressions"
      "(impression_id INTEGER PRIMARY KEY,"
      "impression_data TEXT NOT NULL,"
      "impression_origin TEXT NOT NULL,"
      "conversion_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "impression_time INTEGER NOT NULL,"
      "expiry_time INTEGER NOT NULL,"
      "num_conversions INTEGER DEFAULT 0,"
      "active INTEGER DEFAULT 1,"
      "conversion_destination TEXT NOT NULL)";
  if (!db->Execute(kNewImpressionTableSql))
    return false;

  // Transfer the existing rows to the new table, inserting a placeholder for
  // the conversion_destination column.
  const char kPopulateNewImpressionTableSql[] =
      "INSERT INTO new_impressions SELECT "
      "impression_id,impression_data,impression_origin,"
      "conversion_origin,reporting_origin,impression_time,"
      "expiry_time,num_conversions,active,'' "
      "FROM impressions";
  if (!db->Execute(kPopulateNewImpressionTableSql))
    return false;

  const char kDropOldImpressionTableSql[] = "DROP TABLE impressions";
  if (!db->Execute(kDropOldImpressionTableSql))
    return false;

  const char kRenameImpressionTableSql[] =
      "ALTER TABLE new_impressions RENAME TO impressions";
  if (!db->Execute(kRenameImpressionTableSql))
    return false;

  // Update each of the impression rows to have the correct associated
  // conversion_destination. This is only relevant for active impressions, as
  // the column is only used for matching impressions to conversions, but we
  // update all impressions regardless.
  //
  // We update a subset of rows at a time to avoid pulling the entire
  // impressions table into memory.
  std::vector<ImpressionIdAndConversionOrigin> impressions =
      GetImpressionIdAndConversionOrigins(db, /*start_impression_id=*/0);

  const char kUpdateDestinationSql[] =
      "UPDATE impressions SET conversion_destination = ? WHERE impression_id = "
      "?";
  sql::Statement update_destination_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kUpdateDestinationSql));

  while (!impressions.empty()) {
    // Perform the column updates for each row we pulled into memory.
    for (const auto& impression : impressions) {
      update_destination_statement.Reset(/*clear_bound_vars=*/true);

      // The conversion destination is derived from the conversion origin
      // dynamically.
      update_destination_statement.BindString(
          0, net::SchemefulSite(impression.conversion_origin).Serialize());
      update_destination_statement.BindInt64(1, impression.impression_id);
      update_destination_statement.Run();
    }

    // Fetch the next batch of rows from the database.
    impressions = GetImpressionIdAndConversionOrigins(
        db, impressions.back().impression_id + 1);
  }

  // Create the pre-existing impression table indices on the new table.
  const char kImpressionExpiryIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_expiry_idx "
      "ON impressions(expiry_time)";
  if (!db->Execute(kImpressionExpiryIndexSql))
    return false;

  const char kImpressionOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_origin_idx "
      "ON impressions(impression_origin)";
  if (!db->Execute(kImpressionOriginIndexSql))
    return false;

  // Replace the pre-existing conversion_origin_idx with an index that uses the
  // conversion destination, as attribution logic now depends on the
  // conversion_destination.
  const char kConversionDestinationIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_destination_idx "
      "ON impressions(active, conversion_destination, reporting_origin)";
  if (!db->Execute(kConversionDestinationIndexSql))
    return false;

  meta_table->SetVersionNumber(2);
  return transaction.Commit();
}

bool ConversionStorageSqlMigrations::MigrateToVersion3(
    ConversionStorageSql* conversion_storage,
    sql::Database* db,
    sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Add new source_type and attributed_truthfully columns to the impressions
  // table. This follows the steps documented at
  // https://sqlite.org/lang_altertable.html#otheralter. Other approaches, like
  // using "ALTER ... ADD COLUMN" require setting a DEFAULT value for the column
  // which is undesirable.
  const char kNewImpressionTableSql[] =
      "CREATE TABLE IF NOT EXISTS new_impressions"
      "(impression_id INTEGER PRIMARY KEY,"
      "impression_data TEXT NOT NULL,"
      "impression_origin TEXT NOT NULL,"
      "conversion_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "impression_time INTEGER NOT NULL,"
      "expiry_time INTEGER NOT NULL,"
      "num_conversions INTEGER DEFAULT 0,"
      "active INTEGER DEFAULT 1,"
      "conversion_destination TEXT NOT NULL,"
      "source_type INTEGER NOT NULL,"
      "attributed_truthfully INTEGER NOT NULL)";
  if (!db->Execute(kNewImpressionTableSql))
    return false;

  // Transfer the existing rows to the new table, inserting default values for
  // the source_type and attributed_truthfully columns.
  const char kPopulateNewImpressionTableSql[] =
      "INSERT INTO new_impressions SELECT "
      "impression_id,impression_data,impression_origin,"
      "conversion_origin,reporting_origin,impression_time,"
      "expiry_time,num_conversions,active,conversion_destination,?,? "
      "FROM impressions";
  sql::Statement populate_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kPopulateNewImpressionTableSql));
  // Only navigation type was supported prior to this column being added.
  populate_statement.BindInt(
      0, static_cast<int>(StorableImpression::SourceType::kNavigation));
  populate_statement.BindBool(1, true);
  if (!populate_statement.Run())
    return false;

  const char kDropOldImpressionTableSql[] = "DROP TABLE impressions";
  if (!db->Execute(kDropOldImpressionTableSql))
    return false;

  const char kRenameImpressionTableSql[] =
      "ALTER TABLE new_impressions RENAME TO impressions";
  if (!db->Execute(kRenameImpressionTableSql))
    return false;

  // Create the pre-existing impression table indices on the new table.
  const char kImpressionExpiryIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_expiry_idx "
      "ON impressions(expiry_time)";
  if (!db->Execute(kImpressionExpiryIndexSql))
    return false;

  const char kImpressionOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_origin_idx "
      "ON impressions(impression_origin)";
  if (!db->Execute(kImpressionOriginIndexSql))
    return false;

  const char kConversionDestinationIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_destination_idx "
      "ON impressions(active, conversion_destination, reporting_origin)";
  if (!db->Execute(kConversionDestinationIndexSql))
    return false;

  meta_table->SetVersionNumber(3);
  return transaction.Commit();
}

bool ConversionStorageSqlMigrations::MigrateToVersion4(
    ConversionStorageSql* conversion_storage,
    sql::Database* db,
    sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  const char kRateLimitTableSql[] =
      "CREATE TABLE IF NOT EXISTS rate_limits"
      "(rate_limit_id INTEGER PRIMARY KEY,"
      "attribution_type INTEGER NOT NULL,"
      "impression_id INTEGER NOT NULL,"
      "impression_site TEXT NOT NULL,"
      "impression_origin TEXT NOT NULL,"
      "conversion_destination TEXT NOT NULL,"
      "conversion_origin TEXT NOT NULL,"
      "conversion_time INTEGER NOT NULL)";
  if (!db->Execute(kRateLimitTableSql))
    return false;

  const char kRateLimitImpressionSiteTypeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_impression_site_type_idx "
      "ON rate_limits(attribution_type, conversion_destination, "
      "impression_site, conversion_time)";
  if (!db->Execute(kRateLimitImpressionSiteTypeIndexSql))
    return false;

  const char kRateLimitConversionTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_conversion_time_idx "
      "ON rate_limits(conversion_time)";
  if (!db->Execute(kRateLimitConversionTimeIndexSql))
    return false;

  const char kRateLimitImpressionIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_impression_id_idx "
      "ON rate_limits(impression_id)";
  if (!db->Execute(kRateLimitImpressionIndexSql))
    return false;

  meta_table->SetVersionNumber(4);
  return transaction.Commit();
}

bool ConversionStorageSqlMigrations::MigrateToVersion5(
    ConversionStorageSql* conversion_storage,
    sql::Database* db,
    sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Any corresponding impressions will naturally be cleaned up by the expiry
  // logic.
  const char kDropZeroCreditConversionsSql[] =
      "DELETE FROM conversions WHERE attribution_credit = 0";
  if (!db->Execute(kDropZeroCreditConversionsSql))
    return false;

  const char kDropAttributionCreditColumnSql[] =
      "ALTER TABLE conversions DROP COLUMN attribution_credit";
  if (!db->Execute(kDropAttributionCreditColumnSql))
    return false;

  meta_table->SetVersionNumber(5);
  return transaction.Commit();
}

bool ConversionStorageSqlMigrations::MigrateToVersion6(
    ConversionStorageSql* conversion_storage,
    sql::Database* db,
    sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Add new priority column to the impressions table. This follows the steps
  // documented at https://sqlite.org/lang_altertable.html#otheralter. Other
  // approaches, like using "ALTER ... ADD COLUMN" require setting a DEFAULT
  // value for the column which is undesirable.
  const char kNewImpressionTableSql[] =
      "CREATE TABLE IF NOT EXISTS new_impressions"
      "(impression_id INTEGER PRIMARY KEY,"
      "impression_data TEXT NOT NULL,"
      "impression_origin TEXT NOT NULL,"
      "conversion_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "impression_time INTEGER NOT NULL,"
      "expiry_time INTEGER NOT NULL,"
      "num_conversions INTEGER DEFAULT 0,"
      "active INTEGER DEFAULT 1,"
      "conversion_destination TEXT NOT NULL,"
      "source_type INTEGER NOT NULL,"
      "attributed_truthfully INTEGER NOT NULL,"
      "priority INTEGER NOT NULL)";
  if (!db->Execute(kNewImpressionTableSql))
    return false;

  // Transfer the existing rows to the new table, inserting default values for
  // the priority column.
  const char kPopulateNewImpressionTableSql[] =
      "INSERT INTO new_impressions SELECT "
      "impression_id,impression_data,impression_origin,"
      "conversion_origin,reporting_origin,impression_time,"
      "expiry_time,num_conversions,active,conversion_destination,source_type,"
      "attributed_truthfully,0 "
      "FROM impressions";
  sql::Statement populate_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kPopulateNewImpressionTableSql));
  if (!populate_statement.Run())
    return false;

  const char kDropOldImpressionTableSql[] = "DROP TABLE impressions";
  if (!db->Execute(kDropOldImpressionTableSql))
    return false;

  const char kRenameImpressionTableSql[] =
      "ALTER TABLE new_impressions RENAME TO impressions";
  if (!db->Execute(kRenameImpressionTableSql))
    return false;

  // Create the pre-existing impression table indices on the new table.
  const char kImpressionExpiryIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_expiry_idx "
      "ON impressions(expiry_time)";
  if (!db->Execute(kImpressionExpiryIndexSql))
    return false;

  const char kImpressionOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_origin_idx "
      "ON impressions(impression_origin)";
  if (!db->Execute(kImpressionOriginIndexSql))
    return false;

  const char kConversionDestinationIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_destination_idx "
      "ON impressions(active, conversion_destination, reporting_origin)";
  if (!db->Execute(kConversionDestinationIndexSql))
    return false;

  meta_table->SetVersionNumber(6);
  return transaction.Commit();
}

bool ConversionStorageSqlMigrations::MigrateToVersion7(
    ConversionStorageSql* conversion_storage,
    sql::Database* db,
    sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Add new impression_site column to the impressions table. This follows the
  // steps documented at https://sqlite.org/lang_altertable.html#otheralter.
  // Other approaches, like using "ALTER ... ADD COLUMN" require setting a
  // DEFAULT value for the column which is undesirable.
  const char kNewImpressionTableSql[] =
      "CREATE TABLE IF NOT EXISTS new_impressions"
      "(impression_id INTEGER PRIMARY KEY,"
      "impression_data TEXT NOT NULL,"
      "impression_origin TEXT NOT NULL,"
      "conversion_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "impression_time INTEGER NOT NULL,"
      "expiry_time INTEGER NOT NULL,"
      "num_conversions INTEGER DEFAULT 0,"
      "active INTEGER DEFAULT 1,"
      "conversion_destination TEXT NOT NULL,"
      "source_type INTEGER NOT NULL,"
      "attributed_truthfully INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "impression_site TEXT NOT NULL)";
  if (!db->Execute(kNewImpressionTableSql))
    return false;

  // Transfer the existing rows to the new table, inserting placeholder values
  // for the impression_site column.
  const char kPopulateNewImpressionTableSql[] =
      "INSERT INTO new_impressions SELECT "
      "impression_id,impression_data,impression_origin,"
      "conversion_origin,reporting_origin,impression_time,"
      "expiry_time,num_conversions,active,conversion_destination,source_type,"
      "attributed_truthfully,priority,'' "
      "FROM impressions";
  sql::Statement populate_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kPopulateNewImpressionTableSql));
  if (!populate_statement.Run())
    return false;

  const char kDropOldImpressionTableSql[] = "DROP TABLE impressions";
  if (!db->Execute(kDropOldImpressionTableSql))
    return false;

  const char kRenameImpressionTableSql[] =
      "ALTER TABLE new_impressions RENAME TO impressions";
  if (!db->Execute(kRenameImpressionTableSql))
    return false;

  // Update each of the impression rows to have the correct associated
  // impression_site.
  //
  // We update a subset of rows at a time to avoid pulling the entire
  // impressions table into memory.
  std::vector<ImpressionIdAndImpressionOrigin> impressions =
      GetImpressionIdAndImpressionOrigins(db, /*start_impression_id=*/0);

  const char kUpdateImpressionSiteSql[] =
      "UPDATE impressions SET impression_site = ? WHERE impression_id = ?";
  sql::Statement update_impression_site_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kUpdateImpressionSiteSql));

  while (!impressions.empty()) {
    // Perform the column updates for each row we pulled into memory.
    for (const auto& impression : impressions) {
      update_impression_site_statement.Reset(/*clear_bound_vars=*/true);

      // The impression site is derived from the impression origin dynamically.
      update_impression_site_statement.BindString(
          0, net::SchemefulSite(impression.impression_origin).Serialize());
      update_impression_site_statement.BindInt64(1, impression.impression_id);
      if (!update_impression_site_statement.Run())
        return false;
    }

    // Fetch the next batch of rows from the database.
    impressions = GetImpressionIdAndImpressionOrigins(
        db, impressions.back().impression_id + 1);
  }

  // Create the pre-existing impression table indices on the new table.
  const char kImpressionExpiryIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_expiry_idx "
      "ON impressions(expiry_time)";
  if (!db->Execute(kImpressionExpiryIndexSql))
    return false;

  const char kImpressionOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_origin_idx "
      "ON impressions(impression_origin)";
  if (!db->Execute(kImpressionOriginIndexSql))
    return false;

  const char kConversionDestinationIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_destination_idx "
      "ON impressions(active, conversion_destination, reporting_origin)";
  if (!db->Execute(kConversionDestinationIndexSql))
    return false;

  // Create the new impression table index.
  const char kImpressionSiteIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_site_idx "
      "ON impressions(active, impression_site, source_type)";
  if (!db->Execute(kImpressionSiteIndexSql))
    return false;

  meta_table->SetVersionNumber(7);
  return transaction.Commit();
}

}  // namespace content
