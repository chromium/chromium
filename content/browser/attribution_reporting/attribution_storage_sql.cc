// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql.h"

#include <stdint.h>

#include <limits>
#include <string>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/aggregatable_attribution.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "sql/database.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/origin.h"

namespace content {

// Version number of the database.
//
// Version 1 - 2020/01/27 - https://crrev.com/c/1965450
//
// Version 2 - 2020/11/03 - https://crrev.com/c/2194182
//
// Version 2 adds a new impression column "conversion_destination" based on
// registerable domain which is used for attribution instead of
// "conversion_origin".
//
// Version 3 - 2021/03/08 - https://crrev.com/c/2743337
//
// Version 3 adds new impression columns source_type and attributed_truthfully.
//
// Version 4 - 2021/03/16 - https://crrev.com/c/2716913
//
// Version 4 adds a new rate_limits table.
//
// Version 5 - 2021/04/30 - https://crrev.com/c/2860056
//
// Version 5 drops the conversions.attribution_credit column.
//
// Version 6 - 2021/05/06 - https://crrev.com/c/2878235
//
// Version 6 adds the impressions.priority column.
//
// Version 7 - 2021/06/03 - https://crrev.com/c/2904386
//
// Version 7 adds the impressions.impression_site column.
//
// Version 8 - 2021/06/30 - https://crrev.com/c/2888906
//
// Version 8 changes the conversions.conversion_data and
// impressions.impression_data columns from TEXT to INTEGER and makes
// conversions.impression_id NOT NULL.
//
// Version 9 - 2021/06/30 - https://crrev.com/c/2951620
//
// Version 9 adds the conversions.priority column.
//
// Version 10 - 2021/07/16 - https://crrev.com/c/2983439
//
// Version 10 adds the dedup_keys table.
//
// Version 11 - 2021/08/10 - https://crrev.com/c/3087755
//
// Version 11 replaces impression_site_idx with
// event_source_impression_site_idx, which stores less data.
//
// Version 12 - 2021/08/18 - https://crrev.com/c/3085887
//
// Version 12 adds the rate_limits.bucket and rate_limits.value columns and
// makes rate_limits.rate_limit_id NOT NULL.
//
// Version 13 - 2021/09/08 - https://crrev.com/c/3149550
//
// Version 13 makes the impressions.impression_id and conversions.conversion_id
// columns NOT NULL and AUTOINCREMENT, the latter to prevent ID reuse, which is
// prone to race conditions with the queuing logic vs deletions.
//
// Version 14 - 2021/09/22 - https://crrev.com/c/3138353
//
// Version 14 adds the conversions.failed_send_attempts column.
//
// Version 15 - 2021/11/13 - https://crrev.com/c/3180180
//
// Version 15 adds the conversions.external_report_id column.
//
// Version 16 - 2022/01/31 - https://crrev.com/c/3421414
//
// Version 16 replaces the event_source_impression_site_idx with
// impression_site_reporting_origin_idx, which applies to both source types and
// includes the reporting origin.
//
// Version 17 - 2022/01/31 - https://crrev.com/c/3427311
//
// Version 17 removes the rate_limits.bucket and rate_limits.value columns.
//
// Version 18 - 2022/02/04 - https://crrev.com/c/3425176
//
// Version 18 adds the rate_limits.reporting_origin column and removes the
// rate_limits.attribution_type column.
//
// Version 19 - 2022/02/07 - https://crrev.com/c/3421868
//
// Version 19 adds the impressions.debug_key and conversions.debug_key columns.
//
// Version 20 - 2022/02/07 - https://crrev.com/c/3444062
//
// Version 20 adds the rate_limits.scope column and corresponding indexes.
//
// Version 21 - 2022/02/16 - https://crrev.com/c/3465916
//
// Version 21 changes the dedup_keys.dedup_key column from int64_t to uint64_t.
//
// Version 22 - 2022/02/16 - https://crrev.com/c/3463875
//
// Version 22 renames rate_limit_report_idx to rate_limit_attribution_idx.
//
// Version 23 - 2022/02/17 - https://crrev.com/c/3379484
//
// Version 23 adds the aggregatable_report_metadata and
// aggregatable_contributions tables.
const int AttributionStorageSql::kCurrentVersionNumber = 23;

// Earliest version which can use a |kCurrentVersionNumber| database
// without failing.
const int AttributionStorageSql::kCompatibleVersionNumber = 23;

// Latest version of the database that cannot be upgraded to
// |kCurrentVersionNumber| without razing the database.
//
// Versions 1-14 were deprecated by https://crrev.com/c/3421175.
//
// Version 15 was deprecated by https://crrev.com/c/3421414.
//
// Version 16 was deprecated by https://crrev.com/c/3427311.
//
// Version 17 was deprecated by https://crrev.com/c/3425176.
//
// Version 18 was deprecated by https://crrev.com/c/3421868.
//
// Version 19 was deprecated by https://crrev.com/c/3444062.
//
// Version 20 was deprecated by https://crrev.com/c/3465916.
//
// Version 21 was deprecated by https://crrev.com/c/3463875.
//
// Version 22 was deprecated by https://crrev.com/c/3379484.
//
// Note that Versions 15-22 were introduced during the transitional state of
// the Attribution Reporting API and can be removed when done.
const int AttributionStorageSql::kDeprecatedVersionNumber = 22;

namespace {

using CreateReportResult = ::content::AttributionStorage::CreateReportResult;
using DeactivatedSource = ::content::AttributionStorage::DeactivatedSource;

const base::FilePath::CharType kInMemoryPath[] = FILE_PATH_LITERAL(":memory");

const base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("Conversions");

void RecordInitializationStatus(
    const AttributionStorageSql::InitStatus status) {
  base::UmaHistogramEnumeration("Conversions.Storage.Sql.InitStatus2", status);
}

void RecordSourcesDeleted(int count) {
  UMA_HISTOGRAM_COUNTS_1000(
      "Conversions.ImpressionsDeletedInDataClearOperation", count);
}

void RecordReportsDeleted(int count) {
  UMA_HISTOGRAM_COUNTS_1000("Conversions.ReportsDeletedInDataClearOperation",
                            count);
}

int SerializeAttributionLogic(StoredSource::AttributionLogic val) {
  return static_cast<int>(val);
}

absl::optional<StoredSource::AttributionLogic> DeserializeAttributionLogic(
    int val) {
  switch (val) {
    case static_cast<int>(StoredSource::AttributionLogic::kNever):
      return StoredSource::AttributionLogic::kNever;
    case static_cast<int>(StoredSource::AttributionLogic::kTruthfully):
      return StoredSource::AttributionLogic::kTruthfully;
    case static_cast<int>(StoredSource::AttributionLogic::kFalsely):
      return StoredSource::AttributionLogic::kFalsely;
    default:
      return absl::nullopt;
  }
}

int SerializeSourceType(CommonSourceInfo::SourceType val) {
  return static_cast<int>(val);
}

absl::optional<CommonSourceInfo::SourceType> DeserializeSourceType(int val) {
  switch (val) {
    case static_cast<int>(CommonSourceInfo::SourceType::kNavigation):
      return CommonSourceInfo::SourceType::kNavigation;
    case static_cast<int>(CommonSourceInfo::SourceType::kEvent):
      return CommonSourceInfo::SourceType::kEvent;
    default:
      return absl::nullopt;
  }
}

void BindUint64OrNull(sql::Statement& statement,
                      int col,
                      absl::optional<uint64_t> value) {
  if (value.has_value())
    statement.BindInt64(col, SerializeUint64(*value));
  else
    statement.BindNull(col);
}

absl::optional<uint64_t> ColumnUint64OrNull(sql::Statement& statement,
                                            int col) {
  return statement.GetColumnType(col) == sql::ColumnType::kNull
             ? absl::nullopt
             : absl::make_optional(
                   DeserializeUint64(statement.ColumnInt64(col)));
}

struct SourceToAttribute {
  StoredSource source;
  int num_conversions;
};

absl::optional<SourceToAttribute> ReadSourceToAttribute(
    sql::Database* db,
    StoredSource::Id source_id,
    const url::Origin& reporting_origin) {
  static constexpr char kReadSourceToAttributeSql[] =
      "SELECT impression_origin,impression_time,priority,"
      "conversion_origin,attributed_truthfully,source_type,num_conversions,"
      "impression_data,expiry_time,debug_key "
      "FROM impressions "
      "WHERE impression_id = ?";
  sql::Statement statement(
      db->GetCachedStatement(SQL_FROM_HERE, kReadSourceToAttributeSql));
  statement.BindInt64(0, *source_id);
  if (!statement.Step())
    return absl::nullopt;

  url::Origin impression_origin = DeserializeOrigin(statement.ColumnString(0));
  if (impression_origin.opaque())
    return absl::nullopt;

  base::Time impression_time = statement.ColumnTime(1);
  int64_t priority = statement.ColumnInt64(2);

  url::Origin conversion_origin = DeserializeOrigin(statement.ColumnString(3));
  if (conversion_origin.opaque())
    return absl::nullopt;

  absl::optional<StoredSource::AttributionLogic> attribution_logic =
      DeserializeAttributionLogic(statement.ColumnInt(4));
  // There should never be an unattributed source with `kFalsely`.
  if (!attribution_logic.has_value() ||
      attribution_logic == StoredSource::AttributionLogic::kFalsely) {
    return absl::nullopt;
  }

  absl::optional<CommonSourceInfo::SourceType> source_type =
      DeserializeSourceType(statement.ColumnInt(5));
  if (!source_type.has_value())
    return absl::nullopt;

  int num_conversions = statement.ColumnInt(6);
  if (num_conversions < 0)
    return absl::nullopt;

  uint64_t source_event_id = DeserializeUint64(statement.ColumnInt64(7));
  base::Time expiry_time = statement.ColumnTime(8);
  absl::optional<uint64_t> debug_key = ColumnUint64OrNull(statement, 9);

  return SourceToAttribute{
      .source = StoredSource(
          CommonSourceInfo(source_event_id, std::move(impression_origin),
                           std::move(conversion_origin), reporting_origin,
                           impression_time, expiry_time, *source_type, priority,
                           debug_key),
          *attribution_logic, source_id),
      .num_conversions = num_conversions,
  };
}

// Helper to deserialize source rows. See `GetActiveSources()` for the
// expected ordering of columns used for the input to this function.
absl::optional<StoredSource> ReadSourceFromStatement(
    sql::Statement& statement) {
  DCHECK_EQ(statement.ColumnCount(), 11);

  StoredSource::Id source_id(statement.ColumnInt64(0));
  uint64_t source_event_id = DeserializeUint64(statement.ColumnInt64(1));
  url::Origin impression_origin = DeserializeOrigin(statement.ColumnString(2));
  url::Origin conversion_origin = DeserializeOrigin(statement.ColumnString(3));
  url::Origin reporting_origin = DeserializeOrigin(statement.ColumnString(4));
  base::Time impression_time = statement.ColumnTime(5);
  base::Time expiry_time = statement.ColumnTime(6);
  absl::optional<CommonSourceInfo::SourceType> source_type =
      DeserializeSourceType(statement.ColumnInt(7));
  absl::optional<StoredSource::AttributionLogic> attribution_logic =
      DeserializeAttributionLogic(statement.ColumnInt(8));
  int64_t priority = statement.ColumnInt64(9);
  absl::optional<uint64_t> debug_key = ColumnUint64OrNull(statement, 10);

  if (!source_type.has_value() || !attribution_logic.has_value())
    return absl::nullopt;

  return StoredSource(
      CommonSourceInfo(source_event_id, std::move(impression_origin),
                       std::move(conversion_origin),
                       std::move(reporting_origin), impression_time,
                       expiry_time, *source_type, priority, debug_key),
      *attribution_logic, source_id);
}

}  // namespace

// static
void AttributionStorageSql::RunInMemoryForTesting() {
  g_run_in_memory_ = true;
}

// static
bool AttributionStorageSql::g_run_in_memory_ = false;

AttributionStorageSql::AttributionStorageSql(
    const base::FilePath& path_to_database,
    std::unique_ptr<AttributionStorageDelegate> delegate)
    : path_to_database_(g_run_in_memory_
                            ? base::FilePath(kInMemoryPath)
                            : path_to_database.Append(kDatabasePath)),
      rate_limit_table_(delegate.get()),
      delegate_(std::move(delegate)),
      weak_factory_(this) {
  DCHECK(delegate_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AttributionStorageSql::~AttributionStorageSql() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

absl::optional<std::vector<DeactivatedSource>>
AttributionStorageSql::DeactivateSources(
    const std::string& serialized_conversion_destination,
    const std::string& serialized_reporting_origin,
    int return_limit) {
  std::vector<DeactivatedSource> deactivated_sources;

  if (return_limit != 0) {
    // Get at most `return_limit` sources that will be deactivated. We do this
    // first, instead of using a RETURNING clause in the UPDATE, because we
    // cannot limit the number of returned results there, and we want to avoid
    // bringing all results into memory.
    static constexpr char kGetSourcesToReturnSql[] =
        "SELECT impression_id,impression_data,impression_origin,"
        "conversion_origin,reporting_origin,impression_time,expiry_time,"
        "source_type,attributed_truthfully,priority,debug_key "
        "FROM impressions "
        DCHECK_SQL_INDEXED_BY("conversion_destination_idx")
        "WHERE conversion_destination = ? AND reporting_origin = ? AND "
        "active = 1 AND num_conversions > 0 LIMIT ?";
    sql::Statement get_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kGetSourcesToReturnSql));
    get_statement.BindString(0, serialized_conversion_destination);
    get_statement.BindString(1, serialized_reporting_origin);
    get_statement.BindInt(2, return_limit);

    while (get_statement.Step()) {
      absl::optional<StoredSource> source =
          ReadSourceFromStatement(get_statement);
      if (!source.has_value())
        return absl::nullopt;

      deactivated_sources.emplace_back(
          std::move(*source),
          DeactivatedSource::Reason::kReplacedByNewerSource);
    }
    if (!get_statement.Succeeded())
      return absl::nullopt;

    // If nothing was returned, we know the UPDATE below will do nothing, so
    // just return early.
    if (deactivated_sources.empty())
      return deactivated_sources;
  }

  static constexpr char kDeactivateSourcesSql[] =
      "UPDATE impressions "
      DCHECK_SQL_INDEXED_BY("conversion_destination_idx")
      "SET active = 0 "
      "WHERE conversion_destination = ? AND reporting_origin = ? AND "
      "active = 1 AND num_conversions > 0";
  sql::Statement deactivate_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeactivateSourcesSql));
  deactivate_statement.BindString(0, serialized_conversion_destination);
  deactivate_statement.BindString(1, serialized_reporting_origin);

  if (!deactivate_statement.Run())
    return absl::nullopt;

  for (auto& deactivated_source : deactivated_sources) {
    absl::optional<std::vector<uint64_t>> dedup_keys =
        ReadDedupKeys(deactivated_source.source.source_id());
    if (!dedup_keys.has_value())
      return absl::nullopt;
    deactivated_source.source.SetDedupKeys(std::move(*dedup_keys));
  }

  return deactivated_sources;
}

