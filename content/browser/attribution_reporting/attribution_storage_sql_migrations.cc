// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace content {

namespace {

bool MigrateToVersion34(sql::Database* db, sql::MetaTable* meta_table) {
  // Wrap each migration in its own transaction. This results in smaller
  // transactions, so it's less likely that a transaction's buffer will need to
  // spill to disk. Also, if the database grows a lot and Chrome stops (user
  // quit, process kill, etc.) during the migration process, per-migration
  // transactions make it more likely that we'll make forward progress each time
  // Chrome stops.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Create the new aggregatable_report_metadata table with initial_report_time.
  // This follows the steps documented at
  // https://sqlite.org/lang_altertable.html#otheralter. Other approaches, like
  // using "ALTER ... ADD COLUMN" require setting a DEFAULT value for the column
  // which is undesirable.
  static constexpr char kNewAggregatableReportMetadataTableSql[] =
      "CREATE TABLE IF NOT EXISTS new_aggregatable_report_metadata("
      "aggregation_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "source_id INTEGER NOT NULL,"
      "trigger_time INTEGER NOT NULL,"
      "debug_key INTEGER,"
      "external_report_id TEXT NOT NULL,"
      "report_time INTEGER NOT NULL,"
      "failed_send_attempts INTEGER NOT NULL,"
      "initial_report_time INTEGER NOT NULL)";
  if (!db->Execute(kNewAggregatableReportMetadataTableSql))
    return false;

  // Transfer the existing aggregatable_report_metadata rows to the new table,
  // using the report_time value for initial_report_time.
  static constexpr char kPopulateNewAggregatableReportMetadataSql[] =
      "INSERT INTO new_aggregatable_report_metadata SELECT "
      "aggregation_id,source_id,trigger_time,debug_key,external_report_id,"
      "report_time,failed_send_attempts,report_time "
      "FROM aggregatable_report_metadata";
  sql::Statement populate_new_aggregatable_report_metadata_statement(
      db->GetCachedStatement(SQL_FROM_HERE,
                             kPopulateNewAggregatableReportMetadataSql));
  if (!populate_new_aggregatable_report_metadata_statement.Run())
    return false;

  static constexpr char kDropOldAggregatableReportMetadataTableSql[] =
      "DROP TABLE aggregatable_report_metadata";
  if (!db->Execute(kDropOldAggregatableReportMetadataTableSql))
    return false;

  static constexpr char kRenameAggregatableReportMetadataTableSql[] =
      "ALTER TABLE new_aggregatable_report_metadata "
      "RENAME TO aggregatable_report_metadata";
  if (!db->Execute(kRenameAggregatableReportMetadataTableSql))
    return false;

  // Create the pre-existing aggregatable_report_metadata table indices on the
  // new table.

  static constexpr char kAggregateSourceIdIndexSql[] =
      "CREATE INDEX IF NOT EXISTS aggregate_source_id_idx "
      "ON aggregatable_report_metadata(source_id)";
  if (!db->Execute(kAggregateSourceIdIndexSql))
    return false;

  static constexpr char kAggregateTriggerTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS aggregate_trigger_time_idx "
      "ON aggregatable_report_metadata(trigger_time)";
  if (!db->Execute(kAggregateTriggerTimeIndexSql))
    return false;

  static constexpr char kAggregateReportTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS aggregate_report_time_idx "
      "ON aggregatable_report_metadata(report_time)";
  if (!db->Execute(kAggregateReportTimeIndexSql))
    return false;

  meta_table->SetVersionNumber(34);
  return transaction.Commit();
}

}  // namespace

bool UpgradeAttributionStorageSqlSchema(sql::Database* db,
                                        sql::MetaTable* meta_table) {
  DCHECK(db);
  DCHECK(meta_table);

  base::ThreadTicks start_timestamp = base::ThreadTicks::Now();

  if (meta_table->GetVersionNumber() == 33) {
    if (!MigrateToVersion34(db, meta_table))
      return false;
  }
  // Add similar if () blocks for new versions here.

  base::UmaHistogramMediumTimes("Conversions.Storage.MigrationTime",
                                base::ThreadTicks::Now() - start_timestamp);
  return true;
}

}  // namespace content
