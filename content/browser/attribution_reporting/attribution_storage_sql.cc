// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/ignore_result.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/storable_trigger.h"
#include "sql/database.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

namespace {

using CreateReportResult = ::content::AttributionStorage::CreateReportResult;
using CreateReportStatus =
    ::content::AttributionStorage::CreateReportResult::Status;
using DeactivatedSource = ::content::AttributionStorage::DeactivatedSource;

const base::FilePath::CharType kInMemoryPath[] = FILE_PATH_LITERAL(":memory");

const base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("Conversions");

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
const int kCurrentVersionNumber = 15;

// Earliest version which can use a |kCurrentVersionNumber| database
// without failing.
const int kCompatibleVersionNumber = 15;

// Latest version of the database that cannot be upgraded to
// |kCurrentVersionNumber| without razing the database. No versions are
// currently deprecated.
const int kDeprecatedVersionNumber = 0;

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

WARN_UNUSED_RESULT int SerializeAttributionLogic(
    StorableSource::AttributionLogic val) {
  return static_cast<int>(val);
}

WARN_UNUSED_RESULT absl::optional<StorableSource::AttributionLogic>
DeserializeAttributionLogic(int val) {
  switch (val) {
    case static_cast<int>(StorableSource::AttributionLogic::kNever):
      return StorableSource::AttributionLogic::kNever;
    case static_cast<int>(StorableSource::AttributionLogic::kTruthfully):
      return StorableSource::AttributionLogic::kTruthfully;
    case static_cast<int>(StorableSource::AttributionLogic::kFalsely):
      return StorableSource::AttributionLogic::kFalsely;
    default:
      return absl::nullopt;
  }
}

WARN_UNUSED_RESULT int SerializeSourceType(StorableSource::SourceType val) {
  return static_cast<int>(val);
}

WARN_UNUSED_RESULT absl::optional<StorableSource::SourceType>
DeserializeSourceType(int val) {
  switch (val) {
    case static_cast<int>(StorableSource::SourceType::kNavigation):
      return StorableSource::SourceType::kNavigation;
    case static_cast<int>(StorableSource::SourceType::kEvent):
      return StorableSource::SourceType::kEvent;
    default:
      return absl::nullopt;
  }
}

struct SourceToAttribute {
  StorableSource source;
  int num_conversions;
};

WARN_UNUSED_RESULT absl::optional<SourceToAttribute> ReadSourceToAttribute(
    sql::Database* db,
    StorableSource::Id source_id,
    const url::Origin& reporting_origin) {
  static constexpr char kReadSourceToAttributeSql[] =
      "SELECT impression_origin,impression_time,priority,"
      "conversion_origin,attributed_truthfully,source_type,num_conversions,"
      "impression_data,expiry_time "
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

  absl::optional<StorableSource::AttributionLogic> attribution_logic =
      DeserializeAttributionLogic(statement.ColumnInt(4));
  // There should never be an unattributed source with `kFalsely`.
  if (!attribution_logic.has_value() ||
      attribution_logic == StorableSource::AttributionLogic::kFalsely) {
    return absl::nullopt;
  }

  absl::optional<StorableSource::SourceType> source_type =
      DeserializeSourceType(statement.ColumnInt(5));
  if (!source_type.has_value())
    return absl::nullopt;

  int num_conversions = statement.ColumnInt(6);
  if (num_conversions < 0)
    return absl::nullopt;

  uint64_t source_event_id = DeserializeUint64(statement.ColumnInt64(7));
  base::Time expiry_time = statement.ColumnTime(8);

  return SourceToAttribute{
      .source = StorableSource(source_event_id, std::move(impression_origin),
                               std::move(conversion_origin), reporting_origin,
                               impression_time, expiry_time, *source_type,
                               priority, *attribution_logic, source_id),
      .num_conversions = num_conversions,
  };
}

// Helper to deserialize source rows. See `GetActiveSources()` for the
// expected ordering of columns used for the input to this function.
absl::optional<StorableSource> ReadSourceFromStatement(
    sql::Statement& statement) {
  StorableSource::Id source_id(statement.ColumnInt64(0));
  uint64_t source_event_id = DeserializeUint64(statement.ColumnInt64(1));
  url::Origin impression_origin = DeserializeOrigin(statement.ColumnString(2));
  url::Origin conversion_origin = DeserializeOrigin(statement.ColumnString(3));
  url::Origin reporting_origin = DeserializeOrigin(statement.ColumnString(4));
  base::Time impression_time = statement.ColumnTime(5);
  base::Time expiry_time = statement.ColumnTime(6);
  absl::optional<StorableSource::SourceType> source_type =
      DeserializeSourceType(statement.ColumnInt(7));
  absl::optional<StorableSource::AttributionLogic> attribution_logic =
      DeserializeAttributionLogic(statement.ColumnInt(8));
  int64_t priority = statement.ColumnInt64(9);

  if (!source_type.has_value() || !attribution_logic.has_value())
    return absl::nullopt;

  return StorableSource(source_event_id, std::move(impression_origin),
                        std::move(conversion_origin),
                        std::move(reporting_origin), impression_time,
                        expiry_time, *source_type, priority, *attribution_logic,
                        source_id);
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
    std::unique_ptr<Delegate> delegate,
    const base::Clock* clock)
    : path_to_database_(g_run_in_memory_
                            ? base::FilePath(kInMemoryPath)
                            : path_to_database.Append(kDatabasePath)),
      rate_limit_table_(delegate.get(), clock),
      clock_(clock),
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
        "source_type,attributed_truthfully,priority "
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
      absl::optional<StorableSource> source =
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
    absl::optional<std::vector<int64_t>> dedup_keys =
        ReadDedupKeys(*deactivated_source.source.impression_id());
    if (!dedup_keys.has_value())
      return absl::nullopt;
    deactivated_source.source.SetDedupKeys(std::move(*dedup_keys));
  }

  return deactivated_sources;
}