AttributionStorage::StoreSourceResult AttributionStorageSql::StoreSource(
    const StorableSource& source,
    int deactivated_source_return_limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Force the creation of the database if it doesn't exist, as we need to
  // persist the source.
  if (!LazyInit(DbCreationPolicy::kCreateIfAbsent))
    return StoreSourceResult(StorableSource::Result::kInternalError);

  // Only delete expired impressions periodically to avoid excessive DB
  // operations.
  const base::TimeDelta delete_frequency =
      delegate_->GetDeleteExpiredSourcesFrequency();
  DCHECK_GE(delete_frequency, base::TimeDelta());
  const base::Time now = base::Time::Now();
  if (now - last_deleted_expired_sources_ >= delete_frequency) {
    if (!DeleteExpiredSources())
      return StoreSourceResult(StorableSource::Result::kInternalError);
    last_deleted_expired_sources_ = now;
  }

  const CommonSourceInfo& common_info = source.common_info();

  // TODO(csharrison): Thread this failure to the caller and report a console
  // error.
  const std::string serialized_impression_origin =
      SerializeOrigin(common_info.impression_origin());
  if (!HasCapacityForStoringSource(serialized_impression_origin)) {
    return StoreSourceResult(
        StorableSource::Result::kInsufficientSourceCapacity);
  }

  if (!HasCapacityForUniqueDestinationLimitForPendingSource(source)) {
    return StoreSourceResult(
        StorableSource::Result::kInsufficientUniqueDestinationCapacity);
  }

  switch (rate_limit_table_.SourceAllowedForReportingOriginLimit(db_.get(),
                                                                 source)) {
    case RateLimitTable::Result::kAllowed:
      break;
    case RateLimitTable::Result::kNotAllowed:
      return StoreSourceResult(
          StorableSource::Result::kExcessiveReportingOrigins);
    case RateLimitTable::Result::kError:
      return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  // Wrap the deactivation and insertion in the same transaction. If the
  // deactivation fails, we do not want to store the new source as we may
  // return the wrong set of sources for a trigger.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return StoreSourceResult(StorableSource::Result::kInternalError);

  const std::string serialized_conversion_destination =
      common_info.ConversionDestination().Serialize();
  const std::string serialized_reporting_origin =
      SerializeOrigin(common_info.reporting_origin());

  // In the case where we get a new source for a given <reporting_origin,
  // conversion_destination> we should mark all active, converted impressions
  // with the matching <reporting_origin, conversion_destination> as not active.
  absl::optional<std::vector<DeactivatedSource>> deactivated_sources =
      DeactivateSources(serialized_conversion_destination,
                        serialized_reporting_origin,
                        deactivated_source_return_limit);
  if (!deactivated_sources.has_value())
    return StoreSourceResult(StorableSource::Result::kInternalError);

  AttributionStorageDelegate::RandomizedResponse randomized_response =
      delegate_->GetRandomizedResponse(common_info);

  int num_conversions = 0;
  auto attribution_logic = StoredSource::AttributionLogic::kTruthfully;
  bool active = true;
  if (randomized_response.has_value()) {
    num_conversions = randomized_response->size();
    attribution_logic = num_conversions == 0
                            ? StoredSource::AttributionLogic::kNever
                            : StoredSource::AttributionLogic::kFalsely;
    active = num_conversions == 0;
  }

  static constexpr char kInsertImpressionSql[] =
      "INSERT INTO impressions"
      "(impression_data,impression_origin,conversion_origin,"
      "conversion_destination,"
      "reporting_origin,impression_time,expiry_time,source_type,"
      "attributed_truthfully,priority,impression_site,"
      "num_conversions,active,debug_key)"
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertImpressionSql));
  statement.BindInt64(0, SerializeUint64(common_info.source_event_id()));
  statement.BindString(1, serialized_impression_origin);
  statement.BindString(2, SerializeOrigin(common_info.conversion_origin()));
  statement.BindString(3, serialized_conversion_destination);
  statement.BindString(4, serialized_reporting_origin);
  statement.BindTime(5, common_info.impression_time());
  statement.BindTime(6, common_info.expiry_time());
  statement.BindInt(7, SerializeSourceType(common_info.source_type()));
  statement.BindInt(8, SerializeAttributionLogic(attribution_logic));
  statement.BindInt64(9, common_info.priority());
  statement.BindString(10, common_info.ImpressionSite().Serialize());
  statement.BindInt(11, num_conversions);
  statement.BindBool(12, active);

  BindUint64OrNull(statement, 13, common_info.debug_key());

  if (!statement.Run())
    return StoreSourceResult(StorableSource::Result::kInternalError);

  const StoredSource::Id source_id(db_->GetLastInsertRowId());
  const StoredSource stored_source(source.common_info(), attribution_logic,
                                   source_id);

  if (!rate_limit_table_.AddRateLimitForSource(db_.get(), stored_source))
    return StoreSourceResult(StorableSource::Result::kInternalError);

  absl::optional<base::Time> min_fake_report_time;

  if (attribution_logic == StoredSource::AttributionLogic::kFalsely) {
    const base::Time trigger_time = common_info.impression_time();

    for (const auto& fake_report : *randomized_response) {
      if (!StoreReport(source_id, fake_report.trigger_data, trigger_time,
                       fake_report.report_time,
                       /*priority=*/0, delegate_->NewReportID(),
                       /*trigger_debug_key=*/absl::nullopt)) {
        return StoreSourceResult(StorableSource::Result::kInternalError);
      }

      if (!min_fake_report_time.has_value() ||
          fake_report.report_time < *min_fake_report_time) {
        min_fake_report_time = fake_report.report_time;
      }
    }
  }

  if (!transaction.Commit())
    return StoreSourceResult(StorableSource::Result::kInternalError);

  return StoreSourceResult(StorableSource::Result::kSuccess,
                           std::move(*deactivated_sources),
                           min_fake_report_time);
}

