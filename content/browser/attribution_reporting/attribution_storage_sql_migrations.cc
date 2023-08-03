// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"

#include <vector>

#include "base/functional/function_ref.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "content/browser/attribution_reporting/attribution_features.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "net/base/schemeful_site.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {

namespace {

const base::FeatureParam<base::TimeDelta> kFirstNavigationReportWindowDeadline{
    &blink::features::kConversionMeasurement, "first_report_window_deadline",
    base::Days(2)};

const base::FeatureParam<base::TimeDelta> kSecondNavigationReportWindowDeadline{
    &blink::features::kConversionMeasurement, "second_report_window_deadline",
    base::Days(7)};

const base::FeatureParam<base::TimeDelta> kFirstEventReportWindowDeadline{
    &blink::features::kConversionMeasurement,
    "first_event_report_window_deadline", base::Days(2)};

const base::FeatureParam<base::TimeDelta> kSecondEventReportWindowDeadline{
    &blink::features::kConversionMeasurement,
    "second_event_report_window_deadline", base::Days(7)};

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

bool To54(sql::Database& db) {
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
      "source_expiry_or_attribution_time INTEGER NOT NULL)";
  if (!db.Execute(kRateLimitTableSql)) {
    return false;
  }

  static constexpr char kPopulateSql[] =
      "INSERT INTO new_rate_limits SELECT "
      "id,scope,source_id,source_site,destination_site,context_origin,"
      "reporting_origin,'',time,source_expiry_or_attribution_time "
      "FROM rate_limits";
  if (!db.Execute(kPopulateSql)) {
    return false;
  }

  if (!db.Execute("DROP TABLE rate_limits")) {
    return false;
  }

  if (!db.Execute("ALTER TABLE new_rate_limits RENAME TO rate_limits")) {
    return false;
  }

  static constexpr char kGetReportingOriginSql[] =
      "SELECT id,reporting_origin FROM rate_limits";
  sql::Statement get_statement(db.GetUniqueStatement(kGetReportingOriginSql));

  static constexpr char kSetReportingSiteSql[] =
      "UPDATE rate_limits SET reporting_site=? WHERE id=?";
  sql::Statement set_statement(db.GetUniqueStatement(kSetReportingSiteSql));

  while (get_statement.Step()) {
    int64_t id = get_statement.ColumnInt64(0);
    auto reporting_origin = DeserializeOrigin(get_statement.ColumnString(1));

    set_statement.Reset(/*clear_bound_vars=*/true);
    set_statement.BindString(0,
                             net::SchemefulSite(reporting_origin).Serialize());
    set_statement.BindInt64(1, id);
    if (!set_statement.Run()) {
      return false;
    }
  }
  if (!get_statement.Succeeded()) {
    return false;
  }

  static constexpr char kRateLimitSourceSiteReportingSiteIndexSql[] =
      "CREATE INDEX rate_limit_source_site_reporting_site_idx "
      "ON rate_limits(source_site,reporting_site)"
      "WHERE scope=0";
  if (!db.Execute(kRateLimitSourceSiteReportingSiteIndexSql)) {
    return false;
  }

  static constexpr char kRateLimitReportingOriginIndexSql[] =
      "CREATE INDEX rate_limit_reporting_origin_idx "
      "ON rate_limits(scope,destination_site,source_site)";
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
  return db.Execute(kRateLimitImpressionIdIndexSql);
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

    int max_event_level_reports;
    std::vector<base::TimeDelta> end_times;
    switch (source_type.value()) {
      case attribution_reporting::mojom::SourceType::kNavigation:
        max_event_level_reports = 3;
        end_times = {kFirstNavigationReportWindowDeadline.Get(),
                     kSecondNavigationReportWindowDeadline.Get()};
        break;
      case attribution_reporting::mojom::SourceType::kEvent:
        if (kVTCEarlyReportingWindows.Get()) {
          base::TimeDelta first_window = kFirstEventReportWindowDeadline.Get();
          base::TimeDelta second_window =
              kSecondEventReportWindowDeadline.Get();
          if (!first_window.is_negative() && first_window < second_window) {
            end_times = {first_window, second_window};
          }
        }
        max_event_level_reports = 1;
        break;
    }

    absl::optional<attribution_reporting::EventReportWindows>
        event_report_windows =
            attribution_reporting::EventReportWindows::CreateAndTruncate(
                /*start_time=*/base::Seconds(0), std::move(end_times),
                /*expiry=*/event_report_window_time - source_time);
    if (!event_report_windows.has_value()) {
      continue;
    }

    set_statement.Reset(/*clear_bound_vars=*/true);
    set_statement.BindString(
        0, SerializeReadOnlySourceData(*event_report_windows,
                                       max_event_level_reports));
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

}  // namespace

bool UpgradeAttributionStorageSqlSchema(sql::Database& db,
                                        sql::MetaTable& meta_table) {
  base::ThreadTicks start_timestamp;
  if (base::ThreadTicks::IsSupported()) {
    start_timestamp = base::ThreadTicks::Now();
  }

  static_assert(AttributionStorageSql::kDeprecatedVersionNumber + 1 == 52,
                "Remove migration(s) below.");

  bool ok = MaybeMigrate(db, meta_table, 52, &To53) &&  //
            MaybeMigrate(db, meta_table, 53, &To54) &&  //
            MaybeMigrate(db, meta_table, 54, &To55) &&  //
            MaybeMigrate(db, meta_table, 55, &To56);
  if (!ok) {
    return false;
  }

  static_assert(AttributionStorageSql::kCurrentVersionNumber == 56,
                "Add migration(s) above.");

  if (base::ThreadTicks::IsSupported()) {
    base::UmaHistogramMediumTimes("Conversions.Storage.MigrationTime",
                                  base::ThreadTicks::Now() - start_timestamp);
  }

  return true;
}

}  // namespace content
