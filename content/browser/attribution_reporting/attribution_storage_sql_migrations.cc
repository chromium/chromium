// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"

#include <vector>

#include "base/metrics/histogram_functions.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "net/base/schemeful_site.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/origin.h"

namespace content {

namespace {

WARN_UNUSED_RESULT
StorableSource::Id NextImpressionId(StorableSource::Id id) {
  return StorableSource::Id(*id + 1);
}

struct ImpressionIdAndConversionOrigin {
  StorableSource::Id impression_id;
  url::Origin conversion_origin;
};

std::vector<ImpressionIdAndConversionOrigin>
GetImpressionIdAndConversionOrigins(sql::Database* db,
                                    StorableSource::Id start_impression_id) {
  static constexpr char kGetImpressionsSql[] =
      "SELECT impression_id,conversion_origin "
      "FROM impressions "
      "WHERE impression_id >= ? "
      "ORDER BY impression_id "
      "LIMIT ?";

  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kGetImpressionsSql));
  statement.BindInt64(0, *start_impression_id);

  const int kNumImpressions = 100;
  statement.BindInt(1, kNumImpressions);

  std::vector<ImpressionIdAndConversionOrigin> impressions;
  while (statement.Step()) {
    StorableSource::Id impression_id(statement.ColumnInt64(0));
    url::Origin conversion_origin =
        DeserializeOrigin(statement.ColumnString(1));

    impressions.push_back({impression_id, std::move(conversion_origin)});
  }
  if (!statement.Succeeded())
    return {};
  return impressions;
}

struct ImpressionIdAndImpressionOrigin {
  StorableSource::Id impression_id;
  url::Origin impression_origin;
};

std::vector<ImpressionIdAndImpressionOrigin>
GetImpressionIdAndImpressionOrigins(sql::Database* db,
                                    StorableSource::Id start_impression_id) {
  static constexpr char kGetImpressionsSql[] =
      "SELECT impression_id,impression_origin "
      "FROM impressions "
      "WHERE impression_id >= ? "
      "ORDER BY impression_id "
      "LIMIT ?";

  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kGetImpressionsSql));
  statement.BindInt64(0, *start_impression_id);

  const int kNumImpressions = 100;
  statement.BindInt(1, kNumImpressions);

  std::vector<ImpressionIdAndImpressionOrigin> impressions;
  while (statement.Step()) {
    StorableSource::Id impression_id(statement.ColumnInt64(0));
    url::Origin impression_origin =
        DeserializeOrigin(statement.ColumnString(1));

    impressions.push_back({impression_id, std::move(impression_origin)});
  }
  if (!statement.Succeeded())
    return {};
  return impressions;
}

bool MigrateToVersion2(sql::Database* db, sql::MetaTable* meta_table) {
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
  static constexpr char kNewImpressionTableSql[] =
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
  static constexpr char kPopulateNewImpressionTableSql[] =
      "INSERT INTO new_impressions SELECT "
      "impression_id,impression_data,impression_origin,"
      "conversion_origin,reporting_origin,impression_time,"
      "expiry_time,num_conversions,active,'' "
      "FROM impressions";
  if (!db->Execute(kPopulateNewImpressionTableSql))
    return false;

  static constexpr char kDropOldImpressionTableSql[] = "DROP TABLE impressions";
  if (!db->Execute(kDropOldImpressionTableSql))
    return false;

  static constexpr char kRenameImpressionTableSql[] =
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
      GetImpressionIdAndConversionOrigins(db, StorableSource::Id(0));

  static constexpr char kUpdateDestinationSql[] =
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
      update_destination_statement.BindInt64(1, *impression.impression_id);
      update_destination_statement.Run();
    }

    // Fetch the next batch of rows from the database.
    impressions = GetImpressionIdAndConversionOrigins(
        db, NextImpressionId(impressions.back().impression_id));
  }

  // Create the pre-existing impression table indices on the new table.
  static constexpr char kImpressionExpiryIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_expiry_idx "
      "ON impressions(expiry_time)";
  if (!db->Execute(kImpressionExpiryIndexSql))
    return false;

  static constexpr char kImpressionOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_origin_idx "
      "ON impressions(impression_origin)";
  if (!db->Execute(kImpressionOriginIndexSql))
    return false;

  // Replace the pre-existing conversion_origin_idx with an index that uses the
  // conversion destination, as attribution logic now depends on the
  // conversion_destination.
  static constexpr char kConversionDestinationIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_destination_idx "
      "ON impressions(active,conversion_destination,reporting_origin)";
  if (!db->Execute(kConversionDestinationIndexSql))
    return false;

  meta_table->SetVersionNumber(2);
  return transaction.Commit();
}

