// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage_sql_migrations.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_storage_sql.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace content {

bool ConversionStorageSqlMigrations::UpgradeSchema(
    ConversionStorageSql* conversion_storage,
    sql::Database* db,
    sql::MetaTable* meta_table) {
  base::ThreadTicks start_timestamp = base::ThreadTicks::Now();

  if (meta_table->GetVersionNumber() == 1) {
    if (!MigrateToVersion2(conversion_storage, db, meta_table))
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
  // Wrap each migration in it's own transaction. This results in smaller
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
  // We update `kNumImpressionsPerUpdate` rows at a time, to avoid pulling the
  // entire impressions table into memory.
  int64_t start_impression_id = 0;
  const size_t kNumImpressionsPerUpdate = 100u;
  std::vector<StorableImpression> impressions =
      conversion_storage->GetImpressions(
          ConversionStorageSql::ImpressionFilter::kAll, base::Time::Min(),
          start_impression_id, kNumImpressionsPerUpdate);

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
          0, impression.ConversionDestination().Serialize());
      update_destination_statement.BindInt64(1, *impression.impression_id());
      update_destination_statement.Run();

      // Track the largest row id. This is more efficient than sorting all the
      // rows.
      if (*impression.impression_id() > start_impression_id)
        start_impression_id = *impression.impression_id();
    }

    // Fetch the next batch of rows from the database.
    start_impression_id += 1;
    impressions = conversion_storage->GetImpressions(
        ConversionStorageSql::ImpressionFilter::kAll, base::Time::Min(),
        start_impression_id, kNumImpressionsPerUpdate);
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

}  // namespace content
