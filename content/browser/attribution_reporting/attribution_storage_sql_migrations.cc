// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/function_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/attribution_reporting/source_type.mojom-shared.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/rate_limit_table.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace content {

namespace {

// Ensure that both version numbers are updated together to prevent crashes on
// downgrades as in crbug.com/1413728.
[[nodiscard]] bool SetVersionNumbers(sql::MetaTable* meta_table, int version) {
  DCHECK(meta_table);
  return meta_table->SetVersionNumber(version) &&
         meta_table->SetCompatibleVersionNumber(version);
}

// Wrap each migration in its own transaction. This results in smaller
// transactions, so it's less likely that a transaction's buffer will need to
// spill to disk. Also, if the database grows a lot and Chrome stops (user
// quit, process kill, etc.) during the migration process, per-migration
// transactions make it more likely that we'll make forward progress each time
// Chrome stops.

[[nodiscard]] bool MaybeMigrate(
    sql::Database* db,
    sql::MetaTable* meta_table,
    int old_version,
    base::FunctionRef<bool(sql::Database*)> migrate) {
  DCHECK(db);
  DCHECK(meta_table);

  if (meta_table->GetVersionNumber() != old_version) {
    return true;
  }

  sql::Transaction transaction(db);

  return transaction.Begin() &&                             //
         migrate(db) &&                                     //
         SetVersionNumbers(meta_table, old_version + 1) &&  //
         transaction.Commit();
}

bool To36(sql::Database* db) {
  static constexpr char kDropOldIndexSql[] = "DROP INDEX sources_by_origin";
  if (!db->Execute(kDropOldIndexSql)) {
    return false;
  }

  static constexpr char kCreateNewIndexSql[] =
      "CREATE INDEX active_sources_by_source_origin "
      "ON sources(source_origin)"
      "WHERE event_level_active=1 OR aggregatable_active=1";
  if (!db->Execute(kCreateNewIndexSql)) {
    return false;
  }

  return true;
}

bool To37(sql::Database* db) {
  static constexpr char kNewDedupKeyTableSql[] =
      "CREATE TABLE new_dedup_keys("
      "source_id INTEGER NOT NULL,"
      "report_type INTEGER NOT NULL,"
      "dedup_key INTEGER NOT NULL,"
      "PRIMARY KEY(source_id,report_type,dedup_key))WITHOUT ROWID";
  if (!db->Execute(kNewDedupKeyTableSql)) {
    return false;
  }

  static_assert(static_cast<int>(AttributionReport::Type::kEventLevel) == 0,
                "update the report type value `0` below");

  // Transfer the existing rows to the new table, inserting
  // `Attribution::Type::kEventLevel` as default values for the
  // report_type column.
  static constexpr char kPopulateNewDedupKeyTableSql[] =
      "INSERT INTO new_dedup_keys SELECT "
      "source_id,0,dedup_key "
      "FROM dedup_keys";
  if (!db->Execute(kPopulateNewDedupKeyTableSql)) {
    return false;
  }

  static constexpr char kDropOldDedupKeyTableSql[] = "DROP TABLE dedup_keys";
  if (!db->Execute(kDropOldDedupKeyTableSql)) {
    return false;
  }

  static constexpr char kRenameDedupKeyTableSql[] =
      "ALTER TABLE new_dedup_keys RENAME TO dedup_keys";
  if (!db->Execute(kRenameDedupKeyTableSql)) {
    return false;
  }

  return true;
}

bool To38(sql::Database* db) {
  static constexpr char kNewSourceTableSql[] =
      "CREATE TABLE new_sources("
      "source_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "source_event_id INTEGER NOT NULL,"
      "source_origin TEXT NOT NULL,"
      "destination_origin TEXT NOT NULL,"
      "reporting_origin TEXT NOT NULL,"
      "source_time INTEGER NOT NULL,"
      "expiry_time INTEGER NOT NULL,"
      "event_report_window_time INTEGER NOT NULL,"
      "aggregatable_report_window_time INTEGER NOT NULL,"
      "num_attributions INTEGER NOT NULL,"
      "event_level_active INTEGER NOT NULL,"
      "aggregatable_active INTEGER NOT NULL,"
      "destination_site TEXT NOT NULL,"
      "source_type INTEGER NOT NULL,"
      "attribution_logic INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "source_site TEXT NOT NULL,"
      "debug_key INTEGER,"
      "aggregatable_budget_consumed INTEGER NOT NULL,"
      "aggregatable_source BLOB NOT NULL,"
      "filter_data BLOB NOT NULL)";
  if (!db->Execute(kNewSourceTableSql)) {
    return false;
  }

  // Transfer the existing rows to the new table, inserting
  // `expiry_time` as default values for the event_report_window_time
  // and aggregatable_report_window_time columns.
  static constexpr char kPopulateNewSourceTableSql[] =
      "INSERT INTO new_sources SELECT "
      "source_id,source_event_id,source_origin,destination_origin,"
      "reporting_origin,source_time,expiry_time,expiry_time,expiry_time,"
      "num_attributions,event_level_active,aggregatable_active,"
      "destination_site,source_type,attribution_logic,priority,"
      "source_site,debug_key,aggregatable_budget_consumed,"
      "aggregatable_source,filter_data "
      "FROM sources";
  if (!db->Execute(kPopulateNewSourceTableSql)) {
    return false;
  }

  static constexpr char kDropOldSourceTableSql[] = "DROP TABLE sources";
  if (!db->Execute(kDropOldSourceTableSql)) {
    return false;
  }

  static constexpr char kRenameSourceTableSql[] =
      "ALTER TABLE new_sources RENAME TO sources";
  if (!db->Execute(kRenameSourceTableSql)) {
    return false;
  }

  // Create the sources table indices on the new table.
  static constexpr char kConversionDestinationIndexSql[] =
      "CREATE INDEX sources_by_active_destination_site_reporting_origin "
      "ON sources"
      "(event_level_active,aggregatable_active,destination_site,"
      "reporting_origin)";
  if (!db->Execute(kConversionDestinationIndexSql)) {
    return false;
  }

  static constexpr char kImpressionExpiryIndexSql[] =
      "CREATE INDEX sources_by_expiry_time "
      "ON sources(expiry_time)";
  if (!db->Execute(kImpressionExpiryIndexSql)) {
    return false;
  }

  static constexpr char kImpressionOriginIndexSql[] =
      "CREATE INDEX active_sources_by_source_origin "
      "ON sources(source_origin)"
      "WHERE event_level_active=1 OR aggregatable_active=1";
  if (!db->Execute(kImpressionOriginIndexSql)) {
    return false;
  }

  static constexpr char kImpressionSiteReportingOriginIndexSql[] =
      "CREATE INDEX active_unattributed_sources_by_site_reporting_origin "
      "ON sources(source_site,reporting_origin)"
      "WHERE event_level_active=1 AND num_attributions=0 AND "
      "aggregatable_active=1 AND aggregatable_budget_consumed=0";
  if (!db->Execute(kImpressionSiteReportingOriginIndexSql)) {
    return false;
  }

  return true;
}

bool To39(sql::Database* db) {
  // Create the new aggregatable_report_metadata table with
  // aggregation_coordinator. This follows the steps documented at
  // https://sqlite.org/lang_altertable.html#otheralter. Other approaches, like
  // using "ALTER ... ADD COLUMN" require setting a DEFAULT value for the column
  // which is undesirable.
  static constexpr char kNewAggregatableReportMetadataTableSql[] =
      "CREATE TABLE new_aggregatable_report_metadata("
      "aggregation_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "source_id INTEGER NOT NULL,"
      "trigger_time INTEGER NOT NULL,"
      "debug_key INTEGER,"
      "external_report_id TEXT NOT NULL,"
      "report_time INTEGER NOT NULL,"
      "failed_send_attempts INTEGER NOT NULL,"
      "initial_report_time INTEGER NOT NULL,"
      "aggregation_coordinator INTEGER NOT NULL)";
  if (!db->Execute(kNewAggregatableReportMetadataTableSql)) {
    return false;
  }

  // Transfer the existing aggregatable_report_metadata rows to the new table,
  // using
  // `aggregation_service::mojom::AggregationCoordinator::kDefault`
  // for aggregation_coordinator.
  static_assert(
      static_cast<int>(
          ::aggregation_service::mojom::AggregationCoordinator::kDefault) == 0,
      "update the statement below");

  static constexpr char kPopulateNewAggregatableReportMetadataSql[] =
      "INSERT INTO new_aggregatable_report_metadata SELECT "
      "aggregation_id,source_id,trigger_time,debug_key,external_report_id,"
      "report_time,failed_send_attempts,initial_report_time,0 "
      "FROM aggregatable_report_metadata";
  if (!db->Execute(kPopulateNewAggregatableReportMetadataSql)) {
    return false;
  }

  static constexpr char kDropOldAggregatableReportMetadataTableSql[] =
      "DROP TABLE aggregatable_report_metadata";
  if (!db->Execute(kDropOldAggregatableReportMetadataTableSql)) {
    return false;
  }

  static constexpr char kRenameAggregatableReportMetadataTableSql[] =
      "ALTER TABLE new_aggregatable_report_metadata "
      "RENAME TO aggregatable_report_metadata";
  if (!db->Execute(kRenameAggregatableReportMetadataTableSql)) {
    return false;
  }

  // Create the pre-existing aggregatable_report_metadata table indices on the
  // new table.

  static constexpr char kAggregateSourceIdIndexSql[] =
      "CREATE INDEX aggregate_source_id_idx "
      "ON aggregatable_report_metadata(source_id)";
  if (!db->Execute(kAggregateSourceIdIndexSql)) {
    return false;
  }

  static constexpr char kAggregateTriggerTimeIndexSql[] =
      "CREATE INDEX aggregate_trigger_time_idx "
      "ON aggregatable_report_metadata(trigger_time)";
  if (!db->Execute(kAggregateTriggerTimeIndexSql)) {
    return false;
  }

  static constexpr char kAggregateReportTimeIndexSql[] =
      "CREATE INDEX aggregate_report_time_idx "
      "ON aggregatable_report_metadata(report_time)";
  if (!db->Execute(kAggregateReportTimeIndexSql)) {
    return false;
  }

  return true;
}

bool To40(sql::Database* db) {
  // Create the new aggregatable_contributions table with desired primary-key
  // structure.
  static constexpr char kCreateNewTableSql[] =
      "CREATE TABLE new_aggregatable_contributions("
      "aggregation_id INTEGER NOT NULL,"
      "contribution_id INTEGER NOT NULL,"
      "key_high_bits INTEGER NOT NULL,"
      "key_low_bits INTEGER NOT NULL,"
      "value INTEGER NOT NULL,"
      "PRIMARY KEY(aggregation_id,contribution_id))WITHOUT ROWID";
  if (!db->Execute(kCreateNewTableSql)) {
    return false;
  }

  // Transfer the existing aggregatable_contributions rows to the new table,
  static constexpr char kPopulateNewTableSql[] =
      "INSERT INTO new_aggregatable_contributions SELECT "
      "aggregation_id,contribution_id,key_high_bits,key_low_bits,value "
      "FROM aggregatable_contributions";
  if (!db->Execute(kPopulateNewTableSql)) {
    return false;
  }

  // This implicitly drops the contribution_aggregation_id_idx index.
  static constexpr char kDropOldTableSql[] =
      "DROP TABLE aggregatable_contributions";
  if (!db->Execute(kDropOldTableSql)) {
    return false;
  }

  static constexpr char kRenameTableSql[] =
      "ALTER TABLE new_aggregatable_contributions "
      "RENAME TO aggregatable_contributions";
  if (!db->Execute(kRenameTableSql)) {
    return false;
  }

  return true;
}

bool To41(sql::Database* db) {
  static constexpr char kAddAttestationHeaderColumnSql[] =
      "ALTER TABLE aggregatable_report_metadata "
      "ADD COLUMN attestation_token TEXT";
  if (!db->Execute(kAddAttestationHeaderColumnSql)) {
    return false;
  }

  return true;
}

bool To42(sql::Database* db) {
  static constexpr char kRenameDestinationOriginSql[] =
      "ALTER TABLE rate_limits "
      "RENAME COLUMN destination_origin TO context_origin";
  if (!db->Execute(kRenameDestinationOriginSql)) {
    return false;
  }

  static_assert(static_cast<int>(RateLimitTable::Scope::kSource) == 0);

  static constexpr char kSetContextOriginSql[] =
      "UPDATE rate_limits SET context_origin=source_origin WHERE scope=0";
  if (!db->Execute(kSetContextOriginSql)) {
    return false;
  }

  static constexpr char kDropSourceOriginSql[] =
      "ALTER TABLE rate_limits DROP COLUMN source_origin";
  if (!db->Execute(kDropSourceOriginSql)) {
    return false;
  }

  return true;
}

bool To43(sql::Database* db) {
  static constexpr char kRenameExpiryTimeSql[] =
      "ALTER TABLE rate_limits "
      "RENAME COLUMN expiry_time TO source_expiry_or_attribution_time";
  if (!db->Execute(kRenameExpiryTimeSql)) {
    return false;
  }

  static_assert(static_cast<int>(RateLimitTable::Scope::kAttribution) == 1);

  static constexpr char kSetAttributionTimeSql[] =
      "UPDATE rate_limits "
      "SET source_expiry_or_attribution_time=time WHERE scope=1";
  if (!db->Execute(kSetAttributionTimeSql)) {
    return false;
  }

  return true;
}

bool To44(sql::Database* db) {
  {
    static constexpr char kConversionTableSql[] =
        "CREATE TABLE new_event_level_reports("
        "report_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
        "source_id INTEGER NOT NULL,"
        "trigger_data INTEGER NOT NULL,"
        "trigger_time INTEGER NOT NULL,"
        "report_time INTEGER NOT NULL,"
        "priority INTEGER NOT NULL,"
        "failed_send_attempts INTEGER NOT NULL,"
        "external_report_id TEXT NOT NULL,"
        "debug_key INTEGER,"
        "destination_origin TEXT NOT NULL)";
    if (!db->Execute(kConversionTableSql)) {
      return false;
    }

    // Use the destination site as the destination origin since we don't have
    // finer-grained data available.
    static constexpr char kInsertReportsSql[] =
        "INSERT INTO new_event_level_reports "
        "SELECT R.report_id,R.source_id,R.trigger_data,R.trigger_time,"
        "R.report_time,R.priority,R.failed_send_attempts,R.external_report_id,"
        "R.debug_key,I.destination_site "
        "FROM event_level_reports R "
        "JOIN sources I ON I.source_id=R.source_id";
    if (!db->Execute(kInsertReportsSql)) {
      return false;
    }

    if (!db->Execute("DROP TABLE event_level_reports")) {
      return false;
    }

    static constexpr char kRenameSql[] =
        "ALTER TABLE new_event_level_reports "
        "RENAME TO event_level_reports";
    if (!db->Execute(kRenameSql)) {
      return false;
    }

    static constexpr char kConversionReportTimeIndexSql[] =
        "CREATE INDEX event_level_reports_by_report_time "
        "ON event_level_reports(report_time)";
    if (!db->Execute(kConversionReportTimeIndexSql)) {
      return false;
    }

    static constexpr char kConversionImpressionIdIndexSql[] =
        "CREATE INDEX event_level_reports_by_source_id "
        "ON event_level_reports(source_id)";
    if (!db->Execute(kConversionImpressionIdIndexSql)) {
      return false;
    }
  }

  {
    static constexpr char kAggregatableReportMetadataTableSql[] =
        "CREATE TABLE new_aggregatable_report_metadata("
        "aggregation_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
        "source_id INTEGER NOT NULL,"
        "trigger_time INTEGER NOT NULL,"
        "debug_key INTEGER,"
        "external_report_id TEXT NOT NULL,"
        "report_time INTEGER NOT NULL,"
        "failed_send_attempts INTEGER NOT NULL,"
        "initial_report_time INTEGER NOT NULL,"
        "aggregation_coordinator INTEGER NOT NULL,"
        "attestation_token TEXT,"
        "destination_origin TEXT NOT NULL)";
    if (!db->Execute(kAggregatableReportMetadataTableSql)) {
      return false;
    }

    // Use the destination site as the destination origin since we don't have
    // finer-grained data available.
    static constexpr char kInsertReportsSql[] =
        "INSERT INTO new_aggregatable_report_metadata "
        "SELECT R.aggregation_id,R.source_id,R.trigger_time,R.debug_key,"
        "R.external_report_id,R.report_time,R.failed_send_attempts,"
        "R.initial_report_time,R.aggregation_coordinator,R.attestation_token,"
        "I.destination_site "
        "FROM aggregatable_report_metadata R "
        "JOIN sources I ON I.source_id=R.source_id";
    if (!db->Execute(kInsertReportsSql)) {
      return false;
    }

    if (!db->Execute("DROP TABLE aggregatable_report_metadata")) {
      return false;
    }

    static constexpr char kRenameSql[] =
        "ALTER TABLE new_aggregatable_report_metadata "
        "RENAME TO aggregatable_report_metadata";
    if (!db->Execute(kRenameSql)) {
      return false;
    }

    static constexpr char kAggregateSourceIdIndexSql[] =
        "CREATE INDEX aggregate_source_id_idx "
        "ON aggregatable_report_metadata(source_id)";
    if (!db->Execute(kAggregateSourceIdIndexSql)) {
      return false;
    }

    static constexpr char kAggregateTriggerTimeIndexSql[] =
        "CREATE INDEX aggregate_trigger_time_idx "
        "ON aggregatable_report_metadata(trigger_time)";
    if (!db->Execute(kAggregateTriggerTimeIndexSql)) {
      return false;
    }

    static constexpr char kAggregateReportTimeIndexSql[] =
        "CREATE INDEX aggregate_report_time_idx "
        "ON aggregatable_report_metadata(report_time)";
    if (!db->Execute(kAggregateReportTimeIndexSql)) {
      return false;
    }
  }

  return true;
}

bool To45(sql::Database* db) {
  static constexpr char kRenameSql[] =
      "ALTER TABLE event_level_reports "
      "RENAME COLUMN destination_origin TO context_origin";
  if (!db->Execute(kRenameSql)) {
    return false;
  }

  return true;
}

bool To46(sql::Database* db) {
  if (!db->Execute("ALTER TABLE sources DROP COLUMN destination_origin")) {
    return false;
  }

  return true;
}

bool To47(sql::Database* db) {
  static constexpr char kSourceDestinationsTableSql[] =
      "CREATE TABLE source_destinations("
      "source_id INTEGER NOT NULL,"
      "destination_site TEXT NOT NULL,"
      "PRIMARY KEY(source_id,destination_site))WITHOUT ROWID";
  if (!db->Execute(kSourceDestinationsTableSql)) {
    return false;
  }

  static constexpr char kInsertDestinationsSql[] =
      "INSERT INTO source_destinations "
      "SELECT source_id,destination_site "
      "FROM sources";
  if (!db->Execute(kInsertDestinationsSql)) {
    return false;
  }

  static constexpr char kDropDestinationSiteIndexSql[] =
      "DROP INDEX sources_by_active_destination_site_reporting_origin";
  if (!db->Execute(kDropDestinationSiteIndexSql)) {
    return false;
  }

  if (!db->Execute("ALTER TABLE sources DROP COLUMN destination_site")) {
    return false;
  }

  static constexpr char kSourcesByActiveReportingOriginIndexSql[] =
      "CREATE INDEX sources_by_active_reporting_origin "
      "ON sources(event_level_active,"
      "aggregatable_active,reporting_origin)";
  if (!db->Execute(kSourcesByActiveReportingOriginIndexSql)) {
    return false;
  }

  static constexpr char kSourceDestinationsIndexSql[] =
      "CREATE INDEX sources_by_destination_site "
      "ON source_destinations(destination_site)";
  if (!db->Execute(kSourceDestinationsIndexSql)) {
    return false;
  }

  static constexpr char kDropObsoleteIndexSql[] =
      "DROP INDEX active_unattributed_sources_by_site_reporting_origin";
  if (!db->Execute(kDropObsoleteIndexSql)) {
    return false;
  }

  return true;
}

bool To48(sql::Database* db) {
  static constexpr char kConversionTableSql[] =
      "CREATE TABLE new_event_level_reports("
      "report_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "source_id INTEGER NOT NULL,"
      "trigger_data INTEGER NOT NULL,"
      "trigger_time INTEGER NOT NULL,"
      "report_time INTEGER NOT NULL,"
      "initial_report_time INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "failed_send_attempts INTEGER NOT NULL,"
      "external_report_id TEXT NOT NULL,"
      "debug_key INTEGER,"
      "context_origin TEXT NOT NULL)";
  if (!db->Execute(kConversionTableSql)) {
    return false;
  }

  static constexpr char kInsertReportsSql[] =
      "INSERT INTO new_event_level_reports "
      "SELECT report_id,source_id,trigger_data,trigger_time,"
      "report_time,report_time,priority,failed_send_attempts,"
      "external_report_id,"
      "debug_key,context_origin "
      "FROM event_level_reports";
  if (!db->Execute(kInsertReportsSql)) {
    return false;
  }

  static constexpr char kRetrieveReportTimes[] =
      "SELECT I.source_time,I.event_report_window_time,"
      "R.trigger_time,I.source_type,R.report_id "
      "FROM event_level_reports R "
      "JOIN sources I "
      "ON I.source_id=R.source_id";
  sql::Statement select_statement(db->GetUniqueStatement(kRetrieveReportTimes));

  static constexpr char kUpdateOriginalTime[] =
      "UPDATE new_event_level_reports SET initial_report_time=? "
      "WHERE report_id=?";
  sql::Statement update_statement(db->GetUniqueStatement(kUpdateOriginalTime));

  while (select_statement.Step()) {
    base::Time initial_report_time;
    int source_type = select_statement.ColumnInt(3);
    base::Time source_time = select_statement.ColumnTime(0);
    base::Time event_report_window_time = select_statement.ColumnTime(1);
    base::Time trigger_time = select_statement.ColumnTime(2);
    switch (source_type) {
      case static_cast<int>(
          attribution_reporting::mojom::SourceType::kNavigation):
        initial_report_time = ComputeReportTime(
            source_time, event_report_window_time, trigger_time,
            /*early_deadlines=*/
            base::span<const base::TimeDelta>({base::Days(2), base::Days(7)}));
        break;
      case static_cast<int>(attribution_reporting::mojom::SourceType::kEvent):
        initial_report_time = ComputeReportTime(
            source_time, event_report_window_time, trigger_time,
            /*early_deadlines=*/{});
        break;
      default:
        continue;
    }

    update_statement.Reset(/*clear_bound_vars=*/true);
    update_statement.BindTime(0, initial_report_time);
    update_statement.BindInt64(1, select_statement.ColumnInt64(4));
    if (!update_statement.Run()) {
      return false;
    }
  }

  if (!db->Execute("DROP TABLE event_level_reports")) {
    return false;
  }

  static constexpr char kRenameSql[] =
      "ALTER TABLE new_event_level_reports "
      "RENAME TO event_level_reports";
  if (!db->Execute(kRenameSql)) {
    return false;
  }

  static constexpr char kConversionReportTimeIndexSql[] =
      "CREATE INDEX event_level_reports_by_report_time "
      "ON event_level_reports(report_time)";
  if (!db->Execute(kConversionReportTimeIndexSql)) {
    return false;
  }

  static constexpr char kConversionImpressionIdIndexSql[] =
      "CREATE INDEX event_level_reports_by_source_id "
      "ON event_level_reports(source_id)";
  if (!db->Execute(kConversionImpressionIdIndexSql)) {
    return false;
  }

  return true;
}

}  // namespace