bool MigrateToVersion3(sql::Database* db, sql::MetaTable* meta_table) {
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
  static constexpr char kNewImpressionTableSql[] =
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
  static constexpr char kPopulateNewImpressionTableSql[] =
      "INSERT INTO new_impressions SELECT "
      "impression_id,impression_data,impression_origin,"
      "conversion_origin,reporting_origin,impression_time,"
      "expiry_time,num_conversions,active,conversion_destination,?,? "
      "FROM impressions";
  sql::Statement populate_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kPopulateNewImpressionTableSql));
  // Only navigation type was supported prior to this column being added.
  populate_statement.BindInt(
      0, static_cast<int>(StorableSource::SourceType::kNavigation));
  populate_statement.BindBool(1, true);
  if (!populate_statement.Run())
    return false;

  static constexpr char kDropOldImpressionTableSql[] = "DROP TABLE impressions";
  if (!db->Execute(kDropOldImpressionTableSql))
    return false;

  static constexpr char kRenameImpressionTableSql[] =
      "ALTER TABLE new_impressions RENAME TO impressions";
  if (!db->Execute(kRenameImpressionTableSql))
    return false;

  // Create the pre-existing impression table indices on the new table.
  static constexpr char kImpressionExpiryIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_expiry_idx "
      "ON impressions(expiry_time)";
  if (!db->Execute(kImpressionExpiryIndexSql))
    return false;

  static constexpr char kImpressionOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_origin_idx "
      "ON impressions(impression_origin)";
  if (!db->Execute(kImpressionOriginIndexSql))
    return false;

  static constexpr char kConversionDestinationIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_destination_idx "
      "ON impressions(active,conversion_destination,reporting_origin)";
  if (!db->Execute(kConversionDestinationIndexSql))
    return false;

  meta_table->SetVersionNumber(3);
  return transaction.Commit();
}

bool MigrateToVersion4(sql::Database* db, sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  static constexpr char kRateLimitTableSql[] =
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

  static constexpr char kRateLimitImpressionSiteTypeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_impression_site_type_idx "
      "ON rate_limits(attribution_type,conversion_destination,"
      "impression_site,conversion_time)";
  if (!db->Execute(kRateLimitImpressionSiteTypeIndexSql))
    return false;

  static constexpr char kRateLimitConversionTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_conversion_time_idx "
      "ON rate_limits(conversion_time)";
  if (!db->Execute(kRateLimitConversionTimeIndexSql))
    return false;

  static constexpr char kRateLimitImpressionIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_impression_id_idx "
      "ON rate_limits(impression_id)";
  if (!db->Execute(kRateLimitImpressionIndexSql))
    return false;

  meta_table->SetVersionNumber(4);
  return transaction.Commit();
}

bool MigrateToVersion5(sql::Database* db, sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Any corresponding impressions will naturally be cleaned up by the expiry
  // logic.
  static constexpr char kDropZeroCreditConversionsSql[] =
      "DELETE FROM conversions WHERE attribution_credit = 0";
  if (!db->Execute(kDropZeroCreditConversionsSql))
    return false;

  static constexpr char kDropAttributionCreditColumnSql[] =
      "ALTER TABLE conversions DROP COLUMN attribution_credit";
  if (!db->Execute(kDropAttributionCreditColumnSql))
    return false;

  meta_table->SetVersionNumber(5);
  return transaction.Commit();
}

