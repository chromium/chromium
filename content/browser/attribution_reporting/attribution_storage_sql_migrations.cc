// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"

#include "base/functional/function_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace content {

namespace {

// Ensure that both version numbers are updated together to prevent crashes on
// downgrades as in crbug.com/1413728.
[[nodiscard]] bool SetVersionNumbers(sql::MetaTable& meta_table, int version) {
  return meta_table.SetVersionNumber(version) &&
         meta_table.SetCompatibleVersionNumber(version);
}

// Wrap each migration in its own transaction. This results in smaller
// transactions, so it's less likely that a transaction's buffer will need to
// spill to disk. Also, if the database grows a lot and Chrome stops (user
// quit, process kill, etc.) during the migration process, per-migration
// transactions make it more likely that we'll make forward progress each time
// Chrome stops.
[[nodiscard]] bool MaybeMigrate(
    sql::Database& db,
    sql::MetaTable& meta_table,
    int old_version,
    base::FunctionRef<bool(sql::Database&)> migrate) {
  if (meta_table.GetVersionNumber() != old_version) {
    return true;
  }

  sql::Transaction transaction(&db);

  return transaction.Begin() &&                             //
         migrate(db) &&                                     //
         SetVersionNumbers(meta_table, old_version + 1) &&  //
         transaction.Commit();
}

bool To53(sql::Database& db) {
  static constexpr char kNewSourcesTableSql[] =
      "CREATE TABLE new_sources("
      "source_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "source_event_id INTEGER NOT NULL,"
      "source_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "source_time INTEGER NOT NULL,"
      "expiry_time INTEGER NOT NULL,"
      "event_report_window_time INTEGER NOT NULL,"
      "aggregatable_report_window_time INTEGER NOT NULL,"
      "num_attributions INTEGER NOT NULL,"
      "event_level_active INTEGER NOT NULL,"
      "aggregatable_active INTEGER NOT NULL,"
      "source_type INTEGER NOT NULL,"
      "attribution_logic INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "source_site TEXT NOT NULL,"
      "debug_key INTEGER,"
      "aggregatable_budget_consumed INTEGER NOT NULL,"
      "num_aggregatable_reports INTEGER NOT NULL,"
      "aggregatable_source BLOB NOT NULL,"
      "filter_data BLOB NOT NULL)";
  if (!db.Execute(kNewSourcesTableSql)) {
    return false;
  }

  // Transfer the existing rows to the new table. Set a value of
  // `num_aggregatable_reports` as 1 if the source has a non-zero value of
  // `aggregatable_budget_consumed`.
  static constexpr char kPopulateNewSourcesTableSql[] =
      "INSERT INTO new_sources SELECT "
      "source_id,source_event_id,source_origin,"
      "reporting_origin,source_time,"
      "expiry_time,event_report_window_time,aggregatable_report_window_time,"
      "source_type,attribution_logic,priority,source_site,"
      "num_attributions,event_level_active,aggregatable_active,debug_key,"
      "aggregatable_budget_consumed,"
      "IIF(aggregatable_budget_consumed>0,1,0),"
      "aggregatable_source,filter_data FROM sources";
  if (!db.Execute(kPopulateNewSourcesTableSql)) {
    return false;
  }

  static constexpr char kDropOldSourcesTableSql[] = "DROP TABLE sources";
  if (!db.Execute(kDropOldSourcesTableSql)) {
    return false;
  }

  static constexpr char kRenameSourcesTableSql[] =
      "ALTER TABLE new_sources RENAME TO sources";
  if (!db.Execute(kRenameSourcesTableSql)) {
    return false;
  }

  // Create the sources table indices on the new table.
  static constexpr char kSourcesByActiveReportingOriginIndexSql[] =
      "CREATE INDEX sources_by_active_reporting_origin "
      "ON sources(event_level_active,"
      "aggregatable_active,reporting_origin)";
  if (!db.Execute(kSourcesByActiveReportingOriginIndexSql)) {
    return false;
  }

  static constexpr char kImpressionExpiryIndexSql[] =
      "CREATE INDEX sources_by_expiry_time "
      "ON sources(expiry_time)";
  if (!db.Execute(kImpressionExpiryIndexSql)) {
    return false;
  }

  static constexpr char kImpressionOriginIndexSql[] =
      "CREATE INDEX active_sources_by_source_origin "
      "ON sources(source_origin)"
      "WHERE event_level_active=1 OR aggregatable_active=1";
  if (!db.Execute(kImpressionOriginIndexSql)) {
    return false;
  }

  static constexpr char kSourcesSourceTimeIndexSql[] =
      "CREATE INDEX sources_by_source_time "
      "ON sources(source_time)";
  if (!db.Execute(kSourcesSourceTimeIndexSql)) {
    return false;
  }

  return true;
}

}  // namespace

bool UpgradeAttributionStorageSqlSchema(sql::Database& db,
                                        sql::MetaTable& meta_table) {
  base::ThreadTicks start_timestamp;
  if (base::ThreadTicks::IsSupported()) {
    start_timestamp = base::ThreadTicks::Now();
  }

  static_assert(AttributionStorageSql::kDeprecatedVersionNumber + 1 == 52,
                "Remove migration(s) below.");

  bool ok = MaybeMigrate(db, meta_table, 52, &To53);
  if (!ok) {
    return false;
  }

  static_assert(AttributionStorageSql::kCurrentVersionNumber == 53,
                "Add migration(s) above.");

  if (base::ThreadTicks::IsSupported()) {
    base::UmaHistogramMediumTimes("Conversions.Storage.MigrationTime",
                                  base::ThreadTicks::Now() - start_timestamp);
  }

  return true;
}

}  // namespace content