bool UpgradeAttributionStorageSqlSchema(sql::Database* db,
                                        sql::MetaTable* meta_table) {
  DCHECK(db);
  DCHECK(meta_table);

  base::ThreadTicks start_timestamp;
  if (base::ThreadTicks::IsSupported()) {
    start_timestamp = base::ThreadTicks::Now();
  }

  static_assert(AttributionStorageSql::kDeprecatedVersionNumber + 1 == 35,
                "Remove migration(s) below.");

  bool ok = MaybeMigrate(db, meta_table, 35, &To36) &&
            MaybeMigrate(db, meta_table, 36, &To37) &&
            MaybeMigrate(db, meta_table, 37, &To38) &&
            MaybeMigrate(db, meta_table, 38, &To39) &&
            MaybeMigrate(db, meta_table, 39, &To40) &&
            MaybeMigrate(db, meta_table, 40, &To41) &&
            MaybeMigrate(db, meta_table, 41, &To42) &&
            MaybeMigrate(db, meta_table, 42, &To43) &&
            MaybeMigrate(db, meta_table, 43, &To44) &&
            MaybeMigrate(db, meta_table, 44, &To45) &&
            MaybeMigrate(db, meta_table, 45, &To46) &&
            MaybeMigrate(db, meta_table, 46, &To47) &&
            MaybeMigrate(db, meta_table, 47, &To48);
  if (!ok) {
    return false;
  }

  static_assert(AttributionStorageSql::kCurrentVersionNumber == 48,
                "Add migration(s) above.");

  if (base::ThreadTicks::IsSupported()) {
    base::UmaHistogramMediumTimes("Conversions.Storage.MigrationTime",
                                  base::ThreadTicks::Now() - start_timestamp);
  }

  return true;
}

}  // namespace content