bool MigrateToVersion6(sql::Database* db, sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Add new priority column to the impressions table. This follows the steps
  // documented at https://sqlite.org/lang_altertable.html#otheralter. Other
  // approaches, like using "ALTER ... ADD COLUMN" require setting a DEFAULT
  // value for the column which is undesirable.
  static constexpr char kNewImpressionTableSql[] =
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
  static constexpr char kPopulateNewImpressionTableSql[] =
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

  static constexpr char kDropOldImpressionTableSql[] = "DROP TABLE impressions";
  if (!db->Execute(kDropOldImpressionTableSql))
    return false;

  static constexpr char kRenameImpressionTableSql[] =
      "ALTER TABLE new_impressions RENAME TO impressions";
  if (!db->Execute(kRenameImpressionTableSql))
    return false;

  // Create the pre-existing impression table indices on the new table.
  static constexpr char kImpressionExpiryIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_expiry_idx "
      "ON impressions(expiry_time)";
  if (!db->Execute(kImpressionExpiryIndexSql))
    return false;

  static constexpr char kImpressionOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_origin_idx "
      "ON impressions(impression_origin)";
  if (!db->Execute(kImpressionOriginIndexSql))
    return false;

  static constexpr char kConversionDestinationIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_destination_idx "
      "ON impressions(active,conversion_destination,reporting_origin)";
  if (!db->Execute(kConversionDestinationIndexSql))
    return false;

  meta_table->SetVersionNumber(6);
  return transaction.Commit();
}

bool MigrateToVersion7(sql::Database* db, sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Add new impression_site column to the impressions table. This follows the
  // steps documented at https://sqlite.org/lang_altertable.html#otheralter.
  // Other approaches, like using "ALTER ... ADD COLUMN" require setting a
  // DEFAULT value for the column which is undesirable.
  static constexpr char kNewImpressionTableSql[] =
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
  static constexpr char kPopulateNewImpressionTableSql[] =
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

  static constexpr char kDropOldImpressionTableSql[] = "DROP TABLE impressions";
  if (!db->Execute(kDropOldImpressionTableSql))
    return false;

  static constexpr char kRenameImpressionTableSql[] =
      "ALTER TABLE new_impressions RENAME TO impressions";
  if (!db->Execute(kRenameImpressionTableSql))
    return false;

  // Update each of the impression rows to have the correct associated
  // impression_site.
  //
  // We update a subset of rows at a time to avoid pulling the entire
  // impressions table into memory.
  std::vector<ImpressionIdAndImpressionOrigin> impressions =
      GetImpressionIdAndImpressionOrigins(db, StorableSource::Id(0));

  static constexpr char kUpdateImpressionSiteSql[] =
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
      update_impression_site_statement.BindInt64(1, *impression.impression_id);
      if (!update_impression_site_statement.Run())
        return false;
    }

    // Fetch the next batch of rows from the database.
    impressions = GetImpressionIdAndImpressionOrigins(
        db, NextImpressionId(impressions.back().impression_id));
  }

  // Create the pre-existing impression table indices on the new table.
  static constexpr char kImpressionExpiryIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_expiry_idx "
      "ON impressions(expiry_time)";
  if (!db->Execute(kImpressionExpiryIndexSql))
    return false;

  static constexpr char kImpressionOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_origin_idx "
      "ON impressions(impression_origin)";
  if (!db->Execute(kImpressionOriginIndexSql))
    return false;

  static constexpr char kConversionDestinationIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_destination_idx "
      "ON impressions(active,conversion_destination,reporting_origin)";
  if (!db->Execute(kConversionDestinationIndexSql))
    return false;

  // Create the new impression table index.
  static constexpr char kImpressionSiteIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_site_idx "
      "ON impressions(active,impression_site,source_type)";
  if (!db->Execute(kImpressionSiteIndexSql))
    return false;

  meta_table->SetVersionNumber(7);
  return transaction.Commit();
}

struct ImpressionIdAndImpressionData {
  StorableSource::Id impression_id;
  std::string impression_data;
};