// Checks whether a new report is allowed to be stored for the given source
// based on `GetMaxAttributionsPerSource()`. If there's sufficient capacity,
// the new report should be stored. Otherwise, if all existing reports were from
// an earlier window, the corresponding source is deactivated and the new
// report should be dropped. Otherwise, If there's insufficient capacity, checks
// the new report's priority against all existing ones for the same source.
// If all existing ones have greater priority, the new report should be dropped;
// otherwise, the existing one with the lowest priority is deleted and the new
// one should be stored.
AttributionStorageSql::MaybeReplaceLowerPriorityEventLevelReportResult
AttributionStorageSql::MaybeReplaceLowerPriorityEventLevelReport(
    const AttributionReport& report,
    int num_conversions,
    int64_t conversion_priority,
    absl::optional<AttributionReport>& replaced_report) {
  DCHECK_GE(num_conversions, 0);

  const StoredSource& source = report.attribution_info().source;

  // If there's already capacity for the new report, there's nothing to do.
  if (num_conversions < delegate_->GetMaxAttributionsPerSource(
                            source.common_info().source_type())) {
    return MaybeReplaceLowerPriorityEventLevelReportResult::kAddNewReport;
  }

  // Prioritization is scoped within report windows.
  // This is reasonably optimized as is because we only store a ~small number
  // of reports per impression_id. Selects the report with lowest priority,
  // and uses the greatest conversion_time to break ties. This favors sending
  // reports for report closer to the source time.
  static constexpr char kMinPrioritySql[] =
      "SELECT priority,conversion_id "
      "FROM conversions "
      "WHERE impression_id = ? AND report_time = ? "
      "ORDER BY priority ASC, conversion_time DESC "
      "LIMIT 1";
  sql::Statement min_priority_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kMinPrioritySql));
  min_priority_statement.BindInt64(0, *source.source_id());
  min_priority_statement.BindTime(1, report.report_time());

  const bool has_matching_report = min_priority_statement.Step();
  if (!min_priority_statement.Succeeded())
    return MaybeReplaceLowerPriorityEventLevelReportResult::kError;

  // Deactivate the source as a new report will never be generated in the
  // future.
  if (!has_matching_report) {
    static constexpr char kDeactivateSql[] =
        "UPDATE impressions SET active = 0 WHERE impression_id = ?";
    sql::Statement deactivate_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kDeactivateSql));
    deactivate_statement.BindInt64(0, *source.source_id());
    return deactivate_statement.Run()
               ? MaybeReplaceLowerPriorityEventLevelReportResult::
                     kDropNewReportSourceDeactivated
               : MaybeReplaceLowerPriorityEventLevelReportResult::kError;
  }

  int64_t min_priority = min_priority_statement.ColumnInt64(0);
  AttributionReport::EventLevelData::Id conversion_id_with_min_priority(
      min_priority_statement.ColumnInt64(1));

  // If the new report's priority is less than all existing ones, or if its
  // priority is equal to the minimum existing one and it is more recent, drop
  // it. We could explicitly check the trigger time here, but it would only
  // be relevant in the case of an ill-behaved clock, in which case the rest of
  // the attribution functionality would probably also break.
  if (conversion_priority <= min_priority) {
    return MaybeReplaceLowerPriorityEventLevelReportResult::kDropNewReport;
  }

  absl::optional<AttributionReport> replaced =
      GetReport(conversion_id_with_min_priority);
  if (!replaced.has_value()) {
    return MaybeReplaceLowerPriorityEventLevelReportResult::kError;
  }

  // Otherwise, delete the existing report with the lowest priority.
  if (!DeleteEventLevelReport(conversion_id_with_min_priority)) {
    return MaybeReplaceLowerPriorityEventLevelReportResult::kError;
  }

  replaced_report = std::move(replaced);
  return MaybeReplaceLowerPriorityEventLevelReportResult::kReplaceOldReport;
}

CreateReportResult AttributionStorageSql::MaybeCreateAndStoreReport(
    const AttributionTrigger& trigger) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We don't bother creating the DB here if it doesn't exist, because it's not
  // possible for there to be a matching source if there's no DB.
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return CreateReportResult(
        AttributionTrigger::Result::kNoMatchingImpressions);
  }

  const net::SchemefulSite& conversion_destination =
      trigger.conversion_destination();
  const std::string serialized_conversion_destination =
      conversion_destination.Serialize();

  const url::Origin& reporting_origin = trigger.reporting_origin();
  DCHECK(!conversion_destination.opaque());
  DCHECK(!reporting_origin.opaque());

  base::Time current_time = base::Time::Now();

  // Get all sources that match this <reporting_origin,
  // conversion_destination> pair. Only get sources that are active and not
  // past their expiry time. The sources are fetched in order so that the
  // first one is the one that will be attributed; the others will be deleted.
  static constexpr char kGetMatchingSourcesSql[] =
      "SELECT impression_id FROM impressions "
      DCHECK_SQL_INDEXED_BY("conversion_destination_idx")
      "WHERE conversion_destination = ? AND reporting_origin = ? "
      "AND active = 1 AND expiry_time > ? "
      "ORDER BY priority DESC,impression_time DESC";

  sql::Statement matching_sources_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetMatchingSourcesSql));
  matching_sources_statement.BindString(0, serialized_conversion_destination);
  matching_sources_statement.BindString(1, SerializeOrigin(reporting_origin));
  matching_sources_statement.BindTime(2, current_time);

  // If there are no matching sources, return early.
  if (!matching_sources_statement.Step()) {
    return CreateReportResult(
        matching_sources_statement.Succeeded()
            ? AttributionTrigger::Result::kNoMatchingImpressions
            : AttributionTrigger::Result::kInternalError);
  }

  // The first one returned will be attributed; it has the highest priority.
  StoredSource::Id source_id_to_attribute(
      matching_sources_statement.ColumnInt64(0));

  // Any others will be deleted.
  std::vector<StoredSource::Id> source_ids_to_delete;
  while (matching_sources_statement.Step()) {
    StoredSource::Id source_id(matching_sources_statement.ColumnInt64(0));
    source_ids_to_delete.push_back(source_id);
  }
  // Exit early if the last statement wasn't valid.
  if (!matching_sources_statement.Succeeded()) {
    return CreateReportResult(AttributionTrigger::Result::kInternalError);
  }

  absl::optional<SourceToAttribute> source_to_attribute = ReadSourceToAttribute(
      db_.get(), source_id_to_attribute, reporting_origin);
  // This is only possible if there is a corrupt DB.
  if (!source_to_attribute.has_value()) {
    return CreateReportResult(AttributionTrigger::Result::kInternalError);
  }

  const uint64_t trigger_data =
      source_to_attribute->source.common_info().source_type() ==
              CommonSourceInfo::SourceType::kEvent
          ? trigger.event_source_trigger_data()
          : trigger.trigger_data();

  const base::Time report_time =
      delegate_->GetReportTime(source_to_attribute->source.common_info(),
                               /*trigger_time=*/current_time);
  AttributionReport report(
      AttributionInfo(std::move(source_to_attribute->source),
                      /*time=*/current_time, trigger.debug_key()),
      /*report_time=*/report_time,
      /*external_report_id=*/delegate_->NewReportID(),
      AttributionReport::EventLevelData(trigger_data, trigger.priority(),
                                        /*id=*/absl::nullopt));

  switch (ReportAlreadyStored(source_id_to_attribute, trigger.dedup_key())) {
    case ReportAlreadyStoredStatus::kNotStored:
      break;
    case ReportAlreadyStoredStatus::kStored:
      return CreateReportResult(AttributionTrigger::Result::kDeduplicated,
                                std::move(report));
    case ReportAlreadyStoredStatus::kError:
      return CreateReportResult(AttributionTrigger::Result::kInternalError,
                                std::move(report));
  }

  switch (CapacityForStoringReport(serialized_conversion_destination)) {
    case ConversionCapacityStatus::kHasCapacity:
      break;
    case ConversionCapacityStatus::kNoCapacity:
      return CreateReportResult(
          AttributionTrigger::Result::kNoCapacityForConversionDestination,
          std::move(report));
    case ConversionCapacityStatus::kError:
      return CreateReportResult(AttributionTrigger::Result::kInternalError,
                                std::move(report));
  }

  const AttributionInfo& attribution_info = report.attribution_info();

  switch (rate_limit_table_.AttributionAllowedForAttributionLimit(
      db_.get(), attribution_info)) {
    case RateLimitTable::Result::kAllowed:
      break;
    case RateLimitTable::Result::kNotAllowed:
      return CreateReportResult(
          AttributionTrigger::Result::kExcessiveAttributions,
          std::move(report));
    case RateLimitTable::Result::kError:
      return CreateReportResult(AttributionTrigger::Result::kInternalError,
                                std::move(report));
  }

  switch (rate_limit_table_.AttributionAllowedForReportingOriginLimit(
      db_.get(), attribution_info)) {
    case RateLimitTable::Result::kAllowed:
      break;
    case RateLimitTable::Result::kNotAllowed:
      return CreateReportResult(
          AttributionTrigger::Result::kExcessiveReportingOrigins,
          std::move(report));
    case RateLimitTable::Result::kError:
      return CreateReportResult(AttributionTrigger::Result::kInternalError,
                                std::move(report));
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return CreateReportResult(AttributionTrigger::Result::kInternalError,
                              std::move(report));
  }

  absl::optional<AttributionReport> replaced_report;
  const auto maybe_replace_lower_priority_report_result =
      MaybeReplaceLowerPriorityEventLevelReport(
          report, source_to_attribute->num_conversions, trigger.priority(),
          replaced_report);
  if (maybe_replace_lower_priority_report_result ==
      MaybeReplaceLowerPriorityEventLevelReportResult::kError) {
    return CreateReportResult(AttributionTrigger::Result::kInternalError,
                              std::move(report));
  }

  if (maybe_replace_lower_priority_report_result ==
          MaybeReplaceLowerPriorityEventLevelReportResult::kDropNewReport ||
      maybe_replace_lower_priority_report_result ==
          MaybeReplaceLowerPriorityEventLevelReportResult::
              kDropNewReportSourceDeactivated) {
    if (!transaction.Commit()) {
      return CreateReportResult(AttributionTrigger::Result::kInternalError,
                                std::move(report));
    }
    return CreateReportResult(
        AttributionTrigger::Result::kPriorityTooLow, std::move(report),
        maybe_replace_lower_priority_report_result ==
                MaybeReplaceLowerPriorityEventLevelReportResult::
                    kDropNewReportSourceDeactivated
            ? absl::make_optional(
                  DeactivatedSource::Reason::kReachedAttributionLimit)
            : absl::nullopt);
  }

  // Reports with `AttributionLogic::kNever` should be included in all
  // attribution operations and matching, but only `kTruthfully` should generate
  // reports that get sent.
  const bool create_report = attribution_info.source.attribution_logic() ==
                             StoredSource::AttributionLogic::kTruthfully;

  if (create_report) {
    if (!StoreReport(attribution_info.source.source_id(), trigger_data,
                     attribution_info.time, report.report_time(),
                     trigger.priority(), report.external_report_id(),
                     trigger.debug_key())) {
      return CreateReportResult(AttributionTrigger::Result::kInternalError,
                                std::move(report));
    }
  }

  // If a dedup key is present, store it. We do this regardless of whether
  // `create_report` is true to avoid leaking whether the report was actually
  // stored.
  if (trigger.dedup_key().has_value()) {
    static constexpr char kInsertDedupKeySql[] =
        "INSERT INTO dedup_keys(impression_id,dedup_key)VALUES(?,?)";
    sql::Statement insert_dedup_key_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kInsertDedupKeySql));
    insert_dedup_key_statement.BindInt64(0,
                                         *attribution_info.source.source_id());
    insert_dedup_key_statement.BindInt64(1,
                                         SerializeUint64(*trigger.dedup_key()));
    if (!insert_dedup_key_statement.Run()) {
      return CreateReportResult(AttributionTrigger::Result::kInternalError,
                                std::move(report));
    }
  }

  // Only increment the number of conversions associated with the source if
  // we are adding a new one, rather than replacing a dropped one.
  if (maybe_replace_lower_priority_report_result ==
      MaybeReplaceLowerPriorityEventLevelReportResult::kAddNewReport) {
    static constexpr char kUpdateImpressionForConversionSql[] =
        "UPDATE impressions SET num_conversions = num_conversions + 1 "
        "WHERE impression_id = ?";
    sql::Statement impression_update_statement(db_->GetCachedStatement(
        SQL_FROM_HERE, kUpdateImpressionForConversionSql));

    // Update the attributed source.
    impression_update_statement.BindInt64(0,
                                          *attribution_info.source.source_id());
    if (!impression_update_statement.Run()) {
      return CreateReportResult(AttributionTrigger::Result::kInternalError,
                                std::move(report));
    }
  }

  // Delete all unattributed sources.
  if (!DeleteSources(source_ids_to_delete)) {
    return CreateReportResult(AttributionTrigger::Result::kInternalError,
                              std::move(report));
  }

  // Based on the deletion logic here and the fact that we delete sources
  // with |num_conversions > 1| when there is a new matching source in
  // |StoreSource()|, we should be guaranteed that these sources all
  // have |num_conversions == 0|, and that they never contributed to a rate
  // limit. Therefore, we don't need to call
  // |RateLimitTable::ClearDataForSourceIds()| here.

  if (create_report && !rate_limit_table_.AddRateLimitForAttribution(
                           db_.get(), attribution_info)) {
    return CreateReportResult(AttributionTrigger::Result::kInternalError,
                              std::move(report));
  }

  if (!transaction.Commit()) {
    return CreateReportResult(AttributionTrigger::Result::kInternalError,
                              std::move(report));
  }

  if (!create_report) {
    return CreateReportResult(AttributionTrigger::Result::kDroppedForNoise,
                              std::move(report));
  }

  return CreateReportResult(
      maybe_replace_lower_priority_report_result ==
              MaybeReplaceLowerPriorityEventLevelReportResult::kReplaceOldReport
          ? AttributionTrigger::Result::kSuccessDroppedLowerPriority
          : AttributionTrigger::Result::kSuccess,
      std::move(replaced_report),
      /*dropped_report_source_deactivation_reason=*/absl::nullopt, report_time);
}

