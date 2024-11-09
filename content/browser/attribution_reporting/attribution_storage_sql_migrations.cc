// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"

#include <string_view>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/origin.h"

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

bool To55(sql::Database& db) {
  if (!db.Execute("DROP INDEX rate_limit_source_site_reporting_site_idx")) {
    return false;
  }

  if (!db.Execute("DROP INDEX rate_limit_reporting_origin_idx")) {
    return false;
  }

  static constexpr char kRateLimitReportingOriginIndexSql[] =
      "CREATE INDEX rate_limit_reporting_origin_idx "
      "ON rate_limits(scope,source_site,destination_site)";
  return db.Execute(kRateLimitReportingOriginIndexSql);
}

bool To56(sql::Database& db) {
  static constexpr char kNewSourcesTableSql[] =
      "CREATE TABLE new_sources("
      "source_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "source_event_id INTEGER NOT NULL,"
      "source_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "source_time INTEGER NOT NULL,"
      "expiry_time INTEGER NOT NULL,"
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
      "filter_data BLOB NOT NULL,"
      "read_only_source_data BLOB NOT NULL)";
  if (!db.Execute(kNewSourcesTableSql)) {
    return false;
  }

  // Transfer the existing rows to the new table. Set a value of
  // `read_only_source_data` as '' to be replaced.
  static constexpr char kPopulateNewSourcesTableSql[] =
      "INSERT INTO new_sources SELECT "
      "source_id,source_event_id,source_origin,"
      "reporting_origin,source_time,expiry_time,"
      "aggregatable_report_window_time,"
      "num_attributions,event_level_active,aggregatable_active,"
      "source_type,attribution_logic,priority,source_site,debug_key,"
      "aggregatable_budget_consumed,"
      "num_aggregatable_reports,aggregatable_source,"
      "filter_data,'' FROM sources";
  if (!db.Execute(kPopulateNewSourcesTableSql)) {
    return false;
  }

  static constexpr char kGetEventReportWindowTime[] =
      "SELECT source_id,source_time,event_report_window_time,source_type FROM "
      "sources";
  sql::Statement get_statement(
      db.GetUniqueStatement(kGetEventReportWindowTime));

  static constexpr char kSetFlexEventConfig[] =
      "UPDATE new_sources SET read_only_source_data=? WHERE source_id=?";
  sql::Statement set_statement(db.GetUniqueStatement(kSetFlexEventConfig));

  while (get_statement.Step()) {
    int64_t id = get_statement.ColumnInt64(0);
    base::Time source_time = get_statement.ColumnTime(1);
    base::Time event_report_window_time = get_statement.ColumnTime(2);
    auto source_type = DeserializeSourceType(get_statement.ColumnInt(3));
    if (!source_type.has_value()) {
      continue;
    }

    auto event_report_windows =
        attribution_reporting::EventReportWindows::FromDefaults(
            event_report_window_time - source_time, *source_type);
    if (!event_report_windows.has_value()) {
      continue;
    }

    proto::AttributionReadOnlySourceData msg;
    SetReadOnlySourceData(
        &*event_report_windows,
        attribution_reporting::MaxEventLevelReports(*source_type), msg);

    set_statement.Reset(/*clear_bound_vars=*/true);
    set_statement.BindBlob(0, msg.SerializeAsString());
    set_statement.BindInt64(1, id);
    if (!set_statement.Run()) {
      return false;
    }
  }

  if (!db.Execute("DROP TABLE sources")) {
    return false;
  }

  if (!db.Execute("ALTER TABLE new_sources RENAME TO sources")) {
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

void DeleteCorruptedReports(AttributionStorageSql& storage) {
  AttributionStorageSql::DeletionCounts counts;
  // Performs its own per item transaction when deleting.
  storage.VerifyReports(&counts);
  base::UmaHistogramCounts100000("Conversions.CorruptSourcesDeletedOnMigration",
                                 counts.sources);
  base::UmaHistogramCounts100000("Conversions.CorruptReportsDeletedOnMigration",
                                 counts.reports);
}

// Version bump only, it was initially added to delete corrupted reports.
// However, given that the corrupted reports logic requires the latest schema,
// it was instead moved to a standalone operation `DeleteCorruptedReports`
// performed after all migrations.
bool To57(sql::Database&) {
  return true;
}

// Version bump only: We avoid having to populate the new trigger_data proto
// field for existing sources by treating absence of the field as equivalent to
// specifying the default trigger-data cardinality for the source's source
// type. We nonetheless bump the DB version to ensure that browser
// downgrades do not result in this new field being ignored for sources
// that *do* use non-default trigger data, because we raze the DB if the
// version is too new.
bool To58(sql::Database&) {
  return true;
}

bool To59(sql::Database& db) {
  static constexpr char kRateLimitTableSql[] =
      "CREATE TABLE new_rate_limits("
      "id INTEGER PRIMARY KEY NOT NULL,"
      "scope INTEGER NOT NULL,"
      "source_id INTEGER NOT NULL,"
      "source_site TEXT NOT NULL,"
      "destination_site TEXT NOT NULL,"
      "context_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "reporting_site TEXT NOT NULL,"
      "time INTEGER NOT NULL,"
      "source_expiry_or_attribution_time INTEGER NOT NULL,"
      "report_id INTEGER NOT NULL)";
  if (!db.Execute(kRateLimitTableSql)) {
    return false;
  }

  static constexpr char kPopulateExistingRecordsSql[] =
      "INSERT INTO new_rate_limits SELECT "
      "id,scope,source_id,source_site,destination_site,context_origin,"
      "reporting_origin,reporting_site,time,source_expiry_or_attribution_time,"
      "-1 "
      "FROM rate_limits";
  if (!db.Execute(kPopulateExistingRecordsSql)) {
    return false;
  }

  static constexpr char kPopulateAggregatableAttributionsSql[] =
      "INSERT INTO new_rate_limits "
      "(scope,source_id,source_site,destination_site,context_origin,"
      "reporting_origin,reporting_site,time,source_expiry_or_attribution_time,"
      "report_id)"
      "SELECT "
      "2,source_id,source_site,destination_site,context_origin,"
      "reporting_origin,reporting_site,time,source_expiry_or_attribution_time,"
      "-1 "
      "FROM rate_limits WHERE scope=1";
  if (!db.Execute(kPopulateAggregatableAttributionsSql)) {
    return false;
  }

  if (!db.Execute("DROP TABLE rate_limits")) {
    return false;
  }

  if (!db.Execute("ALTER TABLE new_rate_limits RENAME TO rate_limits")) {
    return false;
  }

  static constexpr char kRateLimitReportingOriginIndexSql[] =
      "CREATE INDEX rate_limit_reporting_origin_idx "
      "ON rate_limits(scope,source_site,destination_site)";
  if (!db.Execute(kRateLimitReportingOriginIndexSql)) {
    return false;
  }

  static constexpr char kRateLimitTimeIndexSql[] =
      "CREATE INDEX rate_limit_time_idx ON rate_limits(time)";
  if (!db.Execute(kRateLimitTimeIndexSql)) {
    return false;
  }

  static constexpr char kRateLimitImpressionIdIndexSql[] =
      "CREATE INDEX rate_limit_source_id_idx "
      "ON rate_limits(source_id)";
  if (!db.Execute(kRateLimitImpressionIdIndexSql)) {
    return false;
  }

  static constexpr char kRateLimitReportIdIndexSql[] =
      "CREATE INDEX rate_limit_report_id_idx "
      "ON rate_limits(scope,report_id)"
      "WHERE (scope=1 OR scope=2) AND report_id!=-1";
  if (!db.Execute(kRateLimitReportIdIndexSql)) {
    return false;
  }

  return true;
}

bool To60(sql::Database& db) {
  static constexpr char kRenameNumAggregatableReportsSql[] =
      "ALTER TABLE sources "
      "RENAME num_aggregatable_reports TO num_aggregatable_attribution_reports";
  if (!db.Execute(kRenameNumAggregatableReportsSql)) {
    return false;
  }

  static constexpr char kRenameAggregatableBudgetConsumedSql[] =
      "ALTER TABLE sources "
      "RENAME aggregatable_budget_consumed TO "
      "remaining_aggregatable_attribution_budget";
  if (!db.Execute(kRenameAggregatableBudgetConsumedSql)) {
    return false;
  }

  static constexpr char kComputeRemainingAggregatableBudgetSql[] =
      "UPDATE sources "
      "SET remaining_aggregatable_attribution_budget="
      "65536-remaining_aggregatable_attribution_budget";
  if (!db.Execute(kComputeRemainingAggregatableBudgetSql)) {
    return false;
  }

  return true;
}

bool To61(sql::Database& db) {
  static constexpr char kNewSourcesTableSql[] =
      "CREATE TABLE new_sources("
      "source_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "source_event_id INTEGER NOT NULL,"
      "source_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "source_time INTEGER NOT NULL,"
      "expiry_time INTEGER NOT NULL,"
      "aggregatable_report_window_time INTEGER NOT NULL,"
      "num_attributions INTEGER NOT NULL,"
      "event_level_active INTEGER NOT NULL,"
      "aggregatable_active INTEGER NOT NULL,"
      "source_type INTEGER NOT NULL,"
      "attribution_logic INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "source_site TEXT NOT NULL,"
      "debug_key INTEGER,"
      "remaining_aggregatable_attribution_budget INTEGER NOT NULL,"
      "num_aggregatable_attribution_reports INTEGER NOT NULL,"
      "aggregatable_source BLOB NOT NULL,"
      "filter_data BLOB NOT NULL,"
      "read_only_source_data BLOB NOT NULL,"
      "remaining_aggregatable_debug_budget INTEGER NOT NULL,"
      "num_aggregatable_debug_reports INTEGER NOT NULL)";
  if (!db.Execute(kNewSourcesTableSql)) {
    return false;
  }

  static constexpr char kPopulateNewSourcesTableSql[] =
      "INSERT INTO new_sources SELECT "
      "source_id,source_event_id,source_origin,"
      "reporting_origin,source_time,expiry_time,"
      "aggregatable_report_window_time,"
      "num_attributions,event_level_active,aggregatable_active,"
      "source_type,attribution_logic,priority,source_site,debug_key,"
      "remaining_aggregatable_attribution_budget,"
      "num_aggregatable_attribution_reports,aggregatable_source,"
      "filter_data,read_only_source_data,0,0 FROM sources";
  if (!db.Execute(kPopulateNewSourcesTableSql)) {
    return false;
  }

  if (!db.Execute("DROP TABLE sources")) {
    return false;
  }

  if (!db.Execute("ALTER TABLE new_sources RENAME TO sources")) {
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

bool To62(sql::Database& db) {
  static constexpr char kAggregatableDebugRateLimitsTableSql[] =
      "CREATE TABLE aggregatable_debug_rate_limits("
      "id INTEGER PRIMARY KEY NOT NULL,"
      "context_site TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "reporting_site TEXT NOT NULL,"
      "time INTEGER NOT NULL,"
      "consumed_budget INTEGER NOT NULL)";
  if (!db.Execute(kAggregatableDebugRateLimitsTableSql)) {
    return false;
  }

  static constexpr char kAggregatableDebugRateLimitsContextSiteIndex[] =
      "CREATE INDEX aggregatable_debug_rate_limits_context_site_idx "
      "ON aggregatable_debug_rate_limits(context_site)";
  if (!db.Execute(kAggregatableDebugRateLimitsContextSiteIndex)) {
    return false;
  }

  static constexpr char kAggregatableDebugRateLimitsTimeIndex[] =
      "CREATE INDEX aggregatable_debug_rate_limits_time_idx "
      "ON aggregatable_debug_rate_limits(time)";
  if (!db.Execute(kAggregatableDebugRateLimitsTimeIndex)) {
    return false;
  }

  return true;
}

bool To63(sql::Database& db) {
  static constexpr char kRateLimitTableSql[] =
      "CREATE TABLE new_rate_limits("
      "id INTEGER PRIMARY KEY NOT NULL,"
      "scope INTEGER NOT NULL,"
      "source_id INTEGER NOT NULL,"
      "source_site TEXT NOT NULL,"
      "destination_site TEXT NOT NULL,"
      "context_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "reporting_site TEXT NOT NULL,"
      "time INTEGER NOT NULL,"
      "source_expiry_or_attribution_time INTEGER NOT NULL,"
      "report_id INTEGER NOT NULL,"
      "deactivated_for_source_destination_limit INTEGER NOT NULL,"
      "destination_limit_priority INTEGER NOT NULL)";
  if (!db.Execute(kRateLimitTableSql)) {
    return false;
  }

  static constexpr char kPopulateExistingRecordsSql[] =
      "INSERT INTO new_rate_limits SELECT "
      "id,scope,source_id,source_site,destination_site,context_origin,"
      "reporting_origin,reporting_site,time,source_expiry_or_attribution_time,"
      "report_id,0,0 "
      "FROM rate_limits";
  if (!db.Execute(kPopulateExistingRecordsSql)) {
    return false;
  }

  if (!db.Execute("DROP TABLE rate_limits")) {
    return false;
  }

  if (!db.Execute("ALTER TABLE new_rate_limits RENAME TO rate_limits")) {
    return false;
  }

  static constexpr char kRateLimitReportingOriginIndexSql[] =
      "CREATE INDEX rate_limit_reporting_origin_idx "
      "ON rate_limits(scope,source_site,destination_site)";
  if (!db.Execute(kRateLimitReportingOriginIndexSql)) {
    return false;
  }

  static constexpr char kRateLimitTimeIndexSql[] =
      "CREATE INDEX rate_limit_time_idx ON rate_limits(time)";
  if (!db.Execute(kRateLimitTimeIndexSql)) {
    return false;
  }

  static constexpr char kRateLimitImpressionIdIndexSql[] =
      "CREATE INDEX rate_limit_source_id_idx "
      "ON rate_limits(source_id)";
  if (!db.Execute(kRateLimitImpressionIdIndexSql)) {
    return false;
  }

  static constexpr char kRateLimitReportIdIndexSql[] =
      "CREATE INDEX rate_limit_report_id_idx "
      "ON rate_limits(scope,report_id)"
      "WHERE (scope=1 OR scope=2) AND report_id!=-1";
  if (!db.Execute(kRateLimitReportIdIndexSql)) {
    return false;
  }

  return true;
}

bool To64(sql::Database& db) {
  static constexpr char kScopesDataColumnSql[] =
      "ALTER TABLE sources ADD attribution_scopes_data BLOB";
  return db.Execute(kScopesDataColumnSql);
}

bool To65(sql::Database& db) {
  static constexpr char
      kRateLimitAttributionDestinationReportingSiteIndexSql[] =
          "CREATE INDEX rate_limit_attribution_destination_reporting_site_idx "
          "ON rate_limits(scope,destination_site,reporting_site)"
          "WHERE(scope=1 OR scope=2)";
  return db.Execute(kRateLimitAttributionDestinationReportingSiteIndexSql);
}

bool To66(sql::Database& db) {
  static constexpr char kNamedBudgetsColumnSql[] =
      "ALTER TABLE sources ADD aggregatable_named_budgets BLOB";
  return db.Execute(kNamedBudgetsColumnSql);
}

bool To67(sql::Database& db) {
  static constexpr char kReportsTableSql[] =
      "CREATE TABLE new_reports("
      "report_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "source_id INTEGER NOT NULL,"
      "trigger_time INTEGER NOT NULL,"
      "report_time INTEGER NOT NULL,"
      "initial_report_time INTEGER NOT NULL,"
      "failed_send_attempts INTEGER NOT NULL,"
      "external_report_id TEXT NOT NULL,"
      "debug_key INTEGER,"
      "context_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "report_type INTEGER NOT NULL,"
      "metadata BLOB NOT NULL,"
      "context_site TEXT NOT NULL)";
  if (!db.Execute(kReportsTableSql)) {
    return false;
  }

  static constexpr char kPopulateExistingRecordsSql[] =
      "INSERT INTO new_reports SELECT "
      "report_id,source_id,trigger_time,report_time,"
      "initial_report_time,failed_send_attempts,external_report_id,"
      "debug_key,context_origin,reporting_origin,report_type,metadata,'' "
      "FROM reports";
  if (!db.Execute(kPopulateExistingRecordsSql)) {
    return false;
  }

  if (!db.Execute("DROP TABLE reports")) {
    return false;
  }

  if (!db.Execute("ALTER TABLE new_reports RENAME TO reports")) {
    return false;
  }

  static constexpr char kGetContextSite[] =
      "SELECT report_id,context_origin FROM reports";
  sql::Statement get_statement(db.GetUniqueStatement(kGetContextSite));

  static constexpr char kSetReportingSiteSql[] =
      "UPDATE reports SET context_site=? WHERE report_id=?";
  sql::Statement set_statement(db.GetUniqueStatement(kSetReportingSiteSql));

  while (get_statement.Step()) {
    AttributionReport::Id report_id(get_statement.ColumnInt64(0));
    auto context_origin = DeserializeOrigin(get_statement.ColumnStringView(1));

    set_statement.Reset(/*clear_bound_vars=*/true);
    set_statement.BindString(0, net::SchemefulSite(context_origin).Serialize());
    set_statement.BindInt64(1, *report_id);
    if (!set_statement.Run()) {
      return false;
    }
  }

  if (!get_statement.Succeeded()) {
    return false;
  }

  static constexpr char kReportsByReportTimeIndexSql[] =
      "CREATE INDEX reports_by_report_time ON reports(report_time)";
  if (!db.Execute(kReportsByReportTimeIndexSql)) {
    return false;
  }

  static constexpr char kReportsBySourceIdReportTypeIndexSql[] =
      "CREATE INDEX reports_by_source_id_report_type "
      "ON reports(source_id,report_type)";
  if (!db.Execute(kReportsBySourceIdReportTypeIndexSql)) {
    return false;
  }

  static constexpr char kReportsByTriggerTimeIndexSql[] =
      "CREATE INDEX reports_by_trigger_time ON reports(trigger_time)";
  if (!db.Execute(kReportsByTriggerTimeIndexSql)) {
    return false;
  }

  static constexpr char kReportsByReportingOriginIndexSql[] =
      "CREATE INDEX reports_by_reporting_origin "
      "ON reports(reporting_origin)WHERE report_type=2";
  if (!db.Execute(kReportsByReportingOriginIndexSql)) {
    return false;
  }

  static constexpr char kReportsByContextSiteIndexSql[] =
      "CREATE INDEX reports_by_context_site "
      "ON reports(context_site)WHERE report_type=1";
  if (!db.Execute(kReportsByContextSiteIndexSql)) {
    return false;
  }

  return true;
}

}  // namespace

bool UpgradeAttributionStorageSqlSchema(AttributionStorageSql& storage,
                                        sql::Database& db,
                                        sql::MetaTable& meta_table) {
  base::ThreadTicks start_timestamp;
  if (base::ThreadTicks::IsSupported()) {
    start_timestamp = base::ThreadTicks::Now();
  }

  static_assert(AttributionStorageSql::kDeprecatedVersionNumber + 1 == 54,
                "Remove migration(s) below.");

  bool ok = MaybeMigrate(db, meta_table, 54, &To55) &&  //
            MaybeMigrate(db, meta_table, 55, &To56) &&  //
            MaybeMigrate(db, meta_table, 56, &To57) &&
            MaybeMigrate(db, meta_table, 57, &To58) &&
            MaybeMigrate(db, meta_table, 58, &To59) &&
            MaybeMigrate(db, meta_table, 59, &To60) &&
            MaybeMigrate(db, meta_table, 60, &To61) &&
            MaybeMigrate(db, meta_table, 61, &To62) &&
            MaybeMigrate(db, meta_table, 62, &To63) &&
            MaybeMigrate(db, meta_table, 63, &To64) &&
            MaybeMigrate(db, meta_table, 64, &To65) &&
            MaybeMigrate(db, meta_table, 65, &To66) &&
            MaybeMigrate(db, meta_table, 66, &To67);
  if (!ok) {
    return false;
  }

  DeleteCorruptedReports(storage);

  static_assert(AttributionStorageSql::kCurrentVersionNumber == 67,
                "Add migration(s) above.");

  if (base::ThreadTicks::IsSupported()) {
    base::UmaHistogramMediumTimes("Conversions.Storage.MigrationTime",
                                  base::ThreadTicks::Now() - start_timestamp);
  }

  return true;
}

}  // namespace content