std::vector<ImpressionIdAndImpressionData> GetImpressionIdAndImpressionData(
    sql::Database* db,
    StorableSource::Id start_impression_id) {
  static constexpr char kGetImpressionsSql[] =
      "SELECT impression_id,impression_data "
      "FROM impressions "
      "WHERE impression_id >= ? "
      "ORDER BY impression_id "
      "LIMIT ?";

  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kGetImpressionsSql));
  statement.BindInt64(0, *start_impression_id);

  const int kNumImpressions = 100;
  statement.BindInt(1, kNumImpressions);

  std::vector<ImpressionIdAndImpressionData> impressions;
  while (statement.Step()) {
    StorableSource::Id impression_id(statement.ColumnInt64(0));
    std::string impression_data = statement.ColumnString(1);

    impressions.push_back({impression_id, std::move(impression_data)});
  }
  if (!statement.Succeeded())
    return {};
  return impressions;
}

bool MigrateToVersion8(sql::Database* db, sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Change the impression_data column from TEXT to INTEGER. This follows the
  // steps documented at https://sqlite.org/lang_altertable.html#otheralter.
  // Other approaches, like using "ALTER ... ADD COLUMN" require setting a
  // DEFAULT value for the column which is undesirable.
  static constexpr char kNewImpressionTableSql[] =
      "CREATE TABLE IF NOT EXISTS new_impressions"
      "(impression_id INTEGER PRIMARY KEY,"
      "impression_data INTEGER NOT NULL,"
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

  // Transfer the existing impressions rows to the new table with a placeholder
  // for the impression_data column.
  static constexpr char kPopulateNewImpressionsSql[] =
      "INSERT INTO new_impressions SELECT "
      "impression_id,0,impression_origin,conversion_origin,reporting_origin,"
      "impression_time,expiry_time,num_conversions,active,"
      "conversion_destination,source_type,attributed_truthfully,priority,"
      "impression_site FROM impressions";
  sql::Statement populate_new_impressions_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kPopulateNewImpressionsSql));
  if (!populate_new_impressions_statement.Run())
    return false;

  // Update each of the impression rows to have the correct associated
  // impression_data. We can't use the CAST SQL function here because it
  // doesn't support the full range of `uint64_t`.
  //
  // We update a subset of rows at a time to avoid pulling the entire
  // impressions table into memory.
  std::vector<ImpressionIdAndImpressionData> impressions =
      GetImpressionIdAndImpressionData(db, StorableSource::Id(0));

  static constexpr char kUpdateImpressionDataSql[] =
      "UPDATE new_impressions SET impression_data = ? WHERE impression_id = ?";
  sql::Statement update_impression_data_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kUpdateImpressionDataSql));

  while (!impressions.empty()) {
    // Perform the column updates for each row we pulled into memory.
    for (const auto& impression : impressions) {
      // If we can't parse the data, skip the update to leave the column as 0.
      uint64_t impression_data = 0u;
      if (!base::StringToUint64(impression.impression_data, &impression_data))
        continue;

      update_impression_data_statement.Reset(/*clear_bound_vars=*/true);
      update_impression_data_statement.BindInt64(
          0, SerializeImpressionOrConversionData(impression_data));
      update_impression_data_statement.BindInt64(1, *impression.impression_id);
      update_impression_data_statement.Run();
    }

    // Fetch the next batch of rows from the database.
    impressions = GetImpressionIdAndImpressionData(
        db, NextImpressionId(impressions.back().impression_id));
  }

  static constexpr char kDropOldImpressionTableSql[] = "DROP TABLE impressions";
  if (!db->Execute(kDropOldImpressionTableSql))
    return false;

  static constexpr char kRenameImpressionTableSql[] =
      "ALTER TABLE new_impressions RENAME TO impressions";
  if (!db->Execute(kRenameImpressionTableSql))
    return false;

  // Create the pre-existing impression table indices on the new table.
  static constexpr char kImpressionExpiryIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_expiry_idx "
      "ON impressions(expiry_time)";
  if (!db->Execute(kImpressionExpiryIndexSql))
    return false;

  static constexpr char kImpressionOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_origin_idx "
      "ON impressions(impression_origin)";
  if (!db->Execute(kImpressionOriginIndexSql))
    return false;

  static constexpr char kConversionDestinationIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_destination_idx "
      "ON impressions(active,conversion_destination,reporting_origin)";
  if (!db->Execute(kConversionDestinationIndexSql))
    return false;

  static constexpr char kImpressionSiteIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_site_idx "
      "ON impressions(active,impression_site,source_type)";
  if (!db->Execute(kImpressionSiteIndexSql))
    return false;

  // Change the conversion_data column from TEXT to INTEGER and make
  // impression_id NOT NULL. This follows the steps documented at
  // https://sqlite.org/lang_altertable.html#otheralter./ Other approaches, like
  // using "ALTER ... ADD COLUMN" require setting a DEFAULT value for the column
  // which is undesirable.
  static constexpr char kNewConversionTableSql[] =
      "CREATE TABLE IF NOT EXISTS new_conversions"
      "(conversion_id INTEGER PRIMARY KEY,"
      "impression_id INTEGER NOT NULL,"
      "conversion_data INTEGER NOT NULL,"
      "conversion_time INTEGER NOT NULL,"
      "report_time INTEGER NOT NULL)";
  if (!db->Execute(kNewConversionTableSql))
    return false;

  // Transfer the existing conversions rows to the new table. See
  // https://www.sqlite.org/lang_expr.html#castexpr for details on CAST, which
  // we can use here because valid conversion_data is in the range [0, 8].
  // Existing impression_id values should never be NULL, but if they are, we
  // insert 0 instead of failing.
  static constexpr char kPopulateNewConversionsSql[] =
      "INSERT INTO new_conversions SELECT "
      "conversion_id,IFNULL(impression_id,0),"
      "CAST(conversion_data AS INTEGER),conversion_time,report_time "
      "FROM conversions";
  sql::Statement populate_new_conversions_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kPopulateNewConversionsSql));
  if (!populate_new_conversions_statement.Run())
    return false;

  static constexpr char kDropOldConversionTableSql[] = "DROP TABLE conversions";
  if (!db->Execute(kDropOldConversionTableSql))
    return false;

  static constexpr char kRenameConversionTableSql[] =
      "ALTER TABLE new_conversions RENAME TO conversions";
  if (!db->Execute(kRenameConversionTableSql))
    return false;

  // Create the pre-existing conversion table indices on the new table.
  static constexpr char kConversionReportTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_report_idx "
      "ON conversions(report_time)";
  if (!db->Execute(kConversionReportTimeIndexSql))
    return false;

  static constexpr char kConversionClickIdIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_impression_id_idx "
      "ON conversions(impression_id)";
  if (!db->Execute(kConversionClickIdIndexSql))
    return false;

  meta_table->SetVersionNumber(8);
  return transaction.Commit();
}