bool AttributionStorageSql::StoreReport(
    StoredSource::Id source_id,
    uint64_t trigger_data,
    base::Time trigger_time,
    base::Time report_time,
    int64_t priority,
    const base::GUID& external_report_id,
    absl::optional<uint64_t> trigger_debug_key) {
  DCHECK(external_report_id.is_valid());

  static constexpr char kStoreReportSql[] =
      "INSERT INTO conversions"
      "(impression_id,conversion_data,conversion_time,report_time,"
      "priority,failed_send_attempts,external_report_id,debug_key)"
      "VALUES(?,?,?,?,?,0,?,?)";
  sql::Statement store_report_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kStoreReportSql));
  store_report_statement.BindInt64(0, *source_id);
  store_report_statement.BindInt64(1, SerializeUint64(trigger_data));
  store_report_statement.BindTime(2, trigger_time);
  store_report_statement.BindTime(3, report_time);
  store_report_statement.BindInt64(4, priority);
  store_report_statement.BindString(5, external_report_id.AsLowercaseString());
  BindUint64OrNull(store_report_statement, 6, trigger_debug_key);
  return store_report_statement.Run();
}

namespace {

// Helper to deserialize report rows. See `GetReport()` for the expected
// ordering of columns used for the input to this function.
absl::optional<AttributionReport> ReadReportFromStatement(
    sql::Statement& statement) {
  DCHECK_EQ(statement.ColumnCount(), 19);

  uint64_t trigger_data = DeserializeUint64(statement.ColumnInt64(0));
  base::Time trigger_time = statement.ColumnTime(1);
  base::Time report_time = statement.ColumnTime(2);
  AttributionReport::EventLevelData::Id report_id(statement.ColumnInt64(3));
  int64_t conversion_priority = statement.ColumnInt64(4);
  int failed_send_attempts = statement.ColumnInt(5);
  base::GUID external_report_id =
      base::GUID::ParseLowercase(statement.ColumnString(6));
  url::Origin impression_origin = DeserializeOrigin(statement.ColumnString(7));
  url::Origin conversion_origin = DeserializeOrigin(statement.ColumnString(8));
  url::Origin reporting_origin = DeserializeOrigin(statement.ColumnString(9));
  uint64_t source_event_id = DeserializeUint64(statement.ColumnInt64(10));
  base::Time impression_time = statement.ColumnTime(11);
  base::Time expiry_time = statement.ColumnTime(12);
  StoredSource::Id source_id(statement.ColumnInt64(13));
  absl::optional<CommonSourceInfo::SourceType> source_type =
      DeserializeSourceType(statement.ColumnInt(14));
  int64_t attribution_source_priority = statement.ColumnInt64(15);
  absl::optional<StoredSource::AttributionLogic> attribution_logic =
      DeserializeAttributionLogic(statement.ColumnInt(16));
  absl::optional<uint64_t> source_debug_key = ColumnUint64OrNull(statement, 17);
  absl::optional<uint64_t> trigger_debug_key =
      ColumnUint64OrNull(statement, 18);

  // Ensure origins are valid before continuing. This could happen if there is
  // database corruption.
  // TODO(csharrison): This should be an extremely rare occurrence but it
  // would entail that some records will remain in the DB as vestigial if a
  // report is never sent. We should delete these entries from the DB.
  // TODO(apaseltiner): Should we raze the DB if we've detected corruption?
  if (impression_origin.opaque() || conversion_origin.opaque() ||
      reporting_origin.opaque() || !source_type.has_value() ||
      !attribution_logic.has_value() || failed_send_attempts < 0 ||
      !external_report_id.is_valid()) {
    return absl::nullopt;
  }

  // Create the source and AttributionReport objects from the retrieved
  // columns.
  StoredSource source(
      CommonSourceInfo(source_event_id, std::move(impression_origin),
                       std::move(conversion_origin),
                       std::move(reporting_origin), impression_time,
                       expiry_time, *source_type, attribution_source_priority,
                       source_debug_key),
      *attribution_logic, source_id);

  AttributionReport report(
      AttributionInfo(std::move(source), trigger_time, trigger_debug_key),
      report_time, std::move(external_report_id),
      AttributionReport::EventLevelData(trigger_data, conversion_priority,
                                        report_id));
  report.set_failed_send_attempts(failed_send_attempts);
  return report;
}

}  // namespace

// TODO(linnan): Move `GetAggregatableContributionReportsForTesting()` into this
// function.
std::vector<AttributionReport> AttributionStorageSql::GetAttributionsToReport(
    base::Time max_report_time,
    int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return {};

  // Get at most |limit| entries in the conversions table with a |report_time|
  // no greater than |max_report_time| and their matching information from the
  // impression table. Negatives are treated as no limit
  // (https://sqlite.org/lang_select.html#limitoffset).
  static constexpr char kGetReportsSql[] =
      "SELECT C.conversion_data,C.conversion_time,C.report_time,"
      "C.conversion_id,C.priority,C.failed_send_attempts,C.external_report_id,"
      "I.impression_origin,I.conversion_origin,I.reporting_origin,"
      "I.impression_data,I.impression_time,I.expiry_time,I.impression_id,"
      "I.source_type,I.priority,I.attributed_truthfully,I.debug_key,"
      "C.debug_key "
      "FROM conversions C JOIN impressions I ON "
      "C.impression_id = I.impression_id WHERE C.report_time <= ? "
      "LIMIT ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetReportsSql));
  statement.BindTime(0, max_report_time);
  statement.BindInt(1, limit);

  std::vector<AttributionReport> reports;
  while (statement.Step()) {
    absl::optional<AttributionReport> report =
        ReadReportFromStatement(statement);
    if (report.has_value())
      reports.push_back(std::move(*report));
  }

  if (!statement.Succeeded())
    return {};

  delegate_->ShuffleReports(reports);
  return reports;
}

absl::optional<base::Time> AttributionStorageSql::GetNextReportTime(
    base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return absl::nullopt;

  static constexpr char kNextReportTimeSql[] =
      "SELECT MIN(report_time) FROM conversions "
      "WHERE report_time > ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kNextReportTimeSql));
  statement.BindTime(0, time);

  if (statement.Step() &&
      statement.GetColumnType(0) != sql::ColumnType::kNull) {
    return statement.ColumnTime(0);
  }

  return absl::nullopt;
}

std::vector<AttributionReport> AttributionStorageSql::GetReports(
    const std::vector<AttributionReport::EventLevelData::Id>& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return {};

  std::vector<AttributionReport> reports;
  for (AttributionReport::EventLevelData::Id id : ids) {
    absl::optional<AttributionReport> report = GetReport(id);
    if (report.has_value())
      reports.push_back(std::move(*report));
  }
  return reports;
}

absl::optional<AttributionReport> AttributionStorageSql::GetReport(
    AttributionReport::EventLevelData::Id conversion_id) {
  static constexpr char kGetReportSql[] =
      "SELECT C.conversion_data,C.conversion_time,C.report_time,"
      "C.conversion_id,C.priority,C.failed_send_attempts,C.external_report_id,"
      "I.impression_origin,I.conversion_origin,I.reporting_origin,"
      "I.impression_data,I.impression_time,I.expiry_time,I.impression_id,"
      "I.source_type,I.priority,I.attributed_truthfully,I.debug_key,"
      "C.debug_key "
      "FROM conversions C JOIN impressions I ON "
      "C.impression_id = I.impression_id WHERE C.conversion_id = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetReportSql));
  statement.BindInt64(0, *conversion_id);

  if (!statement.Step())
    return absl::nullopt;

  return ReadReportFromStatement(statement);
}