std::vector<DeactivatedSource> AttributionStorageSql::StoreSource(
    const StorableSource& source,
    int deactivated_source_return_limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Force the creation of the database if it doesn't exist, as we need to
  // persist the source.
  if (!LazyInit(DbCreationPolicy::kCreateIfAbsent))
    return {};

  // Only delete expired impressions periodically to avoid excessive DB
  // operations.
  const base::TimeDelta delete_frequency =
      delegate_->GetDeleteExpiredSourcesFrequency();
  DCHECK_GE(delete_frequency, base::TimeDelta());
  const base::Time now = clock_->Now();
  if (now - last_deleted_expired_sources_ >= delete_frequency) {
    if (!DeleteExpiredSources())
      return {};
    last_deleted_expired_sources_ = now;
  }

  // TODO(csharrison): Thread this failure to the caller and report a console
  // error.
  const std::string serialized_impression_origin =
      SerializeOrigin(source.impression_origin());
  if (!HasCapacityForStoringSource(serialized_impression_origin))
    return {};

  // Wrap the deactivation and insertion in the same transaction. If the
  // deactivation fails, we do not want to store the new source as we may
  // return the wrong set of sources for a trigger.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return {};

  if (!EnsureCapacityForPendingDestinationLimit(source))
    return {};

  const std::string serialized_conversion_destination =
      source.ConversionDestination().Serialize();
  const std::string serialized_reporting_origin =
      SerializeOrigin(source.reporting_origin());

  // In the case where we get a new source for a given <reporting_origin,
  // conversion_destination> we should mark all active, converted impressions
  // with the matching <reporting_origin, conversion_destination> as not active.
  absl::optional<std::vector<DeactivatedSource>> deactivated_sources =
      DeactivateSources(serialized_conversion_destination,
                        serialized_reporting_origin,
                        deactivated_source_return_limit);
  if (!deactivated_sources.has_value())
    return {};

  static constexpr char kInsertImpressionSql[] =
      "INSERT INTO impressions"
      "(impression_data,impression_origin,conversion_origin,"
      "conversion_destination,"
      "reporting_origin,impression_time,expiry_time,source_type,"
      "attributed_truthfully,priority,impression_site,"
      "num_conversions,active)"
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertImpressionSql));
  statement.BindInt64(0, SerializeUint64(source.source_event_id()));
  statement.BindString(1, serialized_impression_origin);
  statement.BindString(2, SerializeOrigin(source.conversion_origin()));
  statement.BindString(3, serialized_conversion_destination);
  statement.BindString(4, serialized_reporting_origin);
  statement.BindTime(5, source.impression_time());
  statement.BindTime(6, source.expiry_time());
  statement.BindInt(7, SerializeSourceType(source.source_type()));
  statement.BindInt(8, SerializeAttributionLogic(source.attribution_logic()));
  statement.BindInt64(9, source.priority());
  statement.BindString(10, source.ImpressionSite().Serialize());

  if (source.attribution_logic() ==
      StorableSource::AttributionLogic::kFalsely) {
    // Falsely attributed impressions are immediately stored with
    // `num_conversions == 1` and `active == 0`, as they will be attributed via
    // the below call to `StoreReport()` in the same transaction.
    statement.BindInt(11, 1);  // num_conversions
    statement.BindInt(12, 0);  // active
  } else {
    statement.BindInt(11, 0);  // num_conversions
    statement.BindInt(12, 1);  // active
  }

  if (!statement.Run())
    return {};

  if (source.attribution_logic() ==
      StorableSource::AttributionLogic::kFalsely) {
    DCHECK_EQ(StorableSource::SourceType::kEvent, source.source_type());

    StorableSource::Id source_id(db_->GetLastInsertRowId());
    uint64_t event_source_trigger_data =
        delegate_->GetFakeEventSourceTriggerData();

    const base::Time conversion_time = source.impression_time();
    const base::Time report_time =
        delegate_->GetReportTime(source, conversion_time);

    AttributionReport report(source, event_source_trigger_data,
                             /*conversion_time=*/conversion_time,
                             /*report_time=*/report_time,
                             /*priority=*/0,
                             /*external_report_id=*/
                             delegate_->NewReportID(),
                             /*conversion_id=*/absl::nullopt);

    if (!StoreReport(report, source_id))
      return {};
  }

  if (!transaction.Commit())
    return {};
  return *deactivated_sources;
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
AttributionStorageSql::MaybeReplaceLowerPriorityReportResult
AttributionStorageSql::MaybeReplaceLowerPriorityReport(
    const AttributionReport& report,
    int num_conversions,
    int64_t conversion_priority,
    absl::optional<AttributionReport>& replaced_report) {
  DCHECK(report.impression.impression_id().has_value());
  DCHECK_GE(num_conversions, 0);

  // If there's already capacity for the new report, there's nothing to do.
  if (num_conversions <
      delegate_->GetMaxAttributionsPerSource(report.impression.source_type())) {
    return MaybeReplaceLowerPriorityReportResult::kAddNewReport;
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
  min_priority_statement.BindInt64(0,
                                   *report.impression.impression_id().value());
  min_priority_statement.BindTime(1, report.report_time);

  const bool has_matching_report = min_priority_statement.Step();
  if (!min_priority_statement.Succeeded())
    return MaybeReplaceLowerPriorityReportResult::kError;

  // Deactivate the source as a new report will never be generated in the
  // future.
  if (!has_matching_report) {
    static constexpr char kDeactivateSql[] =
        "UPDATE impressions SET active = 0 WHERE impression_id = ?";
    sql::Statement deactivate_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kDeactivateSql));
    deactivate_statement.BindInt64(0,
                                   *report.impression.impression_id().value());
    return deactivate_statement.Run()
               ? MaybeReplaceLowerPriorityReportResult::
                     kDropNewReportSourceDeactivated
               : MaybeReplaceLowerPriorityReportResult::kError;
  }

  int64_t min_priority = min_priority_statement.ColumnInt64(0);
  AttributionReport::Id conversion_id_with_min_priority(
      min_priority_statement.ColumnInt64(1));

  // If the new report's priority is less than all existing ones, or if its
  // priority is equal to the minimum existing one and it is more recent, drop
  // it. We could explicitly check the trigger time here, but it would only
  // be relevant in the case of an ill-behaved clock, in which case the rest of
  // the attribution functionality would probably also break.
  if (conversion_priority <= min_priority) {
    return MaybeReplaceLowerPriorityReportResult::kDropNewReport;
  }

  absl::optional<AttributionReport> replaced =
      GetReport(conversion_id_with_min_priority);
  if (!replaced.has_value()) {
    return MaybeReplaceLowerPriorityReportResult::kError;
  }

  // Otherwise, delete the existing report with the lowest priority.
  if (!DeleteReportInternal(conversion_id_with_min_priority)) {
    return MaybeReplaceLowerPriorityReportResult::kError;
  }

  replaced_report = std::move(replaced);
  return MaybeReplaceLowerPriorityReportResult::kReplaceOldReport;
}