bool MigrateToVersion9(sql::Database* db, sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Add new priority column to the conversions table. This follows the
  // steps documented at https://sqlite.org/lang_altertable.html#otheralter.
  // Other approaches, like using "ALTER ... ADD COLUMN" require setting a
  // DEFAULT value for the column which is undesirable.
  static constexpr char kNewTableSql[] =
      "CREATE TABLE IF NOT EXISTS new_conversions"
      "(conversion_id INTEGER PRIMARY KEY,"
      "impression_id INTEGER NOT NULL,"
      "conversion_data INTEGER NOT NULL,"
      "conversion_time INTEGER NOT NULL,"
      "report_time INTEGER NOT NULL,"
      "priority INTEGER NOT NULL)";
  if (!db->Execute(kNewTableSql))
    return false;

  // Transfer the existing rows to the new table, inserting 0 for the priority
  // column.
  static constexpr char kPopulateSql[] =
      "INSERT INTO new_conversions SELECT "
      "conversion_id,impression_id,conversion_data,conversion_time,"
      "report_time,0 "
      "FROM conversions";
  sql::Statement populate_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kPopulateSql));
  if (!populate_statement.Run())
    return false;

  static constexpr char kDropOldTableSql[] = "DROP TABLE conversions";
  if (!db->Execute(kDropOldTableSql))
    return false;

  static constexpr char kRenameTableSql[] =
      "ALTER TABLE new_conversions RENAME TO conversions";
  if (!db->Execute(kRenameTableSql))
    return false;

  // Create the pre-existing conversion table indices on the new table.
  static constexpr char kConversionReportTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_report_idx "
      "ON conversions(report_time)";
  if (!db->Execute(kConversionReportTimeIndexSql))
    return false;

  static constexpr char kConversionClickIdIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_impression_id_idx "
      "ON conversions(impression_id)";
  if (!db->Execute(kConversionClickIdIndexSql))
    return false;

  meta_table->SetVersionNumber(9);
  return transaction.Commit();
}