bool AttributionStorageSql::DeleteExpiredSources() {
  const int kMaxDeletesPerBatch = 100;

  auto delete_sources_from_paged_select =
      [this](sql::Statement& statement)
          VALID_CONTEXT_REQUIRED(sequence_checker_) -> bool {
    DCHECK_EQ(statement.ColumnCount(), 1);

    while (true) {
      std::vector<StoredSource::Id> source_ids;
      while (statement.Step()) {
        StoredSource::Id source_id(statement.ColumnInt64(0));
        source_ids.push_back(source_id);
      }
      if (!statement.Succeeded())
        return false;
      if (source_ids.empty())
        return true;
      if (!DeleteSources(source_ids))
        return false;
      // Deliberately retain the existing bound vars so that the limit, etc are
      // the same.
      statement.Reset(/*clear_bound_vars=*/false);
    }
  };

  // Delete all sources that have no associated reports and are past
  // their expiry time. Optimized by |kImpressionExpiryIndexSql|.
  static constexpr char kSelectExpiredSourcesSql[] =
      "SELECT impression_id FROM impressions "
      DCHECK_SQL_INDEXED_BY("impression_expiry_idx")
      "WHERE expiry_time <= ? AND "
      "impression_id NOT IN("
      "SELECT impression_id FROM conversions"
      DCHECK_SQL_INDEXED_BY("conversion_impression_id_idx")
      ")AND impression_id NOT IN("
      "SELECT source_id FROM aggregatable_report_metadata"
      DCHECK_SQL_INDEXED_BY("aggregate_source_id_idx")
      ")LIMIT ?";
  sql::Statement select_expired_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectExpiredSourcesSql));
  select_expired_statement.BindTime(0, base::Time::Now());
  select_expired_statement.BindInt(1, kMaxDeletesPerBatch);
  if (!delete_sources_from_paged_select(select_expired_statement))
    return false;

  // Delete all sources that have no associated reports and are
  // inactive. This is done in a separate statement from
  // |kSelectExpiredSourcesSql| so that each query is optimized by an index.
  // Optimized by |kConversionDestinationIndexSql|.
  static constexpr char kSelectInactiveSourcesSql[] =
      "SELECT impression_id FROM impressions "
      DCHECK_SQL_INDEXED_BY("conversion_destination_idx")
      "WHERE active = 0 AND "
      "impression_id NOT IN("
      "SELECT impression_id FROM conversions"
      DCHECK_SQL_INDEXED_BY("conversion_impression_id_idx")
      ")AND impression_id NOT IN("
      "SELECT source_id FROM aggregatable_report_metadata"
      DCHECK_SQL_INDEXED_BY("aggregate_source_id_idx")
      ")LIMIT ?";
  sql::Statement select_inactive_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectInactiveSourcesSql));
  select_inactive_statement.BindInt(0, kMaxDeletesPerBatch);
  return delete_sources_from_paged_select(select_inactive_statement);
}

bool AttributionStorageSql::DeleteReport(AttributionReport::Id report_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return true;

  struct Visitor {
    raw_ptr<AttributionStorageSql> storage;

    bool operator()(AttributionReport::EventLevelData::Id id) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(storage->sequence_checker_);
      return storage->DeleteEventLevelReport(id);
    }

    bool operator()(AttributionReport::AggregatableContributionData::Id id) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(storage->sequence_checker_);
      return storage->DeleteAggregatableContributionReport(id);
    }
  };

  return absl::visit(Visitor{.storage = this}, report_id);
}

bool AttributionStorageSql::DeleteEventLevelReport(
    AttributionReport::EventLevelData::Id report_id) {
  static constexpr char kDeleteReportSql[] =
      "DELETE FROM conversions WHERE conversion_id = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteReportSql));
  statement.BindInt64(0, *report_id);
  return statement.Run();
}

bool AttributionStorageSql::UpdateReportForSendFailure(
    AttributionReport::EventLevelData::Id conversion_id,
    base::Time new_report_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return false;

  static constexpr char kUpdateFailedReportSql[] =
      "UPDATE conversions SET report_time = ?,"
      "failed_send_attempts = failed_send_attempts + 1 "
      "WHERE conversion_id = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kUpdateFailedReportSql));
  statement.BindTime(0, new_report_time);
  statement.BindInt64(1, *conversion_id);
  return statement.Run() && db_->GetLastChangeCount() == 1;
}

absl::optional<base::Time> AttributionStorageSql::AdjustOfflineReportTimes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto delay = delegate_->GetOfflineReportDelayConfig();
  if (!delay.has_value())
    return absl::nullopt;

  DCHECK_GE(delay->min, base::TimeDelta());
  DCHECK_GE(delay->max, base::TimeDelta());
  DCHECK_LE(delay->min, delay->max);

  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return absl::nullopt;

  base::Time now = base::Time::Now();

  // Set the report time for all reports that should have been sent before now
  // to now + a random number of microseconds between `min` and `max`, both
  // inclusive. We use RANDOM, instead of a method on the delegate, to avoid
  // having to pull all reports into memory and update them one by one. We use
  // ABS because RANDOM may return a negative integer. We add 1 to the
  // difference between `max` and `min` to ensure that the range of generated
  // values is inclusive. If `max == min`, we take the remainder modulo 1, which
  // is always 0.
  static constexpr char kSetReportTimeSql[] =
      "UPDATE conversions "
      "SET report_time=?+ABS(RANDOM()%?)"
      "WHERE report_time<?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSetReportTimeSql));
  statement.BindTime(0, now + delay->min);
  statement.BindInt64(1, 1 + (delay->max - delay->min).InMicroseconds());
  statement.BindTime(2, now);
  if (!statement.Run())
    return absl::nullopt;

  return GetNextReportTime(base::Time::Min());
}

void AttributionStorageSql::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const url::Origin&)> filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return;

  SCOPED_UMA_HISTOGRAM_TIMER("Conversions.ClearDataTime");
  if (filter.is_null() && (delete_begin.is_null() || delete_begin.is_min()) &&
      delete_end.is_max()) {
    ClearAllDataAllTime();
    return;
  }

  // Measure the time it takes to perform a clear with a filter separately from
  // the above histogram.
  SCOPED_UMA_HISTOGRAM_TIMER("Conversions.Storage.ClearDataWithFilterDuration");

  // TODO(csharrison, johnidel): This query can be split up and optimized by
  // adding indexes on the impression_time and conversion_time columns.
  // See this comment for more information:
  // crrev.com/c/2150071/4/content/browser/conversions/conversion_storage_sql.cc#342
  //
  // TODO(crbug.com/1290377): Look into optimizing origin filter callback.
  static constexpr char kScanCandidateData[] =
      "SELECT I.impression_origin,I.conversion_origin,I.reporting_origin,"
      "I.impression_id,C.conversion_id "
      "FROM impressions I LEFT JOIN conversions C ON "
      "C.impression_id = I.impression_id WHERE"
      "(I.impression_time BETWEEN ?1 AND ?2)OR"
      "(C.conversion_time BETWEEN ?1 AND ?2)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kScanCandidateData));
  statement.BindTime(0, delete_begin);
  statement.BindTime(1, delete_end);

  std::vector<StoredSource::Id> source_ids_to_delete;
  int num_reports_deleted = 0;
  while (statement.Step()) {
    if (filter.is_null() ||
        filter.Run(DeserializeOrigin(statement.ColumnString(0))) ||
        filter.Run(DeserializeOrigin(statement.ColumnString(1))) ||
        filter.Run(DeserializeOrigin(statement.ColumnString(2)))) {
      source_ids_to_delete.emplace_back(statement.ColumnInt64(3));
      if (statement.GetColumnType(4) != sql::ColumnType::kNull) {
        if (!DeleteEventLevelReport(AttributionReport::EventLevelData::Id(
                statement.ColumnInt64(4)))) {
          return;
        }

        ++num_reports_deleted;
      }
    }
  }

  // TODO(csharrison, johnidel): Should we consider poisoning the DB if some of
  // the delete operations fail?
  if (!statement.Succeeded())
    return;

  // Delete the data in a transaction to avoid cases where the source part
  // of a report is deleted without deleting the associated report, or
  // vice versa.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  if (!ClearAggregatableAttributionForOriginsInRange(
          delete_begin, delete_end, filter, source_ids_to_delete)) {
    return;
  }

  // Since multiple reports can be associated with a single source,
  // deduplicate source IDs using a set to avoid redundant DB operations
  // below.
  source_ids_to_delete =
      base::flat_set<StoredSource::Id>(std::move(source_ids_to_delete))
          .extract();

  if (!DeleteSources(source_ids_to_delete))
    return;

  // Careful! At this point we can still have some vestigial entries in the DB.
  // For example, if a source has two reports, and one report is
  // deleted, the above logic will delete the source as well, leaving the
  // second report in limbo (it was not in the deletion time range).
  // Delete all unattributed reports here to ensure everything is cleaned
  // up.
  static constexpr char kDeleteVestigialConversionSql[] =
      "DELETE FROM conversions WHERE impression_id = ?";
  sql::Statement delete_vestigial_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteVestigialConversionSql));
  for (StoredSource::Id source_id : source_ids_to_delete) {
    delete_vestigial_statement.Reset(/*clear_bound_vars=*/true);
    delete_vestigial_statement.BindInt64(0, *source_id);
    if (!delete_vestigial_statement.Run())
      return;

    num_reports_deleted += db_->GetLastChangeCount();
  }

  // Careful! At this point we can still have some vestigial entries in the DB.
  // See comments above for event-level reports.
  if (!ClearAggregatableAttributionForSourceIds(source_ids_to_delete))
    return;

  if (!rate_limit_table_.ClearDataForSourceIds(db_.get(),
                                               source_ids_to_delete)) {
    return;
  }

  if (!rate_limit_table_.ClearDataForOriginsInRange(db_.get(), delete_begin,
                                                    delete_end, filter)) {
    return;
  }

  if (!transaction.Commit())
    return;

  RecordSourcesDeleted(static_cast<int>(source_ids_to_delete.size()));
  RecordReportsDeleted(num_reports_deleted);
}

void AttributionStorageSql::ClearAllDataAllTime() {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  static constexpr char kDeleteAllReportsSql[] = "DELETE FROM conversions";
  sql::Statement delete_all_reports_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllReportsSql));
  if (!delete_all_reports_statement.Run())
    return;
  int num_reports_deleted = db_->GetLastChangeCount();

  static constexpr char kDeleteAllSourcesSql[] = "DELETE FROM impressions";
  sql::Statement delete_all_sources_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllSourcesSql));
  if (!delete_all_sources_statement.Run())
    return;
  int num_sources_deleted = db_->GetLastChangeCount();

  static constexpr char kDeleteAllDedupKeysSql[] = "DELETE FROM dedup_keys";
  sql::Statement delete_all_dedup_keys_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllDedupKeysSql));
  if (!delete_all_dedup_keys_statement.Run())
    return;

  static constexpr char kDeleteAllAggregationsSql[] =
      "DELETE FROM aggregatable_report_metadata";
  sql::Statement delete_all_aggregations_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllAggregationsSql));
  if (!delete_all_aggregations_statement.Run())
    return;

  static constexpr char kDeleteAllContributionsSql[] =
      "DELETE FROM aggregatable_contributions";
  sql::Statement delete_all_contributions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllContributionsSql));
  if (!delete_all_contributions_statement.Run())
    return;

  if (!rate_limit_table_.ClearAllDataAllTime(db_.get()))
    return;

  if (!transaction.Commit())
    return;

  RecordSourcesDeleted(num_sources_deleted);
  RecordReportsDeleted(num_reports_deleted);
}