CreateReportResult AttributionStorageSql::MaybeCreateAndStoreReport(
    const StorableTrigger& trigger) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We don't bother creating the DB here if it doesn't exist, because it's not
  // possible for there to be a matching source if there's no DB.
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return CreateReportResult(CreateReportStatus::kNoMatchingImpressions);
  }

  const net::SchemefulSite& conversion_destination =
      trigger.conversion_destination();
  const std::string serialized_conversion_destination =
      conversion_destination.Serialize();

  const url::Origin& reporting_origin = trigger.reporting_origin();
  DCHECK(!conversion_destination.opaque());
  DCHECK(!reporting_origin.opaque());

  base::Time current_time = clock_->Now();

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
    return CreateReportResult(matching_sources_statement.Succeeded()
                                  ? CreateReportStatus::kNoMatchingImpressions
                                  : CreateReportStatus::kInternalError);
  }

  // The first one returned will be attributed; it has the highest priority.
  StorableSource::Id source_id_to_attribute(
      matching_sources_statement.ColumnInt64(0));

  // Any others will be deleted.
  std::vector<StorableSource::Id> source_ids_to_delete;
  while (matching_sources_statement.Step()) {
    StorableSource::Id source_id(matching_sources_statement.ColumnInt64(0));
    source_ids_to_delete.push_back(source_id);
  }
  // Exit early if the last statement wasn't valid.
  if (!matching_sources_statement.Succeeded()) {
    return CreateReportResult(CreateReportStatus::kInternalError);
  }

  switch (ReportAlreadyStored(source_id_to_attribute, trigger.dedup_key())) {
    case ReportAlreadyStoredStatus::kNotStored:
      break;
    case ReportAlreadyStoredStatus::kStored:
      return CreateReportResult(CreateReportStatus::kDeduplicated);
    case ReportAlreadyStoredStatus::kError:
      return CreateReportResult(CreateReportStatus::kInternalError);
  }

  switch (CapacityForStoringReport(serialized_conversion_destination)) {
    case ConversionCapacityStatus::kHasCapacity:
      break;
    case ConversionCapacityStatus::kNoCapacity:
      return CreateReportResult(
          CreateReportStatus::kNoCapacityForConversionDestination);
    case ConversionCapacityStatus::kError:
      return CreateReportResult(CreateReportStatus::kInternalError);
  }

  absl::optional<SourceToAttribute> source_to_attribute = ReadSourceToAttribute(
      db_.get(), source_id_to_attribute, reporting_origin);
  // This is only possible if there is a corrupt DB.
  if (!source_to_attribute.has_value()) {
    return CreateReportResult(CreateReportStatus::kInternalError);
  }

  const uint64_t trigger_data = source_to_attribute->source.source_type() ==
                                        StorableSource::SourceType::kEvent
                                    ? trigger.event_source_trigger_data()
                                    : trigger.trigger_data();

  const base::Time report_time =
      delegate_->GetReportTime(source_to_attribute->source,
                               /*trigger_time=*/current_time);
  AttributionReport report(std::move(source_to_attribute->source), trigger_data,
                           /*conversion_time=*/current_time,
                           /*report_time=*/report_time, trigger.priority(),
                           /*external_report_id=*/delegate_->NewReportID(),
                           /*conversion_id=*/absl::nullopt);

  switch (
      rate_limit_table_.AttributionAllowed(db_.get(), report, current_time)) {
    case RateLimitTable::AttributionAllowedStatus::kAllowed:
      break;
    case RateLimitTable::AttributionAllowedStatus::kNotAllowed:
      return CreateReportResult(CreateReportStatus::kRateLimited);
    case RateLimitTable::AttributionAllowedStatus::kError:
      return CreateReportResult(CreateReportStatus::kInternalError);
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return CreateReportResult(CreateReportStatus::kInternalError);
  }

  absl::optional<AttributionReport> replaced_report;
  const auto maybe_replace_lower_priority_report_result =
      MaybeReplaceLowerPriorityReport(report,
                                      source_to_attribute->num_conversions,
                                      trigger.priority(), replaced_report);
  if (maybe_replace_lower_priority_report_result ==
      MaybeReplaceLowerPriorityReportResult::kError) {
    return CreateReportResult(CreateReportStatus::kInternalError);
  }

  if (maybe_replace_lower_priority_report_result ==
          MaybeReplaceLowerPriorityReportResult::kDropNewReport ||
      maybe_replace_lower_priority_report_result ==
          MaybeReplaceLowerPriorityReportResult::
              kDropNewReportSourceDeactivated) {
    if (!transaction.Commit()) {
      return CreateReportResult(CreateReportStatus::kInternalError);
    }
    return CreateReportResult(
        CreateReportStatus::kPriorityTooLow, std::move(report),
        maybe_replace_lower_priority_report_result ==
                MaybeReplaceLowerPriorityReportResult::
                    kDropNewReportSourceDeactivated
            ? absl::make_optional(
                  DeactivatedSource::Reason::kReachedAttributionLimit)
            : absl::nullopt);
  }

  // Reports with `AttributionLogic::kNever` should be included in all
  // attribution operations and matching, but only `kTruthfully` should generate
  // reports that get sent.
  const bool create_report = report.impression.attribution_logic() ==
                             StorableSource::AttributionLogic::kTruthfully;

  if (create_report) {
    DCHECK(report.impression.impression_id().has_value());
    if (!StoreReport(report, *report.impression.impression_id())) {
      return CreateReportResult(CreateReportStatus::kInternalError);
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
    insert_dedup_key_statement.BindInt64(
        0, *report.impression.impression_id().value());
    insert_dedup_key_statement.BindInt64(1, *trigger.dedup_key());
    if (!insert_dedup_key_statement.Run()) {
      return CreateReportResult(CreateReportStatus::kInternalError);
    }
  }

  // Only increment the number of conversions associated with the source if
  // we are adding a new one, rather than replacing a dropped one.
  if (maybe_replace_lower_priority_report_result ==
      MaybeReplaceLowerPriorityReportResult::kAddNewReport) {
    static constexpr char kUpdateImpressionForConversionSql[] =
        "UPDATE impressions SET num_conversions = num_conversions + 1 "
        "WHERE impression_id = ?";
    sql::Statement impression_update_statement(db_->GetCachedStatement(
        SQL_FROM_HERE, kUpdateImpressionForConversionSql));

    // Update the attributed source.
    impression_update_statement.BindInt64(
        0, *report.impression.impression_id().value());
    if (!impression_update_statement.Run()) {
      return CreateReportResult(CreateReportStatus::kInternalError);
    }
  }

  // Delete all unattributed sources.
  if (!DeleteSources(source_ids_to_delete)) {
    return CreateReportResult(CreateReportStatus::kInternalError);
  }

  // Based on the deletion logic here and the fact that we delete sources
  // with |num_conversions > 1| when there is a new matching source in
  // |StoreSource()|, we should be guaranteed that these sources all
  // have |num_conversions == 0|, and that they never contributed to a rate
  // limit. Therefore, we don't need to call
  // |RateLimitTable::ClearDataForSourceIds()| here.

  if (create_report && !rate_limit_table_.AddRateLimit(db_.get(), report)) {
    return CreateReportResult(CreateReportStatus::kInternalError);
  }

  if (!transaction.Commit()) {
    return CreateReportResult(CreateReportStatus::kInternalError);
  }

  if (!create_report) {
    return CreateReportResult(CreateReportStatus::kDroppedForNoise,
                              std::move(report));
  }

  return CreateReportResult(
      maybe_replace_lower_priority_report_result ==
              MaybeReplaceLowerPriorityReportResult::kReplaceOldReport
          ? CreateReportStatus::kSuccessDroppedLowerPriority
          : CreateReportStatus::kSuccess,
      std::move(replaced_report));
}