bool MigrateToVersion10(sql::Database* db, sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  static constexpr char kDedupKeyTableSql[] =
      "CREATE TABLE IF NOT EXISTS dedup_keys"
      "(impression_id INTEGER NOT NULL,"
      "dedup_key INTEGER NOT NULL,"
      "PRIMARY KEY(impression_id,dedup_key))WITHOUT ROWID";
  if (!db->Execute(kDedupKeyTableSql))
    return false;

  meta_table->SetVersionNumber(10);
  return transaction.Commit();
}

bool MigrateToVersion11(sql::Database* db, sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  static constexpr char kDropOldImpressionSiteIdxSql[] =
      "DROP INDEX impression_site_idx";
  if (!db->Execute(kDropOldImpressionSiteIdxSql))
    return false;

  static constexpr char kEventSourceImpressionSiteIndexSql[] =
      "CREATE INDEX IF NOT EXISTS event_source_impression_site_idx "
      "ON impressions(impression_site)"
      "WHERE active = 1 AND num_conversions = 0 AND source_type = 1";
  if (!db->Execute(kEventSourceImpressionSiteIndexSql))
    return false;

  meta_table->SetVersionNumber(11);
  return transaction.Commit();
}

bool MigrateToVersion12(sql::Database* db, sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  static constexpr char kNewRateLimitTableSql[] =
      "CREATE TABLE IF NOT EXISTS new_rate_limits"
      "(rate_limit_id INTEGER PRIMARY KEY NOT NULL,"
      "attribution_type INTEGER NOT NULL,"
      "impression_id INTEGER NOT NULL,"
      "impression_site TEXT NOT NULL,"
      "impression_origin TEXT NOT NULL,"
      "conversion_destination TEXT NOT NULL,"
      "conversion_origin TEXT NOT NULL,"
      "conversion_time INTEGER NOT NULL,"
      "bucket TEXT NOT NULL,"
      "value INTEGER NOT NULL)";
  if (!db->Execute(kNewRateLimitTableSql))
    return false;

  // Transfer the existing rows to the new table, inserting 0 for `bucket` and
  // 1 for `value`, since all existing rows are non-aggregate.
  static constexpr char kPopulateNewRateLimitTableSql[] =
      "INSERT INTO new_rate_limits SELECT "
      "rate_limit_id,attribution_type,impression_id,impression_site,"
      "impression_origin,conversion_destination,conversion_origin,"
      "conversion_time,0,1 "
      "FROM rate_limits";
  if (!db->Execute(kPopulateNewRateLimitTableSql))
    return false;

  static constexpr char kDropOldRateLimitTableSql[] = "DROP TABLE rate_limits";
  if (!db->Execute(kDropOldRateLimitTableSql))
    return false;

  static constexpr char kRenameRateLimitTableSql[] =
      "ALTER TABLE new_rate_limits RENAME TO rate_limits";
  if (!db->Execute(kRenameRateLimitTableSql))
    return false;

  // Create the pre-existing indices on the new table.

  static constexpr char kRateLimitImpressionSiteTypeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_impression_site_type_idx "
      "ON rate_limits(attribution_type,conversion_destination,"
      "impression_site,conversion_time)";
  if (!db->Execute(kRateLimitImpressionSiteTypeIndexSql))
    return false;

  // Add the attribution_type as a prefix of the index.
  static constexpr char kRateLimitAttributionTypeConversionTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS "
      "rate_limit_attribution_type_conversion_time_idx "
      "ON rate_limits(attribution_type,conversion_time)";
  if (!db->Execute(kRateLimitAttributionTypeConversionTimeIndexSql))
    return false;

  static constexpr char kRateLimitImpressionIndexSql[] =
      "CREATE INDEX IF NOT EXISTS rate_limit_impression_id_idx "
      "ON rate_limits(impression_id)";
  if (!db->Execute(kRateLimitImpressionIndexSql))
    return false;

  meta_table->SetVersionNumber(12);
  return transaction.Commit();
}