bool AttributionStorageSql::HasCapacityForStoringSource(
    const std::string& serialized_origin) {
  static constexpr char kCountSourcesSql[] =
      // clang-format off
      "SELECT COUNT(impression_origin)FROM impressions "
      DCHECK_SQL_INDEXED_BY("impression_origin_idx")
      "WHERE impression_origin = ?";  // clang-format on

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kCountSourcesSql));
  statement.BindString(0, serialized_origin);
  if (!statement.Step())
    return false;
  int64_t count = statement.ColumnInt64(0);
  return count < delegate_->GetMaxSourcesPerOrigin();
}

AttributionStorageSql::ReportAlreadyStoredStatus
AttributionStorageSql::ReportAlreadyStored(StoredSource::Id source_id,
                                           absl::optional<uint64_t> dedup_key) {
  if (!dedup_key.has_value())
    return ReportAlreadyStoredStatus::kNotStored;

  static constexpr char kCountReportsSql[] =
      "SELECT COUNT(*)FROM dedup_keys "
      "WHERE impression_id = ? AND dedup_key = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kCountReportsSql));
  statement.BindInt64(0, *source_id);
  statement.BindInt64(1, SerializeUint64(*dedup_key));

  // If there's an error, return true so `MaybeCreateAndStoreReport()`
  // returns early.
  if (!statement.Step())
    return ReportAlreadyStoredStatus::kError;

  int64_t count = statement.ColumnInt64(0);
  return count > 0 ? ReportAlreadyStoredStatus::kStored
                   : ReportAlreadyStoredStatus::kNotStored;
}

AttributionStorageSql::ConversionCapacityStatus
AttributionStorageSql::CapacityForStoringReport(
    const std::string& serialized_origin) {
  // This query should be reasonably optimized via
  // `kConversionDestinationIndexSql`. The conversion origin is the second
  // column in a multi-column index where the first column is just a boolean.
  // Therefore the second column in the index should be very well-sorted.
  //
  // Note: to take advantage of this, we need to hint to the query planner that
  // |active| is a boolean, so include it in the conditional.
  static constexpr char kCountReportsSql[] =
      "SELECT COUNT(conversion_id)FROM conversions C "
      "JOIN impressions I "
      DCHECK_SQL_INDEXED_BY("conversion_destination_idx")
      "ON I.impression_id = C.impression_id "
      "WHERE I.conversion_destination = ? AND(active BETWEEN 0 AND 1)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kCountReportsSql));
  statement.BindString(0, serialized_origin);
  if (!statement.Step())
    return ConversionCapacityStatus::kError;
  int64_t count = statement.ColumnInt64(0);
  return count < delegate_->GetMaxAttributionsPerOrigin()
             ? ConversionCapacityStatus::kHasCapacity
             : ConversionCapacityStatus::kNoCapacity;
}

std::vector<StoredSource> AttributionStorageSql::GetActiveSources(int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return {};

  // Negatives are treated as no limit
  // (https://sqlite.org/lang_select.html#limitoffset).
  static constexpr char kGetActiveSourcesSql[] =
      "SELECT impression_id,impression_data,impression_origin,"
      "conversion_origin,reporting_origin,impression_time,expiry_time,"
      "source_type,attributed_truthfully,priority,debug_key "
      "FROM impressions "
      "WHERE active = 1 and expiry_time > ? "
      "LIMIT ?";

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetActiveSourcesSql));
  statement.BindTime(0, base::Time::Now());
  statement.BindInt(1, limit);

  std::vector<StoredSource> sources;
  while (statement.Step()) {
    absl::optional<StoredSource> source = ReadSourceFromStatement(statement);
    if (source.has_value())
      sources.push_back(std::move(*source));
  }
  if (!statement.Succeeded())
    return {};

  for (auto& source : sources) {
    absl::optional<std::vector<uint64_t>> dedup_keys =
        ReadDedupKeys(source.source_id());
    if (!dedup_keys.has_value())
      return {};
    source.SetDedupKeys(std::move(*dedup_keys));
  }

  return sources;
}

absl::optional<std::vector<uint64_t>> AttributionStorageSql::ReadDedupKeys(
    StoredSource::Id source_id) {
  static constexpr char kDedupKeySql[] =
      "SELECT dedup_key FROM dedup_keys WHERE impression_id = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDedupKeySql));
  statement.BindInt64(0, *source_id);

  std::vector<uint64_t> dedup_keys;
  while (statement.Step()) {
    dedup_keys.push_back(DeserializeUint64(statement.ColumnInt64(0)));
  }
  if (!statement.Succeeded())
    return absl ::nullopt;

  return dedup_keys;
}

void AttributionStorageSql::HandleInitializationFailure(
    const InitStatus status) {
  RecordInitializationStatus(status);
  db_.reset();
  db_init_status_ = DbStatus::kClosed;
}

bool AttributionStorageSql::LazyInit(DbCreationPolicy creation_policy) {
  if (!db_init_status_) {
    if (g_run_in_memory_) {
      db_init_status_ = DbStatus::kDeferringCreation;
    } else {
      db_init_status_ = base::PathExists(path_to_database_)
                            ? DbStatus::kDeferringOpen
                            : DbStatus::kDeferringCreation;
    }
  }

  switch (*db_init_status_) {
    // If the database file has not been created, we defer creation until
    // storage needs to be used for an operation which needs to operate even on
    // an empty database.
    case DbStatus::kDeferringCreation:
      if (creation_policy == DbCreationPolicy::kIgnoreIfAbsent)
        return false;
      break;
    case DbStatus::kDeferringOpen:
      break;
    case DbStatus::kClosed:
      return false;
    case DbStatus::kOpen:
      return true;
  }

  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions{
      .exclusive_locking = true, .page_size = 4096, .cache_size = 32});
  db_->set_histogram_tag("Conversions");

  // Supply this callback with a weak_ptr to avoid calling the error callback
  // after |this| has been deleted.
  db_->set_error_callback(
      base::BindRepeating(&AttributionStorageSql::DatabaseErrorCallback,
                          weak_factory_.GetWeakPtr()));

  if (path_to_database_.value() == kInMemoryPath) {
    if (!db_->OpenInMemory()) {
      HandleInitializationFailure(InitStatus::kFailedToOpenDbInMemory);
      return false;
    }
  } else {
    const base::FilePath& dir = path_to_database_.DirName();
    const bool dir_exists_or_was_created =
        base::DirectoryExists(dir) || base::CreateDirectory(dir);
    if (dir_exists_or_was_created == false) {
      DLOG(ERROR) << "Failed to create directory for Conversion database";
      HandleInitializationFailure(InitStatus::kFailedToCreateDir);
      return false;
    }
    if (db_->Open(path_to_database_) == false) {
      HandleInitializationFailure(InitStatus::kFailedToOpenDbFile);
      return false;
    }
  }

  if (!InitializeSchema(db_init_status_ == DbStatus::kDeferringCreation)) {
    HandleInitializationFailure(InitStatus::kFailedToInitializeSchema);
    return false;
  }

  db_init_status_ = DbStatus::kOpen;
  RecordInitializationStatus(InitStatus::kSuccess);
  return true;
}

bool AttributionStorageSql::InitializeSchema(bool db_empty) {
  if (db_empty)
    return CreateSchema();

  // Create the meta table if it doesn't already exist. The only version for
  // which this is the case is version 1.
  if (!meta_table_.Init(db_.get(), /*version=*/1, kCompatibleVersionNumber))
    return false;

  int version = meta_table_.GetVersionNumber();
  if (version == kCurrentVersionNumber)
    return true;

  // Recreate the DB if the version is deprecated or too new. In the latter
  // case, the DB will never work until Chrome is re-upgraded. Assume the user
  // will continue using this Chrome version and raze the DB to get attribution
  // reporting working.
  if (version <= kDeprecatedVersionNumber ||
      meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    // Note that this also razes the meta table, so it will need to be
    // initialized again.
    db_->Raze();
    meta_table_.Reset();
    return CreateSchema();
  }

  return UpgradeAttributionStorageSqlSchema(db_.get(), &meta_table_);
}