bool AttributionStorageSql::StoreReport(const AttributionReport& report,
                                        StorableSource::Id source_id) {
  static constexpr char kStoreReportSql[] =
      "INSERT INTO conversions"
      "(impression_id,conversion_data,conversion_time,report_time,"
      "priority,failed_send_attempts,external_report_id)VALUES(?,?,?,?,?,0,?)";
  sql::Statement store_report_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kStoreReportSql));
  store_report_statement.BindInt64(0, *source_id);
  store_report_statement.BindInt64(1, SerializeUint64(report.trigger_data));
  store_report_statement.BindTime(2, report.conversion_time);
  store_report_statement.BindTime(3, report.report_time);
  store_report_statement.BindInt64(4, report.priority);
  store_report_statement.BindString(
      5, report.external_report_id.AsLowercaseString());
  return store_report_statement.Run();
}

namespace {

// Helper to deserialize report rows. See `GetReport()` for the expected
// ordering of columns used for the input to this function.
absl::optional<AttributionReport> ReadReportFromStatement(
    sql::Statement& statement) {
  uint64_t trigger_data = DeserializeUint64(statement.ColumnInt64(0));
  base::Time conversion_time = statement.ColumnTime(1);
  base::Time report_time = statement.ColumnTime(2);
  AttributionReport::Id conversion_id(statement.ColumnInt64(3));
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
  StorableSource::Id source_id(statement.ColumnInt64(13));
  absl::optional<StorableSource::SourceType> source_type =
      DeserializeSourceType(statement.ColumnInt(14));
  int64_t attribution_source_priority = statement.ColumnInt64(15);
  absl::optional<StorableSource::AttributionLogic> attribution_logic =
      DeserializeAttributionLogic(statement.ColumnInt(16));

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
  StorableSource source(source_event_id, std::move(impression_origin),
                        std::move(conversion_origin),
                        std::move(reporting_origin), impression_time,
                        expiry_time, *source_type, attribution_source_priority,
                        *attribution_logic, source_id);

  AttributionReport report(std::move(source), trigger_data, conversion_time,
                           report_time, conversion_priority,
                           std::move(external_report_id), conversion_id);
  report.failed_send_attempts = failed_send_attempts;
  return report;
}

}  // namespace