bool MigrateToVersion13(sql::Database* db, sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Create the new impressions table with impression_id `NOT NULL` and
  // `AUTOINCREMENT`.
  static constexpr char kNewImpressionTableSql[] =
      "CREATE TABLE IF NOT EXISTS new_impressions"
      "(impression_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "impression_data INTEGER NOT NULL,"
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

  // Transfer the existing rows to the new table.
  static constexpr char kPopulateNewImpressionTableSql[] =
      "INSERT INTO new_impressions SELECT "
      "impression_id,impression_data,impression_origin,"
      "conversion_origin,reporting_origin,impression_time,"
      "expiry_time,num_conversions,active,conversion_destination,"
      "source_type,attributed_truthfully,priority,impression_site "
      "FROM impressions";
  if (!db->Execute(kPopulateNewImpressionTableSql))
    return false;

  static constexpr char kDropOldImpressionTableSql[] = "DROP TABLE impressions";
  if (!db->Execute(kDropOldImpressionTableSql))
    return false;

  static constexpr char kRenameImpressionTableSql[] =
      "ALTER TABLE new_impressions RENAME TO impressions";
  if (!db->Execute(kRenameImpressionTableSql))
    return false;

  // Create the pre-existing impression table indices on the new table.

  static constexpr char kConversionDestinationIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_destination_idx "
      "ON impressions(active,conversion_destination,reporting_origin)";
  if (!db->Execute(kConversionDestinationIndexSql))
    return false;

  static constexpr char kImpressionExpiryIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_expiry_idx "
      "ON impressions(expiry_time)";
  if (!db->Execute(kImpressionExpiryIndexSql))
    return false;

  static constexpr char kImpressionOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_origin_idx "
      "ON impressions(impression_origin)";
  if (!db->Execute(kImpressionOriginIndexSql))
    return false;

  static constexpr char kEventSourceImpressionSiteIndexSql[] =
      "CREATE INDEX IF NOT EXISTS event_source_impression_site_idx "
      "ON impressions(impression_site)"
      "WHERE active = 1 AND num_conversions = 0 AND source_type = 1";
  if (!db->Execute(kEventSourceImpressionSiteIndexSql))
    return false;

  // Create the new conversions table with conversion_id `NOT NULL` and
  // `AUTOINCREMENT`.
  static constexpr char kNewConversionTableSql[] =
      "CREATE TABLE IF NOT EXISTS new_conversions"
      "(conversion_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "impression_id INTEGER NOT NULL,"
      "conversion_data INTEGER NOT NULL,"
      "conversion_time INTEGER NOT NULL,"
      "report_time INTEGER NOT NULL,"
      "priority INTEGER NOT NULL)";
  if (!db->Execute(kNewConversionTableSql))
    return false;

  // Transfer the existing rows to the new table.
  static constexpr char kPopulateNewConversionTableSql[] =
      "INSERT INTO new_conversions SELECT "
      "conversion_id,impression_id,conversion_data,conversion_time,"
      "report_time,priority "
      "FROM conversions";
  if (!db->Execute(kPopulateNewConversionTableSql))
    return false;

  static constexpr char kDropOldConversionTableSql[] = "DROP TABLE conversions";
  if (!db->Execute(kDropOldConversionTableSql))
    return false;

  static constexpr char kRenameConversionTableSql[] =
      "ALTER TABLE new_conversions RENAME TO conversions";
  if (!db->Execute(kRenameConversionTableSql))
    return false;

  // Create the pre-existing conversion table indices on the new table.

  static constexpr char kConversionReportTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_report_idx "
      "ON conversions(report_time)";
  if (!db->Execute(kConversionReportTimeIndexSql))
    return false;

  static constexpr char kConversionImpressionIdIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_impression_id_idx "
      "ON conversions(impression_id)";
  if (!db->Execute(kConversionImpressionIdIndexSql))
    return false;

  meta_table->SetVersionNumber(13);
  return transaction.Commit();
}