bool AttributionStorageSql::CreateSchema() {
  base::ThreadTicks start_timestamp = base::ThreadTicks::Now();

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  // TODO(johnidel, csharrison): Many sources will share a target origin and
  // a reporting origin, so it makes sense to make a "shared string" table for
  // these to save disk / memory. However, this complicates the schema a lot, so
  // probably best to only do it if there's performance problems here.
  //
  // Origins usually aren't _that_ big compared to a 64 bit integer(8 bytes).
  //
  // All of the columns in this table are designed to be "const" except for
  // |num_conversions| and |active| which are updated when a new report is
  // received. |num_conversions| is the number of times a report has
  // been created for a given source. |delegate_| can choose to enforce a
  // maximum limit on this. |active| indicates whether a source is able to
  // create new associated reports. |active| can be unset on a number
  // of conditions:
  //   - A source converted too many times.
  //   - A new source was stored after a source converted, making it
  //     ineligible for new sources due to the attribution model documented
  //     in `StoreSource()`.
  //   - A source has expired but still has unsent reports in the
  //     conversions table meaning it cannot be deleted yet.
  // |source_type| is the type of the source of the source, currently always
  // |kNavigation|.
  // |attributed_truthfully| corresponds to the
  // |StoredSource::AttributionLogic| enum.
  // |impression_site| is used to optimize the lookup of sources;
  // |CommonSourceInfo::ImpressionSite| is always derived from the origin.
  //
  // |impression_id| uses AUTOINCREMENT to ensure that IDs aren't reused over
  // the lifetime of the DB.
  static constexpr char kImpressionTableSql[] =
      "CREATE TABLE IF NOT EXISTS impressions"
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
      "impression_site TEXT NOT NULL,"
      "debug_key INTEGER)";
  if (!db_->Execute(kImpressionTableSql))
    return false;

  // Optimizes source lookup by conversion destination/reporting origin
  // during calls to `MaybeCreateAndStoreReport()`,
  // `StoreSource()`, `DeleteExpiredSources()`. Sources and
  // triggers are considered matching if they share this pair. These calls
  // need to distinguish between active and inactive reports, so include
  // |active| in the index.
  static constexpr char kConversionDestinationIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_destination_idx "
      "ON impressions(active,conversion_destination,reporting_origin)";
  if (!db_->Execute(kConversionDestinationIndexSql))
    return false;

  // Optimizes calls to `DeleteExpiredSources()` and
  // `MaybeCreateAndStoreReport()` by indexing sources by expiry
  // time. Both calls require only returning sources that expire after a
  // given time.
  static constexpr char kImpressionExpiryIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_expiry_idx "
      "ON impressions(expiry_time)";
  if (!db_->Execute(kImpressionExpiryIndexSql))
    return false;

  // Optimizes counting sources by source origin.
  static constexpr char kImpressionOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_origin_idx "
      "ON impressions(impression_origin)";
  if (!db_->Execute(kImpressionOriginIndexSql))
    return false;

  // Optimizes `HasCapacityForUniqueDestinationLimitForPendingSource()`, which
  // only needs to examine active, unconverted sources.
  static constexpr char kImpressionSiteReportingOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS impression_site_reporting_origin_idx "
      "ON impressions(impression_site,reporting_origin)"
      "WHERE active=1 AND num_conversions=0";
  if (!db_->Execute(kImpressionSiteReportingOriginIndexSql))
    return false;

  // All columns in this table are const except |report_time| and
  // |failed_send_attempts|,
  // which are updated when a report fails to send, as part of retries.
  // |impression_id| is the primary key of a row in the [impressions] table,
  // [impressions.impression_id]. |conversion_time| is the time at which the
  // trigger was registered, and should be used for clearing site data.
  // |report_time| is the time a <report, source> pair should be
  // reported, and is specified by |delegate_|.
  //
  // |conversion_id| uses AUTOINCREMENT to ensure that IDs aren't reused over
  // the lifetime of the DB.
  static constexpr char kConversionTableSql[] =
      "CREATE TABLE IF NOT EXISTS conversions"
      "(conversion_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "impression_id INTEGER NOT NULL,"
      "conversion_data INTEGER NOT NULL,"
      "conversion_time INTEGER NOT NULL,"
      "report_time INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "failed_send_attempts INTEGER NOT NULL,"
      "external_report_id TEXT NOT NULL,"
      "debug_key INTEGER)";
  if (!db_->Execute(kConversionTableSql))
    return false;

  // Optimize sorting reports by report time for calls to
  // GetAttributionsToReport(). The reports with the earliest report times are
  // periodically fetched from storage to be sent.
  static constexpr char kConversionReportTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_report_idx "
      "ON conversions(report_time)";
  if (!db_->Execute(kConversionReportTimeIndexSql))
    return false;

  // Want to optimize report look up by source id. This allows us to
  // quickly know if an expired source can be deleted safely if it has no
  // corresponding pending reports during calls to
  // `DeleteExpiredSources()`.
  static constexpr char kConversionImpressionIdIndexSql[] =
      "CREATE INDEX IF NOT EXISTS conversion_impression_id_idx "
      "ON conversions(impression_id)";
  if (!db_->Execute(kConversionImpressionIdIndexSql))
    return false;

  if (!rate_limit_table_.CreateTable(db_.get()))
    return false;

  static constexpr char kDedupKeyTableSql[] =
      "CREATE TABLE IF NOT EXISTS dedup_keys"
      "(impression_id INTEGER NOT NULL,"
      "dedup_key INTEGER NOT NULL,"
      "PRIMARY KEY(impression_id,dedup_key))WITHOUT ROWID";
  if (!db_->Execute(kDedupKeyTableSql))
    return false;

  // ============================
  // AGGREGATE ATTRIBUTION SCHEMA
  // ============================

  // An attribution might make multiple histogram contributions. Therefore
  // multiple rows in |aggregatable_contributions| table might correspond to the
  // same row in |aggregatable_report_metadata| table.

  // All columns in this table are const.
  // `source_id` is the primary key of a row in the [impressions] table,
  // [impressions.impression_id].
  // `trigger_time` is the time at which the trigger was registered, and
  // should be used for clearing site data.
  static constexpr char kAggregatableReportMetadataTableSql[] =
      "CREATE TABLE IF NOT EXISTS aggregatable_report_metadata("
      "aggregation_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "source_id INTEGER NOT NULL,"
      "trigger_time INTEGER NOT NULL)";
  if (!db_->Execute(kAggregatableReportMetadataTableSql))
    return false;

  // Optimizes aggregatable report look up by source id during calls to
  // `DeleteExpiredSources()`, `ClearAggregatableAttributionForSourceIds()`,
  // `GetAggregatableContributionReportsForTesting()`.
  static constexpr char kAggregateSourceIdIndexSql[] =
      "CREATE INDEX IF NOT EXISTS aggregate_source_id_idx "
      "ON aggregatable_report_metadata(source_id)";
  if (!db_->Execute(kAggregateSourceIdIndexSql))
    return false;

  // Optimizes aggregatable report look up by trigger time for clearing site
  // data during calls to `ClearAggregatableAttributionForOriginsInRange()`.
  static constexpr char kAggregateTriggerTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS aggregate_trigger_time_idx "
      "ON aggregatable_report_metadata(trigger_time)";
  if (!db_->Execute(kAggregateTriggerTimeIndexSql))
    return false;

  // All columns in this table are const except `report_time` and
  // `failed_send_attempts`, which are updated when a report fails to send, as
  // part of retries.
  // `aggregation_id` is the primary key of a row in the
  // [aggregatable_report_metadata] table.
  // `report_time` is the time the aggregatable report should be reported.
  // `bucket` is the histogram bucket.
  // `value` is the histogram value.
  // `external_report_id` is used for deduplicating reports received by the
  // reporting origin.
  static constexpr char kAggregatableContributionsTableSql[] =
      "CREATE TABLE IF NOT EXISTS aggregatable_contributions("
      "contribution_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "aggregation_id INTEGER NOT NULL,"
      "report_time INTEGER NOT NULL,"
      "failed_send_attempts INTEGER NOT NULL,"
      "bucket TEXT NOT NULL,"
      "value INTEGER NOT NULL,"
      "external_report_id TEXT NOT NULL)";
  if (!db_->Execute(kAggregatableContributionsTableSql))
    return false;

  // Optimizes contribution look up by aggregation id during calls to
  // `ClearAggregatableContributions()`,
  // `DeleteAggregatableContributionReport()`.
  static constexpr char kContributionAggregationIdIndexSql[] =
      "CREATE INDEX IF NOT EXISTS contribution_aggregation_id_idx "
      "ON aggregatable_contributions(aggregation_id)";
  if (!db_->Execute(kContributionAggregationIdIndexSql))
    return false;

  // Optimizes contribution report look up by report time to get reports in a
  // time range during calls to
  // `GetAggregatableContributionReportsForTesting()`.
  static constexpr char kContributionReportTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS contribution_report_time_idx "
      "ON aggregatable_contributions(report_time)";
  if (!db_->Execute(kContributionReportTimeIndexSql))
    return false;

  if (!meta_table_.Init(db_.get(), kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    return false;
  }

  if (!transaction.Commit())
    return false;

  base::UmaHistogramMediumTimes("Conversions.Storage.CreationTime",
                                base::ThreadTicks::Now() - start_timestamp);
  return true;
}

void AttributionStorageSql::DatabaseErrorCallback(int extended_error,
                                                  sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Attempt to recover a corrupt database, unless it is setup in memory.
  if (sql::Recovery::ShouldRecover(extended_error) &&
      (path_to_database_.value() != kInMemoryPath)) {
    // Prevent reentrant calls.
    db_->reset_error_callback();

    // After this call, the |db_| handle is poisoned so that future calls will
    // return errors until the handle is re-opened.
    sql::Recovery::RecoverDatabaseWithMetaVersion(db_.get(), path_to_database_);

    // The DLOG(FATAL) below is intended to draw immediate attention to errors
    // in newly-written code.  Database corruption is generally a result of OS
    // or hardware issues, not coding errors at the client level, so displaying
    // the error would probably lead to confusion.  The ignored call signals the
    // test-expectation framework that the error was handled.
    std::ignore = sql::Database::IsExpectedSqliteError(extended_error);
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error) &&
      !ignore_errors_for_testing_)
    DLOG(FATAL) << db_->GetErrorMessage();

  // Consider the  database closed if we did not attempt to recover so we did
  // not produce further errors.
  db_init_status_ = DbStatus::kClosed;
}

bool AttributionStorageSql::
    HasCapacityForUniqueDestinationLimitForPendingSource(
        const StorableSource& source) {
  const int max = delegate_->GetMaxDestinationsPerSourceSiteReportingOrigin();
  // TODO(apaseltiner): We could just make
  // `GetMaxDestinationsPerSourceSiteReportingOrigin()` return `size_t`, but it
  // would be inconsistent with the other `AttributionStorageDelegate`
  // methods.
  DCHECK_GT(max, 0);

  const std::string serialized_conversion_destination =
      source.common_info().ConversionDestination().Serialize();

  // Optimized by `kImpressionSiteReportingOriginIndexSql`.
  static constexpr char kSelectSourcesSql[] =
      "SELECT conversion_destination FROM impressions "
      DCHECK_SQL_INDEXED_BY("impression_site_reporting_origin_idx")
      "WHERE impression_site=? AND reporting_origin=? "
      "AND active=1 AND num_conversions=0";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectSourcesSql));
  statement.BindString(0, source.common_info().ImpressionSite().Serialize());
  statement.BindString(
      1, SerializeOrigin(source.common_info().reporting_origin()));

  base::flat_set<std::string> destinations;
  while (statement.Step()) {
    std::string destination = statement.ColumnString(0);

    // The destination isn't new, so it doesn't change the count.
    if (destination == serialized_conversion_destination)
      return true;

    destinations.insert(std::move(destination));

    if (destinations.size() == static_cast<size_t>(max))
      return false;
  }

  return statement.Succeeded();
}

bool AttributionStorageSql::DeleteSources(
    const std::vector<StoredSource::Id>& source_ids) {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteSourcesSql[] =
      "DELETE FROM impressions WHERE impression_id = ?";
  sql::Statement delete_impression_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteSourcesSql));

  for (StoredSource::Id source_id : source_ids) {
    delete_impression_statement.Reset(/*clear_bound_vars=*/true);
    delete_impression_statement.BindInt64(0, *source_id);
    if (!delete_impression_statement.Run())
      return false;
  }

  static constexpr char kDeleteDedupKeySql[] =
      "DELETE FROM dedup_keys WHERE impression_id = ?";
  sql::Statement delete_dedup_key_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteDedupKeySql));

  for (StoredSource::Id source_id : source_ids) {
    delete_dedup_key_statement.Reset(/*clear_bound_vars=*/true);
    delete_dedup_key_statement.BindInt64(0, *source_id);
    if (!delete_dedup_key_statement.Run())
      return false;
  }

  return transaction.Commit();
}