std::vector<AttributionReport> AttributionStorageSql::GetAttributionsToReport(
    base::Time max_report_time,
    int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return {};

  // Get at most |limit| entries in the conversions table with a |report_time|
  // less than |max_report_time| and their matching information from the
  // impression table. Negatives are treated as no limit
  // (https://sqlite.org/lang_select.html#limitoffset).
  static constexpr char kGetReportsSql[] =
      "SELECT C.conversion_data,C.conversion_time,C.report_time,"
      "C.conversion_id,C.priority,C.failed_send_attempts,C.external_report_id,"
      "I.impression_origin,I.conversion_origin,I.reporting_origin,"
      "I.impression_data,I.impression_time,I.expiry_time,I.impression_id,"
      "I.source_type,I.priority,I.attributed_truthfully "
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
  return reports;
}

absl::optional<AttributionReport> AttributionStorageSql::GetReport(
    AttributionReport::Id conversion_id) {
  static constexpr char kGetReportSql[] =
      "SELECT C.conversion_data,C.conversion_time,C.report_time,"
      "C.conversion_id,C.priority,C.failed_send_attempts,C.external_report_id,"
      "I.impression_origin,I.conversion_origin,I.reporting_origin,"
      "I.impression_data,I.impression_time,I.expiry_time,I.impression_id,"
      "I.source_type,I.priority,I.attributed_truthfully "
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
    while (true) {
      std::vector<StorableSource::Id> source_ids;
      while (statement.Step()) {
        StorableSource::Id source_id(statement.ColumnInt64(0));
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
      ")LIMIT ?";
  sql::Statement select_expired_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectExpiredSourcesSql));
  select_expired_statement.BindTime(0, clock_->Now());
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
  return DeleteReportInternal(report_id);
}