bool MigrateToVersion14(sql::Database* db, sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. See comment in
  // |MigrateToVersion2|.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Create the new conversions table with failed_send_attempts.
  static constexpr char kNewConversionTableSql[] =
      "CREATE TABLE IF NOT EXISTS new_conversions"
      "(conversion_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "impression_id INTEGER NOT NULL,"
      "conversion_data INTEGER NOT NULL,"
      "conversion_time INTEGER NOT NULL,"
      "report_time INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "failed_send_attempts INTEGER NOT NULL)";
  if (!db->Execute(kNewConversionTableSql))
    return false;

  // Transfer the existing conversions rows to the new table, using 0 for
  // failed_send_attempts since we have no basis to say otherwise.
  static constexpr char kPopulateNewConversionsSql[] =
      "INSERT INTO new_conversions SELECT "
      "conversion_id,impression_id,conversion_data,conversion_time,report_time,"
      "priority,0 FROM conversions";
  sql::Statement populate_new_conversions_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kPopulateNewConversionsSql));
  if (!populate_new_conversions_statement.Run())
    return false;

  static constexpr char kDropOldConversionTableSql[] = "DROP TABLE conversions";
  if (!db->Execute(kDropOldConversionTableSql))
    return false;

  static constexpr char kRenameConversionTableSql[] =
      "ALTER TABLE new_conversions RENAME TO conversions";
  if (!db->Execute(kRenameConversionTableSql))
    return false;

  // Create the pre-existing conversion table indices on the new table.

  static constexpr char kConversionReportTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_report_idx "
      "ON conversions(report_time)";
  if (!db->Execute(kConversionReportTimeIndexSql))
    return false;

  static constexpr char kConversionImpressionIdIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_impression_id_idx "
      "ON conversions(impression_id)";
  if (!db->Execute(kConversionImpressionIdIndexSql))
    return false;

  meta_table->SetVersionNumber(14);
  return transaction.Commit();
}

}  // namespace

bool UpgradeAttributionStorageSqlSchema(sql::Database* db,
                                        sql::MetaTable* meta_table) {
  base::ThreadTicks start_timestamp = base::ThreadTicks::Now();

  if (meta_table->GetVersionNumber() == 1) {
    if (!MigrateToVersion2(db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 2) {
    if (!MigrateToVersion3(db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 3) {
    if (!MigrateToVersion4(db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 4) {
    if (!MigrateToVersion5(db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 5) {
    if (!MigrateToVersion6(db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 6) {
    if (!MigrateToVersion7(db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 7) {
    if (!MigrateToVersion8(db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 8) {
    if (!MigrateToVersion9(db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 9) {
    if (!MigrateToVersion10(db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 10) {
    if (!MigrateToVersion11(db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 11) {
    if (!MigrateToVersion12(db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 12) {
    if (!MigrateToVersion13(db, meta_table))
      return false;
  }
  if (meta_table->GetVersionNumber() == 13) {
    if (!MigrateToVersion14(db, meta_table))
      return false;
  }
  // Add similar if () blocks for new versions here.

  base::UmaHistogramMediumTimes("Conversions.Storage.MigrationTime",
                                base::ThreadTicks::Now() - start_timestamp);
  return true;
}

}  // namespace content