bool AttributionStorageSql::AddAggregatableAttributionForTesting(
    const AggregatableAttribution& aggregatable_attribution) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kCreateIfAbsent))
    return false;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  static constexpr char kInsertMetadataSql[] =
      "INSERT INTO aggregatable_report_metadata"
      "(source_id,trigger_time)"
      "VALUES(?,?)";
  sql::Statement insert_metadata_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertMetadataSql));
  insert_metadata_statement.BindInt64(0, *aggregatable_attribution.source_id);
  insert_metadata_statement.BindTime(1, aggregatable_attribution.trigger_time);
  if (!insert_metadata_statement.Run())
    return false;

  AggregatableAttribution::Id aggregation_id(db_->GetLastInsertRowId());

  static constexpr char kInsertContributionsSql[] =
      "INSERT INTO aggregatable_contributions"
      "(aggregation_id,report_time,failed_send_attempts,bucket,value,"
      "external_report_id)"
      "VALUES(?,?,0,?,?,?)";
  sql::Statement insert_contributions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertContributionsSql));

  for (const HistogramContribution& contribution :
       aggregatable_attribution.contributions) {
    insert_contributions_statement.Reset(/*clear_bound_vars=*/true);
    insert_contributions_statement.BindInt64(0, *aggregation_id);
    insert_contributions_statement.BindTime(
        1, aggregatable_attribution.report_time);
    insert_contributions_statement.BindString(2, contribution.bucket());
    insert_contributions_statement.BindInt64(
        3, static_cast<int64_t>(contribution.value()));
    insert_contributions_statement.BindString(
        4, delegate_->NewReportID().AsLowercaseString());
    if (!insert_contributions_statement.Run())
      return false;
  }

  return transaction.Commit();
}

bool AttributionStorageSql::ClearAggregatableAttributionForOriginsInRange(
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const url::Origin&)> filter,
    std::vector<StoredSource::Id>& source_ids_to_delete) {
  DCHECK_LE(delete_begin, delete_end);

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  // TODO(linnan): Considering optimizing SQL query by moving some logic to C++.
  // See the comment in crrev.com/c/3379484 for more information.
  static constexpr char kScanCandidateData[] =
      "SELECT I.impression_origin,I.conversion_origin,I.reporting_origin,"
      "I.impression_id,A.aggregation_id "
      "FROM impressions I LEFT JOIN aggregatable_report_metadata A "
      DCHECK_SQL_INDEXED_BY("aggregate_trigger_time_idx")
      "ON A.source_id=I.impression_id WHERE"
      "(I.impression_time BETWEEN ?1 AND ?2)OR"
      "(A.trigger_time BETWEEN ?1 AND ?2)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kScanCandidateData));
  statement.BindTime(0, delete_begin);
  statement.BindTime(1, delete_end);

  while (statement.Step()) {
    if (filter.is_null() ||
        filter.Run(DeserializeOrigin(statement.ColumnString(0))) ||
        filter.Run(DeserializeOrigin(statement.ColumnString(1))) ||
        filter.Run(DeserializeOrigin(statement.ColumnString(2)))) {
      source_ids_to_delete.emplace_back(statement.ColumnInt64(3));
      if (statement.GetColumnType(4) != sql::ColumnType::kNull &&
          !ClearAggregatableAttribution(
              AggregatableAttribution::Id(statement.ColumnInt64(4)))) {
        return false;
      }
    }
  }

  if (!statement.Succeeded())
    return false;

  return transaction.Commit();
}

bool AttributionStorageSql::ClearAggregatableAttribution(
    AggregatableAttribution::Id aggregation_id) {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteAggregationSql[] =
      "DELETE FROM aggregatable_report_metadata WHERE aggregation_id=?";
  sql::Statement delete_aggregation_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAggregationSql));
  delete_aggregation_statement.BindInt64(0, *aggregation_id);
  if (!delete_aggregation_statement.Run())
    return false;

  if (!ClearAggregatableContributions(aggregation_id))
    return false;

  return transaction.Commit();
}

bool AttributionStorageSql::ClearAggregatableContributions(
    AggregatableAttribution::Id aggregation_id) {
  static constexpr char kDeleteContributionsSql[] =
      // clang-format off
      "DELETE FROM aggregatable_contributions "
      DCHECK_SQL_INDEXED_BY("contribution_aggregation_id_idx")
      "WHERE aggregation_id=?";  // clang-format on
  sql::Statement delete_contributions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteContributionsSql));
  delete_contributions_statement.BindInt64(0, *aggregation_id);
  return delete_contributions_statement.Run();
}

bool AttributionStorageSql::ClearAggregatableAttributionForSourceIds(
    const std::vector<StoredSource::Id>& source_ids) {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteAggregationsSql[] =
      "DELETE FROM aggregatable_report_metadata "
      DCHECK_SQL_INDEXED_BY("aggregate_source_id_idx")
      "WHERE source_id=? "
      "RETURNING aggregation_id";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAggregationsSql));

  for (StoredSource::Id id : source_ids) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, *id);

    while (statement.Step()) {
      if (!ClearAggregatableContributions(
              AggregatableAttribution::Id(statement.ColumnInt64(0)))) {
        return false;
      }
    }

    if (!statement.Succeeded())
      return false;
  }

  return transaction.Commit();
}

std::vector<AttributionReport>
AttributionStorageSql::GetAggregatableContributionReportsForTesting(
    base::Time max_report_time,
    int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return {};

  // TODO(linnan): Consider breaking the SQL query down for simplicity.
  // See the comment in crrev.com/c/3379484 for more information.
  static constexpr char kGetContributionsSql[] =
      "SELECT C.contribution_id,C.report_time,C.failed_send_attempts,"
      "C.bucket,C.value,C.external_report_id,A.trigger_time,"
      "I.impression_origin,I.conversion_origin,I.reporting_origin,"
      "I.impression_data,I.impression_time,I.expiry_time,I.impression_id,"
      "I.source_type,I.priority,I.attributed_truthfully,I.debug_key "
      "FROM aggregatable_contributions AS C "
      DCHECK_SQL_INDEXED_BY("contribution_report_time_idx")
      "JOIN aggregatable_report_metadata AS A "
      DCHECK_SQL_INDEXED_BY("aggregate_source_id_idx")
      "ON C.aggregation_id=A.aggregation_id "
      "JOIN impressions AS I ON A.source_id=I.impression_id "
      "WHERE C.report_time<=? "
      "ORDER BY C.aggregation_id, C.contribution_id LIMIT ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetContributionsSql));
  statement.BindTime(0, max_report_time);
  statement.BindInt(1, limit);

  std::vector<AttributionReport> reports;
  while (statement.Step()) {
    AttributionReport::AggregatableContributionData::Id report_id(
        statement.ColumnInt64(0));
    base::Time report_time = statement.ColumnTime(1);
    int failed_send_attempts = statement.ColumnInt(2);
    std::string bucket = statement.ColumnString(3);
    int64_t value = statement.ColumnInt64(4);
    base::GUID external_report_id =
        base::GUID::ParseLowercase(statement.ColumnString(5));
    base::Time trigger_time = statement.ColumnTime(6);
    url::Origin impression_origin =
        DeserializeOrigin(statement.ColumnString(7));
    url::Origin conversion_origin =
        DeserializeOrigin(statement.ColumnString(8));
    url::Origin reporting_origin = DeserializeOrigin(statement.ColumnString(9));
    uint64_t source_event_id = DeserializeUint64(statement.ColumnInt64(10));
    base::Time impression_time = statement.ColumnTime(11);
    base::Time expiry_time = statement.ColumnTime(12);
    StoredSource::Id source_id(statement.ColumnInt64(13));
    absl::optional<CommonSourceInfo::SourceType> source_type =
        DeserializeSourceType(statement.ColumnInt(14));
    int64_t attribution_source_priority = statement.ColumnInt64(15);
    absl::optional<StoredSource::AttributionLogic> attribution_logic =
        DeserializeAttributionLogic(statement.ColumnInt(16));
    absl::optional<uint64_t> source_debug_key =
        ColumnUint64OrNull(statement, 17);

    // Ensure origins are valid before continuing. This could happen if there is
    // database corruption.
    if (bucket.empty() || value < 0 ||
        value > std::numeric_limits<uint32_t>::max() ||
        !external_report_id.is_valid() || impression_origin.opaque() ||
        conversion_origin.opaque() || reporting_origin.opaque() ||
        !source_type.has_value() || !attribution_logic.has_value() ||
        failed_send_attempts < 0) {
      continue;
    }

    // Create the source and AggregatableContributionReport objects from the
    // retrieved columns.
    StoredSource source(
        CommonSourceInfo(source_event_id, std::move(impression_origin),
                         std::move(conversion_origin),
                         std::move(reporting_origin), impression_time,
                         expiry_time, *source_type, attribution_source_priority,
                         source_debug_key),
        *attribution_logic, source_id);

    // TODO(linnan): Store and read trigger_debug_key.
    AttributionReport report(
        AttributionInfo(std::move(source), trigger_time,
                        /*debug_key=*/absl::nullopt),
        report_time, std::move(external_report_id),
        AttributionReport::AggregatableContributionData(
            HistogramContribution(std::move(bucket),
                                  static_cast<uint32_t>(value)),
            report_id));
    report.set_failed_send_attempts(failed_send_attempts);

    reports.push_back(std::move(report));
  }

  if (!statement.Succeeded())
    return {};

  return reports;
}

bool AttributionStorageSql::DeleteAggregatableContributionReport(
    AttributionReport::AggregatableContributionData::Id report_id) {
  static constexpr char kSelectCountSql[] =
      "SELECT COUNT(*),aggregation_id FROM aggregatable_contributions "
      DCHECK_SQL_INDEXED_BY("contribution_aggregation_id_idx")
      "WHERE aggregation_id=("
      "SELECT aggregation_id FROM aggregatable_contributions "
      "WHERE contribution_id=?)";
  sql::Statement select_count_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectCountSql));
  select_count_statement.BindInt64(0, *report_id);

  if (!select_count_statement.Step())
    return select_count_statement.Succeeded();

  int64_t count = select_count_statement.ColumnInt64(0);
  if (count == 0)
    return true;

  AggregatableAttribution::Id aggregation_id(
      select_count_statement.ColumnInt64(1));

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteReportSql[] =
      "DELETE FROM aggregatable_contributions WHERE contribution_id=?";
  sql::Statement delete_report_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteReportSql));
  delete_report_statement.BindInt64(0, *report_id);

  if (!delete_report_statement.Run())
    return false;

  if (count != 1)
    return transaction.Commit();

  // If this is the last row in `aggregatable_contributions` table with the
  // corresponding `aggregation_id`, also delete the row in
  // `aggregatable_report_metadata` table.
  static constexpr char kDeleteMetadataSql[] =
      "DELETE FROM aggregatable_report_metadata WHERE aggregation_id=?";
  sql::Statement delete_metadata_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteMetadataSql));
  delete_metadata_statement.BindInt64(0, *aggregation_id);

  if (!delete_metadata_statement.Run())
    return false;

  return transaction.Commit();
}

}  // namespace content