bool AttributionStorageSql::DeleteReportInternal(
    AttributionReport::Id report_id) {
  static constexpr char kDeleteReportSql[] =
      "DELETE FROM conversions WHERE conversion_id = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteReportSql));
  statement.BindInt64(0, *report_id);
  return statement.Run();
}

bool AttributionStorageSql::UpdateReportForSendFailure(
    AttributionReport::Id conversion_id,
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

void AttributionStorageSql::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const url::Origin&)> filter) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return;

  SCOPED_UMA_HISTOGRAM_TIMER("Conversions.ClearDataTime");
  if (filter.is_null()) {
    ClearAllDataInRange(delete_begin, delete_end);
    return;
  }

  // Measure the time it takes to perform a clear with a filter separately from
  // the above histogram.
  SCOPED_UMA_HISTOGRAM_TIMER("Conversions.Storage.ClearDataWithFilterDuration");

  // TODO(csharrison, johnidel): This query can be split up and optimized by
  // adding indexes on the impression_time and conversion_time columns.
  // See this comment for more information:
  // crrev.com/c/2150071/4/content/browser/conversions/conversion_storage_sql.cc#342
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

  std::vector<StorableSource::Id> source_ids_to_delete;
  std::vector<AttributionReport::Id> conversion_ids_to_delete;
  while (statement.Step()) {
    if (filter.Run(DeserializeOrigin(statement.ColumnString(0))) ||
        filter.Run(DeserializeOrigin(statement.ColumnString(1))) ||
        filter.Run(DeserializeOrigin(statement.ColumnString(2)))) {
      source_ids_to_delete.emplace_back(statement.ColumnInt64(3));
      if (statement.GetColumnType(4) != sql::ColumnType::kNull)
        conversion_ids_to_delete.emplace_back(statement.ColumnInt64(4));
    }
  }

  // TODO(csharrison, johnidel): Should we consider poisoning the DB if some of
  // the delete operations fail?
  if (!statement.Succeeded())
    return;

  // Since multiple reports can be associated with a single source,
  // deduplicate source IDs using a set to avoid redundant DB operations
  // below.
  source_ids_to_delete =
      base::flat_set<StorableSource::Id>(std::move(source_ids_to_delete))
          .extract();

  // Delete the data in a transaction to avoid cases where the source part
  // of a report is deleted without deleting the associated report, or
  // vice versa.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  if (!DeleteSources(source_ids_to_delete))
    return;

  for (AttributionReport::Id conversion_id : conversion_ids_to_delete) {
    if (!DeleteReportInternal(conversion_id))
      return;
  }

  int num_reports_deleted = static_cast<int>(conversion_ids_to_delete.size());

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
  for (StorableSource::Id source_id : source_ids_to_delete) {
    delete_vestigial_statement.Reset(/*clear_bound_vars=*/true);
    delete_vestigial_statement.BindInt64(0, *source_id);
    if (!delete_vestigial_statement.Run())
      return;

    num_reports_deleted += db_->GetLastChangeCount();
  }

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

void AttributionStorageSql::ClearAllDataInRange(base::Time delete_begin,
                                                base::Time delete_end) {
  // Browsing data remover will call this with null |delete_begin|, but also
  // perform the ClearAllDataAllTime optimization if |delete_begin| is
  // base::Time::Min().
  if ((delete_begin.is_null() || delete_begin.is_min()) &&
      delete_end.is_max()) {
    ClearAllDataAllTime();
    return;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  // Select all sources and reports in the given time range.
  // Note: This should follow the same basic logic in ClearData, with the
  // assumption that all origins match the filter. We cannot use a DELETE
  // statement, because we need the list of |impression_id|s to delete from the
  // |rate_limits| table.
  //
  // Optimizing these queries are also tough, see this comment for an idea:
  // http://crrev.com/c/2150071/12/content/browser/conversions/conversion_storage_sql.cc#468
  static constexpr char kSelectSourceRangeSql[] =
      "SELECT impression_id FROM impressions WHERE(impression_time BETWEEN ?1 "
      "AND ?2)OR "
      "impression_id IN(SELECT impression_id FROM conversions "
      "WHERE conversion_time BETWEEN ?1 AND ?2)";
  sql::Statement select_sources_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectSourceRangeSql));
  select_sources_statement.BindTime(0, delete_begin);
  select_sources_statement.BindTime(1, delete_end);

  std::vector<StorableSource::Id> source_ids_to_delete;
  while (select_sources_statement.Step()) {
    StorableSource::Id source_id(select_sources_statement.ColumnInt64(0));
    source_ids_to_delete.push_back(source_id);
  }
  if (!select_sources_statement.Succeeded())
    return;

  if (!DeleteSources(source_ids_to_delete))
    return;

  static constexpr char kDeleteReportRangeSql[] =
      "DELETE FROM conversions WHERE(conversion_time BETWEEN ? AND ?)"
      "OR impression_id NOT IN(SELECT impression_id FROM impressions)";
  sql::Statement delete_reports_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteReportRangeSql));
  delete_reports_statement.BindTime(0, delete_begin);
  delete_reports_statement.BindTime(1, delete_end);
  if (!delete_reports_statement.Run())
    return;

  int num_reports_deleted = db_->GetLastChangeCount();

  if (!rate_limit_table_.ClearDataForSourceIds(db_.get(), source_ids_to_delete))
    return;

  if (!rate_limit_table_.ClearAllDataInRange(db_.get(), delete_begin,
                                             delete_end))
    return;

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
AttributionStorageSql::ReportAlreadyStored(StorableSource::Id source_id,
                                           absl::optional<int64_t> dedup_key) {
  if (!dedup_key.has_value())
    return ReportAlreadyStoredStatus::kNotStored;

  static constexpr char kCountReportsSql[] =
      "SELECT COUNT(*)FROM dedup_keys "
      "WHERE impression_id = ? AND dedup_key = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kCountReportsSql));
  statement.BindInt64(0, *source_id);
  statement.BindInt64(1, *dedup_key);

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

std::vector<StorableSource> AttributionStorageSql::GetActiveSources(int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent))
    return {};

  // Negatives are treated as no limit
  // (https://sqlite.org/lang_select.html#limitoffset).
  static constexpr char kGetActiveSourcesSql[] =
      "SELECT impression_id,impression_data,impression_origin,"
      "conversion_origin,reporting_origin,impression_time,expiry_time,"
      "source_type,attributed_truthfully,priority "
      "FROM impressions "
      "WHERE active = 1 and expiry_time > ? "
      "LIMIT ?";

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetActiveSourcesSql));
  statement.BindTime(0, clock_->Now());
  statement.BindInt(1, limit);

  std::vector<StorableSource> sources;
  while (statement.Step()) {
    absl::optional<StorableSource> source = ReadSourceFromStatement(statement);
    if (source.has_value())
      sources.push_back(std::move(*source));
  }
  if (!statement.Succeeded())
    return {};

  for (auto& source : sources) {
    absl::optional<std::vector<int64_t>> dedup_keys =
        ReadDedupKeys(*source.impression_id());
    if (!dedup_keys.has_value())
      return {};
    source.SetDedupKeys(std::move(*dedup_keys));
  }

  return sources;
}

absl::optional<std::vector<int64_t>> AttributionStorageSql::ReadDedupKeys(
    StorableSource::Id source_id) {
  static constexpr char kDedupKeySql[] =
      "SELECT dedup_key FROM dedup_keys WHERE impression_id = ?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDedupKeySql));
  statement.BindInt64(0, *source_id);

  std::vector<int64_t> dedup_keys;
  while (statement.Step()) {
    dedup_keys.push_back(statement.ColumnInt64(0));
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

  if (InitializeSchema(db_init_status_ == DbStatus::kDeferringCreation) ==
      false) {
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

  int current_version = kCurrentVersionNumber;
  if (!sql::MetaTable::DoesTableExist(db_.get())) {
    // Version 1 of the schema did not have a metadata table.
    current_version = 1;
    if (!meta_table_.Init(db_.get(), current_version, kCompatibleVersionNumber))
      return false;
  } else {
    if (!meta_table_.Init(db_.get(), current_version, kCompatibleVersionNumber))
      return false;
    current_version = meta_table_.GetVersionNumber();
  }

  if (current_version == kCurrentVersionNumber)
    return true;

  if (current_version <= kDeprecatedVersionNumber) {
    // Note that this also razes the meta table, so it will need to be
    // initialized again.
    db_->Raze();
    return CreateSchema();
  }

  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    // In this case the database version is too new to be used. The DB will
    // never work until Chrome is re-upgraded. Assume the user will continue
    // using this Chrome version and raze the DB to get attribution reporting
    // working.
    db_->Raze();
    return CreateSchema();
  }

  return UpgradeAttributionStorageSqlSchema(db_.get(), &meta_table_,
                                            delegate_.get());
}

bool AttributionStorageSql::CreateSchema() {
  base::ThreadTicks start_timestamp = base::ThreadTicks::Now();
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
  // |StorableSource::AttributionLogic| enum.
  // |impression_site| is used to optimize the lookup of sources;
  // |StorableSource::ImpressionSite| is always derived from the origin.
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
      "impression_site TEXT NOT NULL)";
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

  // Optimizes `EnsureCapacityForPendingDestinationLimit()`, which only needs to
  // examine active, unconverted, event-source sources.
  static constexpr char kEventSourceImpressionSiteIndexSql[] =
      "CREATE INDEX IF NOT EXISTS event_source_impression_site_idx "
      "ON impressions(impression_site)"
      "WHERE active = 1 AND num_conversions = 0 AND source_type = 1";
  if (!db_->Execute(kEventSourceImpressionSiteIndexSql))
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
      "external_report_id TEXT NOT NULL)";
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

  if (!meta_table_.Init(db_.get(), kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    return false;
  }

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
    ignore_result(sql::Database::IsExpectedSqliteError(extended_error));
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

bool AttributionStorageSql::EnsureCapacityForPendingDestinationLimit(
    const StorableSource& source) {
  // TODO(apaseltiner): Add metrics for how this behaves so we can see how often
  // sites are hitting the limit.

  if (source.source_type() != StorableSource::SourceType::kEvent)
    return true;

  static_assert(static_cast<int>(StorableSource::SourceType::kEvent) == 1,
                "Update the SQL statement below and this condition");

  const std::string serialized_conversion_destination =
      source.ConversionDestination().Serialize();

  // Optimized by `kEventSourceImpressionSiteIndexSql`.
  static constexpr char kSelectSourcesSql[] =
      "SELECT impression_id,conversion_destination FROM impressions "
      DCHECK_SQL_INDEXED_BY("event_source_impression_site_idx")
      "WHERE impression_site = ? AND source_type = 1 "
      "AND active = 1 AND num_conversions = 0 "
      "ORDER BY impression_time ASC";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kSelectSourcesSql));
  statement.BindString(0, source.ImpressionSite().Serialize());

  base::flat_map<std::string, size_t> conversion_destinations;

  struct SourceData {
    StorableSource::Id source_id;
    std::string conversion_destination;
  };
  std::vector<SourceData> sources_by_impression_time;

  while (statement.Step()) {
    SourceData impression_data = {
        .source_id = StorableSource::Id(statement.ColumnInt64(0)),
        .conversion_destination = statement.ColumnString(1),
    };

    // If there's already a source matching the to-be-stored
    // `impression_site` and `conversion_destination`, then the unique count
    // won't be changed, so there's nothing else to do.
    if (impression_data.conversion_destination ==
        serialized_conversion_destination) {
      return true;
    }

    conversion_destinations[impression_data.conversion_destination]++;
    sources_by_impression_time.push_back(std::move(impression_data));
  }

  if (!statement.Succeeded())
    return false;

  const int max = delegate_->GetMaxAttributionDestinationsPerEventSource();
  // TODO(apaseltiner): We could just make
  // `GetMaxAttributionDestinationsPerEventSource()` return `size_t`, but it
  // would be inconsistent with the other `AttributionStorage::Delegate`
  // methods.
  DCHECK_GT(max, 0);

  // Otherwise, if there's capacity for the new `conversion_destination` to be
  // stored for the `impression_site`, there's nothing else to do.
  if (conversion_destinations.size() < static_cast<size_t>(max))
    return true;

  // Otherwise, delete sources in order by `impression_time` until the
  // number of distinct `conversion_destination`s is under `max`.
  std::vector<StorableSource::Id> source_ids_to_delete;
  for (const auto& source_data : sources_by_impression_time) {
    auto it = conversion_destinations.find(source_data.conversion_destination);
    DCHECK(it != conversion_destinations.end());
    it->second--;
    if (it->second == 0)
      conversion_destinations.erase(it);
    source_ids_to_delete.push_back(source_data.source_id);
    if (conversion_destinations.size() < static_cast<size_t>(max))
      break;
  }

  return DeleteSources(source_ids_to_delete);

  // Because this is limited to active sources with `num_conversions = 0`,
  // we should be guaranteed that there is not any corresponding data in the
  // rate limit table or the report table.
}

bool AttributionStorageSql::DeleteSources(
    const std::vector<StorableSource::Id>& source_ids) {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return false;

  static constexpr char kDeleteSourcesSql[] =
      "DELETE FROM impressions WHERE impression_id = ?";
  sql::Statement delete_impression_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteSourcesSql));

  for (StorableSource::Id source_id : source_ids) {
    delete_impression_statement.Reset(/*clear_bound_vars=*/true);
    delete_impression_statement.BindInt64(0, *source_id);
    if (!delete_impression_statement.Run())
      return false;
  }

  static constexpr char kDeleteDedupKeySql[] =
      "DELETE FROM dedup_keys WHERE impression_id = ?";
  sql::Statement delete_dedup_key_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteDedupKeySql));

  for (StorableSource::Id source_id : source_ids) {
    delete_dedup_key_statement.Reset(/*clear_bound_vars=*/true);
    delete_dedup_key_statement.BindInt64(0, *source_id);
    if (!delete_dedup_key_statement.Run())
      return false;
  }

  return transaction.Commit();
}

}  // namespace content
