// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql.h"

#include <stdint.h>

#include <functional>
#include <iterator>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/aggregation_service/aggregation_service.mojom.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_observer_types.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_source_type.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "content/browser/attribution_reporting/sql_queries.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/attribution_data_model.h"
#include "net/base/schemeful_site.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

// Version number of the database.
// TODO: remove the active_unattributed_sources_by_site_reporting_origin index
// during the next DB migration.
const int AttributionStorageSql::kCurrentVersionNumber = 43;

// Earliest version which can use a |kCurrentVersionNumber| database
// without failing.
const int AttributionStorageSql::kCompatibleVersionNumber = 43;

// Latest version of the database that cannot be upgraded to
// |kCurrentVersionNumber| without razing the database.
//
// Note that all versions >=15 were introduced during the transitional state of
// the Attribution Reporting API and can be removed when done.
const int AttributionStorageSql::kDeprecatedVersionNumber = 32;

namespace {

using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;
using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;

using ::attribution_reporting::SuitableOrigin;

const base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("Conversions");

constexpr int64_t kUnsetReportId = -1;

void RecordInitializationStatus(
    const AttributionStorageSql::InitStatus status) {
  base::UmaHistogramEnumeration("Conversions.Storage.Sql.InitStatus2", status);
}

void RecordSourcesDeleted(int count) {
  UMA_HISTOGRAM_COUNTS_1000(
      "Conversions.ImpressionsDeletedInDataClearOperation", count);
}

void RecordReportsDeleted(int event_count, int aggregatable_count) {
  UMA_HISTOGRAM_COUNTS_1000(
      "Conversions.ReportsDeletedInDataClearOperation.Event", event_count);
  UMA_HISTOGRAM_COUNTS_1000(
      "Conversions.ReportsDeletedInDataClearOperation.Aggregatable",
      aggregatable_count);
}

int64_t SerializeUint64(uint64_t data) {
  // There is no `sql::Statement::BindUint64()` method, so we reinterpret the
  // bits of `data` as an `int64_t`, which is safe because the value is opaque:
  // it is never used with arithmetic or comparison operations in the DB, only
  // stored and retrieved.
  return static_cast<int64_t>(data);
}

uint64_t DeserializeUint64(int64_t data) {
  // There is no `sql::Statement::ColumnUint64()` method, so we reinterpret the
  // bits of `data` as a `uint64_t`, which is safe because the value is opaque:
  // it is never used with arithmetic or comparison operations in the DB, only
  // stored and retrieved.
  return static_cast<uint64_t>(data);
}

// Prevent these functions from being called in the wrong direction.
int64_t SerializeUint64(int64_t data) = delete;
uint64_t DeserializeUint64(uint64_t data) = delete;

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

int SerializeSourceType(AttributionSourceType val) {
  return static_cast<int>(val);
}

absl::optional<AttributionSourceType> DeserializeSourceType(int val) {
  switch (val) {
    case static_cast<int>(AttributionSourceType::kNavigation):
      return AttributionSourceType::kNavigation;
    case static_cast<int>(AttributionSourceType::kEvent):
      return AttributionSourceType::kEvent;
    default:
      return absl::nullopt;
  }
}

int SerializeReportType(AttributionReport::Type val) {
  return static_cast<int>(val);
}

int SerializeAggregationCoordinator(
    ::aggregation_service::mojom::AggregationCoordinator val) {
  return static_cast<int>(val);
}

absl::optional<::aggregation_service::mojom::AggregationCoordinator>
DeserializeAggregationCoordinator(int val) {
  switch (val) {
    case static_cast<int>(
        ::aggregation_service::mojom::AggregationCoordinator::kAwsCloud):
      return ::aggregation_service::mojom::AggregationCoordinator::kAwsCloud;
    default:
      return absl::nullopt;
  }
}

std::string SerializeFilterData(
    const attribution_reporting::FilterData& filter_data) {
  proto::AttributionFilterData msg;

  for (const auto& [filter, values] : filter_data.filter_values()) {
    proto::AttributionFilterValues filter_values_msg;
    filter_values_msg.mutable_values()->Reserve(values.size());
    for (std::string value : values) {
      filter_values_msg.mutable_values()->Add(std::move(value));
    }
    (*msg.mutable_filter_values())[filter] = std::move(filter_values_msg);
  }

  std::string string;
  bool success = msg.SerializeToString(&string);
  DCHECK(success);
  return string;
}

absl::optional<attribution_reporting::FilterData> DeserializeFilterData(
    sql::Statement& stmt,
    int col) {
  std::string string;
  if (!stmt.ColumnBlobAsString(col, &string)) {
    return absl::nullopt;
  }

  proto::AttributionFilterData msg;
  if (!msg.ParseFromString(string)) {
    return absl::nullopt;
  }

  attribution_reporting::FilterValues::container_type filter_values;
  filter_values.reserve(msg.filter_values().size());

  for (google::protobuf::MapPair<std::string, proto::AttributionFilterValues>&
           entry : *msg.mutable_filter_values()) {
    // Serialized source filter data can only contain this key due to DB
    // corruption or deliberate modification.
    if (entry.first ==
        attribution_reporting::FilterData::kSourceTypeFilterKey) {
      continue;
    }

    google::protobuf::RepeatedPtrField<std::string>* values =
        entry.second.mutable_values();

    filter_values.emplace_back(
        entry.first,
        std::vector<std::string>(std::make_move_iterator(values->begin()),
                                 std::make_move_iterator(values->end())));
  }

  return attribution_reporting::FilterData::Create(std::move(filter_values));
}

std::string SerializeAggregationKeys(
    const attribution_reporting::AggregationKeys& keys) {
  proto::AttributionAggregatableSource msg;

  for (const auto& [id, key] : keys.keys()) {
    proto::AttributionAggregationKey key_msg;
    key_msg.set_high_bits(absl::Uint128High64(key));
    key_msg.set_low_bits(absl::Uint128Low64(key));
    (*msg.mutable_keys())[id] = std::move(key_msg);
  }

  std::string str;
  bool success = msg.SerializeToString(&str);
  DCHECK(success);
  return str;
}

absl::optional<attribution_reporting::AggregationKeys>
DeserializeAggregationKeys(sql::Statement& stmt, int col) {
  std::string str;
  if (!stmt.ColumnBlobAsString(col, &str)) {
    return absl::nullopt;
  }

  proto::AttributionAggregatableSource msg;
  if (!msg.ParseFromString(str)) {
    return absl::nullopt;
  }

  attribution_reporting::AggregationKeys::Keys::container_type keys;
  keys.reserve(msg.keys().size());

  for (const auto& [id, key] : msg.keys()) {
    if (!key.has_high_bits() || !key.has_low_bits()) {
      return absl::nullopt;
    }

    keys.emplace_back(id, absl::MakeUint128(key.high_bits(), key.low_bits()));
  }

  return attribution_reporting::AggregationKeys::FromKeys(std::move(keys));
}

absl::optional<StoredSource::ActiveState> GetSourceActiveState(
    bool event_level_active,
    bool aggregatable_active) {
  if (event_level_active && aggregatable_active) {
    return StoredSource::ActiveState::kActive;
  }

  if (!event_level_active && !aggregatable_active) {
    return StoredSource::ActiveState::kInactive;
  }

  if (!event_level_active) {
    return StoredSource::ActiveState::kReachedEventLevelAttributionLimit;
  }

  // We haven't enforced aggregatable attribution limit yet.
  return absl::nullopt;
}

void BindUint64OrNull(sql::Statement& statement,
                      int col,
                      absl::optional<uint64_t> value) {
  if (value.has_value()) {
    statement.BindInt64(col, SerializeUint64(*value));
  } else {
    statement.BindNull(col);
  }
}

void BindStringOrNull(sql::Statement& statement,
                      int col,
                      const absl::optional<std::string>& value) {
  if (value.has_value()) {
    statement.BindString(col, value.value());
  } else {
    statement.BindNull(col);
  }
}
absl::optional<std::string> ColumnStringOrNull(sql::Statement& statement,
                                               int col) {
  return statement.GetColumnType(col) == sql::ColumnType::kNull
             ? absl::nullopt
             : absl::make_optional(statement.ColumnString(col));
}

absl::optional<uint64_t> ColumnUint64OrNull(sql::Statement& statement,
                                            int col) {
  return statement.GetColumnType(col) == sql::ColumnType::kNull
             ? absl::nullopt
             : absl::make_optional(
                   DeserializeUint64(statement.ColumnInt64(col)));
}

struct StoredSourceData {
  StoredSource source;
  int num_conversions;
};

constexpr int kSourceColumnCount = 19;

// Helper to deserialize source rows. See `GetActiveSources()` for the
// expected ordering of columns used for the input to this function.
absl::optional<StoredSourceData> ReadSourceFromStatement(
    sql::Statement& statement) {
  DCHECK_GE(statement.ColumnCount(), kSourceColumnCount);

  int col = 0;

  StoredSource::Id source_id(statement.ColumnInt64(col++));
  uint64_t source_event_id = DeserializeUint64(statement.ColumnInt64(col++));
  absl::optional<SuitableOrigin> source_origin =
      SuitableOrigin::Deserialize(statement.ColumnString(col++));
  absl::optional<SuitableOrigin> destination_origin =
      SuitableOrigin::Deserialize(statement.ColumnString(col++));
  absl::optional<SuitableOrigin> reporting_origin =
      SuitableOrigin::Deserialize(statement.ColumnString(col++));
  base::Time source_time = statement.ColumnTime(col++);
  base::Time expiry_time = statement.ColumnTime(col++);
  base::Time event_report_window_time = statement.ColumnTime(col++);
  base::Time aggregatable_report_window_time = statement.ColumnTime(col++);
  absl::optional<AttributionSourceType> source_type =
      DeserializeSourceType(statement.ColumnInt(col++));
  absl::optional<StoredSource::AttributionLogic> attribution_logic =
      DeserializeAttributionLogic(statement.ColumnInt(col++));
  int64_t priority = statement.ColumnInt64(col++);
  absl::optional<uint64_t> debug_key = ColumnUint64OrNull(statement, col++);
  int num_conversions = statement.ColumnInt(col++);
  int64_t aggregatable_budget_consumed = statement.ColumnInt64(col++);
  absl::optional<attribution_reporting::AggregationKeys> aggregation_keys =
      DeserializeAggregationKeys(statement, col++);

  // TODO: Enforce remaining expiry/report_window/time invariants from
  // CommonSource.
  if (!source_origin || !destination_origin || !reporting_origin ||
      !source_type.has_value() || !attribution_logic.has_value() ||
      num_conversions < 0 || aggregatable_budget_consumed < 0 ||
      !aggregation_keys.has_value()) {
    return absl::nullopt;
  }

  absl::optional<attribution_reporting::FilterData> filter_data =
      DeserializeFilterData(statement, col++);
  if (!filter_data) {
    return absl::nullopt;
  }

  bool event_level_active = statement.ColumnBool(col++);
  bool aggregatable_active = statement.ColumnBool(col++);
  absl::optional<StoredSource::ActiveState> active_state =
      GetSourceActiveState(event_level_active, aggregatable_active);
  if (!active_state.has_value()) {
    return absl::nullopt;
  }

  return StoredSourceData{
      .source = StoredSource(
          CommonSourceInfo(
              source_event_id, std::move(*source_origin),
              std::move(*destination_origin), std::move(*reporting_origin),
              source_time,
              /*expiry_time=*/expiry_time,
              /*event_report_window_time=*/event_report_window_time,
              /*aggregatable_report_window_time=*/
              aggregatable_report_window_time, *source_type, priority,
              std::move(*filter_data), debug_key, std::move(*aggregation_keys)),
          *attribution_logic, *active_state, source_id,
          aggregatable_budget_consumed),
      .num_conversions = num_conversions};
}

absl::optional<StoredSourceData> ReadSourceToAttribute(
    sql::Database* db,
    StoredSource::Id source_id) {
  sql::Statement statement(db->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kReadSourceToAttributeSql));
  statement.BindInt64(0, *source_id);
  if (!statement.Step()) {
    return absl::nullopt;
  }

  return ReadSourceFromStatement(statement);
}

base::FilePath DatabasePath(const base::FilePath& user_data_directory) {
  return user_data_directory.Append(kDatabasePath);
}

}  // namespace

// static
bool AttributionStorageSql::DeleteStorageForTesting(
    const base::FilePath& user_data_directory) {
  return sql::Database::Delete(DatabasePath(user_data_directory));
}

AttributionStorageSql::AttributionStorageSql(
    const base::FilePath& user_data_directory,
    std::unique_ptr<AttributionStorageDelegate> delegate)
    : path_to_database_(user_data_directory.empty()
                            ? base::FilePath()
                            : DatabasePath(user_data_directory)),
      delegate_(std::move(delegate)),
      rate_limit_table_(delegate_.get()) {
  DCHECK(delegate_);
}

AttributionStorageSql::~AttributionStorageSql() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool AttributionStorageSql::DeactivateSources(
    const std::vector<StoredSource::Id>& sources) {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return false;
  }

  static constexpr char kDeactivateSourcesSql[] =
      "UPDATE sources "
      "SET event_level_active=0,aggregatable_active=0 "
      "WHERE source_id=?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeactivateSourcesSql));

  for (StoredSource::Id id : sources) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, *id);
    if (!statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

AttributionStorage::StoreSourceResult AttributionStorageSql::StoreSource(
    const StorableSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Force the creation of the database if it doesn't exist, as we need to
  // persist the source.
  if (!LazyInit(DbCreationPolicy::kCreateIfAbsent)) {
    return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  // Only delete expired impressions periodically to avoid excessive DB
  // operations.
  const base::TimeDelta delete_frequency =
      delegate_->GetDeleteExpiredSourcesFrequency();
  DCHECK_GE(delete_frequency, base::TimeDelta());
  const base::Time now = base::Time::Now();
  if (now - last_deleted_expired_sources_ >= delete_frequency) {
    if (!DeleteExpiredSources()) {
      return StoreSourceResult(StorableSource::Result::kInternalError);
    }
    last_deleted_expired_sources_ = now;
  }

  const CommonSourceInfo& common_info = source.common_info();

  const std::string serialized_source_origin =
      common_info.source_origin().Serialize();
  if (!HasCapacityForStoringSource(serialized_source_origin)) {
    return StoreSourceResult(
        StorableSource::Result::kInsufficientSourceCapacity,
        /*min_fake_report_time=*/absl::nullopt,
        /*max_destinations_per_source_site_reporting_origin=*/absl::nullopt,
        delegate_->GetMaxSourcesPerOrigin());
  }

  switch (
      rate_limit_table_.SourceAllowedForDestinationLimit(db_.get(), source)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      return StoreSourceResult(
          StorableSource::Result::kInsufficientUniqueDestinationCapacity,
          /*min_fake_report_time=*/absl::nullopt,
          delegate_->GetMaxDestinationsPerSourceSiteReportingOrigin());
    case RateLimitResult::kError:
      return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  switch (rate_limit_table_.SourceAllowedForReportingOriginLimit(db_.get(),
                                                                 source)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      return StoreSourceResult(
          StorableSource::Result::kExcessiveReportingOrigins);
    case RateLimitResult::kError:
      return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  AttributionStorageDelegate::RandomizedResponse randomized_response =
      delegate_->GetRandomizedResponse(common_info);

  int num_conversions = 0;
  auto attribution_logic = StoredSource::AttributionLogic::kTruthfully;
  bool event_level_active = true;
  if (randomized_response.has_value()) {
    num_conversions = randomized_response->size();
    attribution_logic = num_conversions == 0
                            ? StoredSource::AttributionLogic::kNever
                            : StoredSource::AttributionLogic::kFalsely;
    event_level_active = num_conversions == 0;
  }
  // Aggregatable reports are not subject to `attribution_logic`.
  const bool aggregatable_active = true;

  static constexpr char kInsertImpressionSql[] =
      "INSERT INTO sources"
      "(source_event_id,source_origin,destination_origin,"
      "destination_site,reporting_origin,source_time,"
      "expiry_time,event_report_window_time,aggregatable_report_window_time,"
      "source_type,attribution_logic,priority,source_site,"
      "num_attributions,event_level_active,aggregatable_active,debug_key,"
      "aggregatable_budget_consumed,aggregatable_source,filter_data)"
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,0,?,?)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertImpressionSql));
  statement.BindInt64(0, SerializeUint64(delegate_->SanitizeSourceEventId(
                             common_info.source_event_id())));
  statement.BindString(1, serialized_source_origin);
  statement.BindString(2, common_info.destination_origin().Serialize());
  statement.BindString(3, common_info.DestinationSite().Serialize());
  statement.BindString(4, common_info.reporting_origin().Serialize());
  statement.BindTime(5, common_info.source_time());
  statement.BindTime(6, common_info.expiry_time());
  statement.BindTime(7, common_info.event_report_window_time());
  statement.BindTime(8, common_info.aggregatable_report_window_time());
  statement.BindInt(9, SerializeSourceType(common_info.source_type()));
  statement.BindInt(10, SerializeAttributionLogic(attribution_logic));
  statement.BindInt64(11, common_info.priority());
  statement.BindString(12, common_info.SourceSite().Serialize());
  statement.BindInt(13, num_conversions);
  statement.BindBool(14, event_level_active);
  statement.BindBool(15, aggregatable_active);

  BindUint64OrNull(statement, 16, common_info.debug_key());

  absl::optional<StoredSource::ActiveState> active_state =
      GetSourceActiveState(event_level_active, aggregatable_active);
  DCHECK(active_state.has_value());

  statement.BindBlob(17,
                     SerializeAggregationKeys(common_info.aggregation_keys()));
  statement.BindBlob(18, SerializeFilterData(common_info.filter_data()));

  if (!statement.Run()) {
    return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  const StoredSource::Id source_id(db_->GetLastInsertRowId());
  const StoredSource stored_source(source.common_info(), attribution_logic,
                                   *active_state, source_id,
                                   /*aggregatable_budget_consumed=*/0);

  if (!rate_limit_table_.AddRateLimitForSource(db_.get(), stored_source)) {
    return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  absl::optional<base::Time> min_fake_report_time;

  if (attribution_logic == StoredSource::AttributionLogic::kFalsely) {
    for (const auto& fake_report : *randomized_response) {
      DCHECK_EQ(fake_report.trigger_data,
                delegate_->SanitizeTriggerData(fake_report.trigger_data,
                                               common_info.source_type()));

      DCHECK_LT(common_info.source_time(), fake_report.trigger_time);
      DCHECK_LT(fake_report.trigger_time, fake_report.report_time);

      if (!StoreEventLevelReport(source_id, fake_report.trigger_data,
                                 fake_report.trigger_time,
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

  if (attribution_logic != StoredSource::AttributionLogic::kTruthfully) {
    if (!rate_limit_table_.AddRateLimitForAttribution(
            db_.get(), AttributionInfo(std::move(stored_source),
                                       /*time=*/common_info.source_time(),
                                       /*debug_key=*/absl::nullopt))) {
      return StoreSourceResult(StorableSource::Result::kInternalError);
    }
  }

  if (!transaction.Commit()) {
    return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  return StoreSourceResult(
      attribution_logic == StoredSource::AttributionLogic::kTruthfully
          ? StorableSource::Result::kSuccess
          : StorableSource::Result::kSuccessNoised,
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
  // of reports per source_id. Selects the report with lowest priority,
  // and uses the greatest trigger_time to break ties. This favors sending
  // reports for report closer to the source time.
  sql::Statement min_priority_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kMinPrioritySql));
  min_priority_statement.BindInt64(0, *source.source_id());
  min_priority_statement.BindTime(1, report.report_time());

  absl::optional<AttributionReport::EventLevelData::Id>
      conversion_id_with_min_priority;
  int64_t min_priority;
  base::Time max_trigger_time;

  while (min_priority_statement.Step()) {
    int64_t priority = min_priority_statement.ColumnInt64(0);
    base::Time trigger_time = min_priority_statement.ColumnTime(1);

    if (!conversion_id_with_min_priority.has_value() ||
        priority < min_priority ||
        (priority == min_priority && trigger_time > max_trigger_time)) {
      conversion_id_with_min_priority.emplace(
          min_priority_statement.ColumnInt64(2));
      min_priority = priority;
      max_trigger_time = trigger_time;
    }
  }

  if (!min_priority_statement.Succeeded()) {
    return MaybeReplaceLowerPriorityEventLevelReportResult::kError;
  }

  // Deactivate the source at event-level as a new report will never be
  // generated in the future.
  if (!conversion_id_with_min_priority.has_value()) {
    static constexpr char kDeactivateSql[] =
        "UPDATE sources SET event_level_active=0 WHERE source_id=?";
    sql::Statement deactivate_statement(
        db_->GetCachedStatement(SQL_FROM_HERE, kDeactivateSql));
    deactivate_statement.BindInt64(0, *source.source_id());
    return deactivate_statement.Run()
               ? MaybeReplaceLowerPriorityEventLevelReportResult::
                     kDropNewReportSourceDeactivated
               : MaybeReplaceLowerPriorityEventLevelReportResult::kError;
  }

  // If the new report's priority is less than all existing ones, or if its
  // priority is equal to the minimum existing one and it is more recent, drop
  // it. We could explicitly check the trigger time here, but it would only
  // be relevant in the case of an ill-behaved clock, in which case the rest of
  // the attribution functionality would probably also break.
  if (conversion_priority <= min_priority) {
    return MaybeReplaceLowerPriorityEventLevelReportResult::kDropNewReport;
  }

  absl::optional<AttributionReport> replaced =
      GetReport(*conversion_id_with_min_priority);
  if (!replaced.has_value()) {
    return MaybeReplaceLowerPriorityEventLevelReportResult::kError;
  }

  // Otherwise, delete the existing report with the lowest priority.
  if (!DeleteReportInternal(*conversion_id_with_min_priority)) {
    return MaybeReplaceLowerPriorityEventLevelReportResult::kError;
  }

  replaced_report = std::move(replaced);
  return MaybeReplaceLowerPriorityEventLevelReportResult::kReplaceOldReport;
}

namespace {

bool IsSuccessResult(absl::optional<EventLevelResult> result) {
  return result == EventLevelResult::kSuccess ||
         result == EventLevelResult::kSuccessDroppedLowerPriority;
}

bool IsSuccessResult(absl::optional<AggregatableResult> result) {
  return result == AggregatableResult::kSuccess;
}

}  // namespace

CreateReportResult AttributionStorageSql::MaybeCreateAndStoreReport(
    const AttributionTrigger& trigger) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Time trigger_time = base::Time::Now();

  // Declarations for all of the various pieces of information which may be
  // collected and/or returned as a result of computing new reports in order to
  // produce a `CreateReportResult`.
  absl::optional<EventLevelResult> event_level_status;
  absl::optional<AttributionReport> new_event_level_report;

  absl::optional<AggregatableResult> aggregatable_status;
  absl::optional<AttributionReport> new_aggregatable_report;

  absl::optional<AttributionReport> replaced_event_level_report;
  absl::optional<AttributionReport> dropped_event_level_report;

  absl::optional<AttributionInfo> attribution_info;

  CreateReportResult::Limits limits;

  auto assemble_report_result =
      [&](absl::optional<EventLevelResult> new_event_level_status,
          absl::optional<AggregatableResult> new_aggregatable_status) {
        event_level_status = event_level_status.has_value()
                                 ? event_level_status
                                 : new_event_level_status;
        DCHECK(event_level_status.has_value());

        if (!IsSuccessResult(*event_level_status)) {
          new_event_level_report = absl::nullopt;
          replaced_event_level_report = absl::nullopt;
        }

        aggregatable_status = aggregatable_status.has_value()
                                  ? aggregatable_status
                                  : new_aggregatable_status;
        DCHECK(aggregatable_status.has_value());

        if (!IsSuccessResult(*aggregatable_status)) {
          new_aggregatable_report = absl::nullopt;
        }

        return CreateReportResult(
            trigger_time, *event_level_status, *aggregatable_status,
            std::move(replaced_event_level_report),
            std::move(new_event_level_report),
            std::move(new_aggregatable_report),
            attribution_info
                ? absl::make_optional(std::move(attribution_info->source))
                : absl::nullopt,
            limits, std::move(dropped_event_level_report));
      };

  const attribution_reporting::TriggerRegistration& trigger_registration =
      trigger.registration();

  if (trigger_registration.event_triggers.vec().empty()) {
    event_level_status = EventLevelResult::kNotRegistered;
  }

  if (trigger_registration.aggregatable_trigger_data.vec().empty() &&
      trigger_registration.aggregatable_values.values().empty()) {
    aggregatable_status = AggregatableResult::kNotRegistered;
  }

  if (event_level_status.has_value() && aggregatable_status.has_value()) {
    return assemble_report_result(/*new_event_level_status=*/absl::nullopt,
                                  /*new_aggregaable_status=*/absl::nullopt);
  }

  // We don't bother creating the DB here if it doesn't exist, because it's not
  // possible for there to be a matching source if there's no DB.
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return assemble_report_result(EventLevelResult::kNoMatchingImpressions,
                                  AggregatableResult::kNoMatchingImpressions);
  }

  absl::optional<StoredSource::Id> source_id_to_attribute;
  std::vector<StoredSource::Id> source_ids_to_delete;
  std::vector<StoredSource::Id> source_ids_to_deactivate;
  if (!FindMatchingSourceForTrigger(
          trigger, trigger_time, source_id_to_attribute, source_ids_to_delete,
          source_ids_to_deactivate)) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }
  if (!source_id_to_attribute.has_value()) {
    return assemble_report_result(EventLevelResult::kNoMatchingImpressions,
                                  AggregatableResult::kNoMatchingImpressions);
  }

  absl::optional<StoredSourceData> source_to_attribute =
      ReadSourceToAttribute(db_.get(), *source_id_to_attribute);
  // This is only possible if there is a corrupt DB.
  if (!source_to_attribute.has_value()) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  const bool top_level_filters_match = AttributionFiltersMatch(
      source_to_attribute->source.common_info().filter_data(),
      source_to_attribute->source.common_info().source_type(),
      trigger_registration.filters, trigger_registration.not_filters);

  attribution_info.emplace(std::move(source_to_attribute->source), trigger_time,
                           trigger_registration.debug_key);

  if (!top_level_filters_match) {
    return assemble_report_result(
        EventLevelResult::kNoMatchingSourceFilterData,
        AggregatableResult::kNoMatchingSourceFilterData);
  }

  absl::optional<uint64_t> dedup_key;
  if (!event_level_status.has_value()) {
    if (EventLevelResult create_event_level_status =
            MaybeCreateEventLevelReport(
                *attribution_info, trigger, new_event_level_report, dedup_key,
                limits.max_event_level_reports_per_destination);
        create_event_level_status != EventLevelResult::kSuccess) {
      event_level_status = create_event_level_status;
    }
  }

  if (!aggregatable_status.has_value()) {
    if (AggregatableResult create_aggregatable_status =
            MaybeCreateAggregatableAttributionReport(
                *attribution_info, trigger, new_aggregatable_report,
                limits.max_aggregatable_reports_per_destination);
        create_aggregatable_status != AggregatableResult::kSuccess) {
      aggregatable_status = create_aggregatable_status;
    }
  }

  if (event_level_status.has_value() && aggregatable_status.has_value()) {
    return assemble_report_result(/*new_event_level_status=*/absl::nullopt,
                                  /*new_aggregaable_status=*/absl::nullopt);
  }

  switch (rate_limit_table_.AttributionAllowedForAttributionLimit(
      db_.get(), *attribution_info)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      limits.rate_limits_max_attributions =
          delegate_->GetRateLimits().max_attributions;
      return assemble_report_result(EventLevelResult::kExcessiveAttributions,
                                    AggregatableResult::kExcessiveAttributions);
    case RateLimitResult::kError:
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
  }

  switch (rate_limit_table_.AttributionAllowedForReportingOriginLimit(
      db_.get(), *attribution_info)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      limits.rate_limits_max_attribution_reporting_origins =
          delegate_->GetRateLimits().max_attribution_reporting_origins;
      return assemble_report_result(
          EventLevelResult::kExcessiveReportingOrigins,
          AggregatableResult::kExcessiveReportingOrigins);
    case RateLimitResult::kError:
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  absl::optional<EventLevelResult> store_event_level_status;
  if (!event_level_status.has_value()) {
    DCHECK(new_event_level_report.has_value());
    store_event_level_status = MaybeStoreEventLevelReport(
        *new_event_level_report, dedup_key,
        source_to_attribute->num_conversions, replaced_event_level_report,
        dropped_event_level_report);
  }

  absl::optional<AggregatableResult> store_aggregatable_status;
  if (!aggregatable_status.has_value()) {
    DCHECK(new_aggregatable_report.has_value());
    store_aggregatable_status = MaybeStoreAggregatableAttributionReport(
        *new_aggregatable_report,
        source_to_attribute->source.aggregatable_budget_consumed(),
        trigger_registration.aggregatable_dedup_key,
        limits.aggregatable_budget_per_source);
  }

  if (store_event_level_status == EventLevelResult::kInternalError ||
      store_aggregatable_status == AggregatableResult::kInternalError) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  // Early exit if done modifying the storage. Dropped reports still need to
  // clean sources.
  if (!IsSuccessResult(store_event_level_status) &&
      !IsSuccessResult(store_aggregatable_status) &&
      store_event_level_status != EventLevelResult::kDroppedForNoise) {
    if (!transaction.Commit()) {
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
    }

    return assemble_report_result(store_event_level_status,
                                  store_aggregatable_status);
  }

  // Delete all unattributed sources.
  if (!DeleteSources(source_ids_to_delete)) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  // Deactivate all attributed sources.
  if (!DeactivateSources(source_ids_to_deactivate)) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  // Based on the deletion logic here and the fact that we delete sources
  // with |num_attributions > 0| or |aggregatable_budget_consumed > 0| when
  // there is a new matching source in |StoreSource()|, we should be
  // guaranteed that these sources all have |num_conversions == 0| and
  // |aggregatable_budget_consumed == 0|, and that they never contributed to a
  // rate limit. Therefore, we don't need to call
  // |RateLimitTable::ClearDataForSourceIds()| here.

  // Reports which are dropped do not need to make any further changes.
  if (store_event_level_status == EventLevelResult::kDroppedForNoise &&
      !IsSuccessResult(store_aggregatable_status)) {
    if (!transaction.Commit()) {
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
    }

    return assemble_report_result(store_event_level_status,
                                  store_aggregatable_status);
  }

  if (!rate_limit_table_.AddRateLimitForAttribution(db_.get(),
                                                    *attribution_info)) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  if (!transaction.Commit()) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  return assemble_report_result(store_event_level_status,
                                store_aggregatable_status);
}

bool AttributionStorageSql::FindMatchingSourceForTrigger(
    const AttributionTrigger& trigger,
    base::Time trigger_time,
    absl::optional<StoredSource::Id>& source_id_to_attribute,
    std::vector<StoredSource::Id>& source_ids_to_delete,
    std::vector<StoredSource::Id>& source_ids_to_deactivate) {
  const SuitableOrigin& destination_origin = trigger.destination_origin();
  const SuitableOrigin& reporting_origin = trigger.reporting_origin();

  // Get all sources that match this <reporting_origin,
  // conversion_destination> pair. Only get sources that are active and not
  // past their expiry time. The sources are fetched in order so that the
  // first one is the one that will be attributed; the others will be deleted or
  // deactivated, depending on whether they have ever been attributed.
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetMatchingSourcesSql));
  statement.BindString(0, net::SchemefulSite(destination_origin).Serialize());
  statement.BindString(1, reporting_origin.Serialize());
  statement.BindTime(2, trigger_time);

  // If there are no matching sources, return early.
  if (!statement.Step()) {
    return statement.Succeeded();
  }

  // The first one returned will be attributed; it has the highest priority.
  source_id_to_attribute = StoredSource::Id(statement.ColumnInt64(0));

  // Any others will be deleted or deactivated.
  while (statement.Step()) {
    StoredSource::Id source_id(statement.ColumnInt64(0));
    int num_attributions = statement.ColumnInt(1);
    int64_t aggregatable_budget_consumed = statement.ColumnInt64(2);

    if (num_attributions > 0 || aggregatable_budget_consumed > 0) {
      source_ids_to_deactivate.push_back(source_id);
    } else {
      source_ids_to_delete.push_back(source_id);
    }
  }
  return statement.Succeeded();
}

EventLevelResult AttributionStorageSql::MaybeCreateEventLevelReport(
    const AttributionInfo& attribution_info,
    const AttributionTrigger& trigger,
    absl::optional<AttributionReport>& report,
    absl::optional<uint64_t>& dedup_key,
    absl::optional<int>& max_event_level_reports_per_destination) {
  if (attribution_info.source.attribution_logic() ==
      StoredSource::AttributionLogic::kFalsely) {
    DCHECK_EQ(attribution_info.source.active_state(),
              StoredSource::ActiveState::kReachedEventLevelAttributionLimit);
    return EventLevelResult::kFalselyAttributedSource;
  }

  const CommonSourceInfo& common_info = attribution_info.source.common_info();

  if (attribution_info.time > common_info.event_report_window_time()) {
    return EventLevelResult::kReportWindowPassed;
  }

  const AttributionSourceType source_type = common_info.source_type();

  auto event_trigger = base::ranges::find_if(
      trigger.registration().event_triggers.vec(),
      [&](const attribution_reporting::EventTriggerData& event_trigger) {
        return AttributionFiltersMatch(common_info.filter_data(), source_type,
                                       event_trigger.filters,
                                       event_trigger.not_filters);
      });

  if (event_trigger == trigger.registration().event_triggers.vec().end()) {
    return EventLevelResult::kNoMatchingConfigurations;
  }

  switch (ReportAlreadyStored(attribution_info.source.source_id(),
                              event_trigger->dedup_key,
                              AttributionReport::Type::kEventLevel)) {
    case ReportAlreadyStoredStatus::kNotStored:
      break;
    case ReportAlreadyStoredStatus::kStored:
      return EventLevelResult::kDeduplicated;
    case ReportAlreadyStoredStatus::kError:
      return EventLevelResult::kInternalError;
  }

  switch (
      CapacityForStoringReport(trigger, AttributionReport::Type::kEventLevel)) {
    case ConversionCapacityStatus::kHasCapacity:
      break;
    case ConversionCapacityStatus::kNoCapacity:
      max_event_level_reports_per_destination =
          delegate_->GetMaxReportsPerDestination(
              AttributionReport::Type::kEventLevel);
      return EventLevelResult::kNoCapacityForConversionDestination;
    case ConversionCapacityStatus::kError:
      return EventLevelResult::kInternalError;
  }

  const base::Time report_time =
      delegate_->GetEventLevelReportTime(common_info, attribution_info.time);

  // TODO(apaseltiner): When the real values returned by
  // `GetRandomizedResponseRate()` are changed for the first time, we must
  // remove the call to that function here and instead associate each newly
  // stored source and report with the current configuration. One way to do that
  // is to permanently store the configuration history in the binary with each
  // version having a unique ID, and storing that ID in a new column in the
  // sources and event_level_reports DB tables. This code would then look up the
  // values for the particular IDs. Because such an approach would entail
  // complicating the DB schema, we hardcode the values for now and will wait
  // for the first time the values are changed before complicating the codebase.
  const double randomized_response_rate =
      delegate_->GetRandomizedResponseRate(source_type);

  // TODO(apaseltiner): Consider informing the manager if the trigger
  // data was out of range for DevTools issue reporting.
  report = AttributionReport(
      attribution_info, report_time, delegate_->NewReportID(),
      /*failed_send_attempts=*/0,
      AttributionReport::EventLevelData(
          delegate_->SanitizeTriggerData(event_trigger->data, source_type),
          event_trigger->priority, randomized_response_rate,
          AttributionReport::EventLevelData::Id(kUnsetReportId)));

  dedup_key = event_trigger->dedup_key;

  return EventLevelResult::kSuccess;
}

EventLevelResult AttributionStorageSql::MaybeStoreEventLevelReport(
    AttributionReport& report,
    absl::optional<uint64_t> dedup_key,
    int num_conversions,
    absl::optional<AttributionReport>& replaced_report,
    absl::optional<AttributionReport>& dropped_report) {
  if (report.attribution_info().source.active_state() ==
      StoredSource::ActiveState::kReachedEventLevelAttributionLimit) {
    dropped_report = std::move(report);
    return EventLevelResult::kExcessiveReports;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return EventLevelResult::kInternalError;
  }

  auto* event_level_data =
      absl::get_if<AttributionReport::EventLevelData>(&report.data());
  DCHECK(event_level_data);
  const auto maybe_replace_lower_priority_report_result =
      MaybeReplaceLowerPriorityEventLevelReport(
          report, num_conversions, event_level_data->priority, replaced_report);
  if (maybe_replace_lower_priority_report_result ==
      MaybeReplaceLowerPriorityEventLevelReportResult::kError) {
    return EventLevelResult::kInternalError;
  }

  if (maybe_replace_lower_priority_report_result ==
          MaybeReplaceLowerPriorityEventLevelReportResult::kDropNewReport ||
      maybe_replace_lower_priority_report_result ==
          MaybeReplaceLowerPriorityEventLevelReportResult::
              kDropNewReportSourceDeactivated) {
    if (!transaction.Commit()) {
      return EventLevelResult::kInternalError;
    }

    dropped_report = std::move(report);

    return maybe_replace_lower_priority_report_result ==
                   MaybeReplaceLowerPriorityEventLevelReportResult::
                       kDropNewReport
               ? EventLevelResult::kPriorityTooLow
               : EventLevelResult::kExcessiveReports;
  }

  const AttributionInfo& attribution_info = report.attribution_info();

  // Reports with `AttributionLogic::kNever` should be included in all
  // attribution operations and matching, but only `kTruthfully` should generate
  // reports that get sent.
  const bool create_report = attribution_info.source.attribution_logic() ==
                             StoredSource::AttributionLogic::kTruthfully;

  if (create_report) {
    absl::optional<AttributionReport::EventLevelData::Id> id =
        StoreEventLevelReport(
            attribution_info.source.source_id(), event_level_data->trigger_data,
            attribution_info.time, report.report_time(),
            event_level_data->priority, report.external_report_id(),
            attribution_info.debug_key);
    if (!id) {
      return EventLevelResult::kInternalError;
    }

    event_level_data->id = *id;
  }

  // If a dedup key is present, store it. We do this regardless of whether
  // `create_report` is true to avoid leaking whether the report was actually
  // stored.
  if (dedup_key.has_value() &&
      !StoreDedupKey(attribution_info.source.source_id(), *dedup_key,
                     AttributionReport::Type::kEventLevel)) {
    return EventLevelResult::kInternalError;
  }

  // Only increment the number of conversions associated with the source if
  // we are adding a new one, rather than replacing a dropped one.
  if (maybe_replace_lower_priority_report_result ==
      MaybeReplaceLowerPriorityEventLevelReportResult::kAddNewReport) {
    static constexpr char kUpdateImpressionForConversionSql[] =
        "UPDATE sources SET num_attributions=num_attributions+1 "
        "WHERE source_id=?";
    sql::Statement impression_update_statement(db_->GetCachedStatement(
        SQL_FROM_HERE, kUpdateImpressionForConversionSql));

    // Update the attributed source.
    impression_update_statement.BindInt64(0,
                                          *attribution_info.source.source_id());
    if (!impression_update_statement.Run()) {
      return EventLevelResult::kInternalError;
    }
  }

  if (!transaction.Commit()) {
    return EventLevelResult::kInternalError;
  }

  if (!create_report) {
    return EventLevelResult::kDroppedForNoise;
  }

  return maybe_replace_lower_priority_report_result ==
                 MaybeReplaceLowerPriorityEventLevelReportResult::
                     kReplaceOldReport
             ? EventLevelResult::kSuccessDroppedLowerPriority
             : EventLevelResult::kSuccess;
}

absl::optional<AttributionReport::EventLevelData::Id>
AttributionStorageSql::StoreEventLevelReport(
    StoredSource::Id source_id,
    uint64_t trigger_data,
    base::Time trigger_time,
    base::Time report_time,
    int64_t priority,
    const base::GUID& external_report_id,
    absl::optional<uint64_t> trigger_debug_key) {
  DCHECK(external_report_id.is_valid());

  static constexpr char kStoreReportSql[] =
      "INSERT INTO event_level_reports"
      "(source_id,trigger_data,trigger_time,report_time,"
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
  if (!store_report_statement.Run()) {
    return absl::nullopt;
  }

  return AttributionReport::EventLevelData::Id(db_->GetLastInsertRowId());
}

// Helper to deserialize report rows. See `GetReport()` for the expected
// ordering of columns used for the input to this function.
absl::optional<AttributionReport>
AttributionStorageSql::ReadReportFromStatement(sql::Statement& statement) {
  DCHECK_EQ(statement.ColumnCount(), kSourceColumnCount + 8);

  absl::optional<StoredSourceData> source_data =
      ReadSourceFromStatement(statement);

  int col = kSourceColumnCount;
  uint64_t trigger_data = DeserializeUint64(statement.ColumnInt64(col++));
  base::Time trigger_time = statement.ColumnTime(col++);
  base::Time report_time = statement.ColumnTime(col++);
  AttributionReport::EventLevelData::Id report_id(statement.ColumnInt64(col++));
  int64_t conversion_priority = statement.ColumnInt64(col++);
  int failed_send_attempts = statement.ColumnInt(col++);
  base::GUID external_report_id =
      base::GUID::ParseLowercase(statement.ColumnString(col++));
  absl::optional<uint64_t> trigger_debug_key =
      ColumnUint64OrNull(statement, col++);

  // Ensure data is valid before continuing. This could happen if there is
  // database corruption.
  // TODO(apaseltiner): Should we raze the DB if we've detected corruption?
  if (failed_send_attempts < 0 || !external_report_id.is_valid() ||
      !source_data.has_value()) {
    return absl::nullopt;
  }

  double randomized_response_rate = delegate_->GetRandomizedResponseRate(
      source_data->source.common_info().source_type());

  return AttributionReport(
      AttributionInfo(std::move(source_data->source), trigger_time,
                      trigger_debug_key),
      report_time, std::move(external_report_id), failed_send_attempts,
      AttributionReport::EventLevelData(trigger_data, conversion_priority,
                                        randomized_response_rate, report_id));
}

std::vector<AttributionReport> AttributionStorageSql::GetAttributionReports(
    base::Time max_report_time,
    int limit,
    AttributionReport::Types report_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!report_types.Empty());

  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return {};
  }

  std::vector<AttributionReport> reports;

  for (AttributionReport::Type report_type : report_types) {
    switch (report_type) {
      case AttributionReport::Type::kEventLevel: {
        std::vector<AttributionReport> event_level_reports =
            GetEventLevelReportsInternal(max_report_time, limit);
        reports.insert(reports.end(),
                       std::make_move_iterator(event_level_reports.begin()),
                       std::make_move_iterator(event_level_reports.end()));
        break;
      }
      case AttributionReport::Type::kAggregatableAttribution: {
        std::vector<AttributionReport> aggregatable_reports =
            GetAggregatableAttributionReportsInternal(max_report_time, limit);
        reports.insert(reports.end(),
                       std::make_move_iterator(aggregatable_reports.begin()),
                       std::make_move_iterator(aggregatable_reports.end()));
        break;
      }
    }
  }

  if (limit >= 0 && reports.size() > static_cast<size_t>(limit)) {
    base::ranges::partial_sort(reports, reports.begin() + limit, /*comp=*/{},
                               &AttributionReport::report_time);
    reports.erase(reports.begin() + limit);
  }

  delegate_->ShuffleReports(reports);
  return reports;
}

std::vector<AttributionReport>
AttributionStorageSql::GetEventLevelReportsInternal(base::Time max_report_time,
                                                    int limit) {
  // Get at most |limit| entries in the event_level_reports table with a
  // |report_time| no greater than |max_report_time| and their matching
  // information from the impression table. Negatives are treated as no limit
  // (https://sqlite.org/lang_select.html#limitoffset).
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetEventLevelReportsSql));
  statement.BindTime(0, max_report_time);
  statement.BindInt(1, limit);

  std::vector<AttributionReport> reports;
  while (statement.Step()) {
    absl::optional<AttributionReport> report =
        ReadReportFromStatement(statement);
    if (report.has_value()) {
      reports.push_back(std::move(*report));
    }
  }

  if (!statement.Succeeded()) {
    return {};
  }

  return reports;
}

absl::optional<base::Time> AttributionStorageSql::GetNextReportTime(
    base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return absl::nullopt;
  }

  absl::optional<base::Time> next_event_level_report_time =
      GetNextEventLevelReportTime(time);
  absl::optional<base::Time> next_aggregatable_report_time =
      GetNextAggregatableAttributionReportTime(time);

  return AttributionReport::MinReportTime(next_event_level_report_time,
                                          next_aggregatable_report_time);
}

absl::optional<base::Time> AttributionStorageSql::GetNextReportTime(
    sql::StatementID id,
    const char* sql,
    base::Time time) {
  sql::Statement statement(db_->GetCachedStatement(id, sql));
  statement.BindTime(0, time);

  if (statement.Step() &&
      statement.GetColumnType(0) != sql::ColumnType::kNull) {
    return statement.ColumnTime(0);
  }

  return absl::nullopt;
}

absl::optional<base::Time> AttributionStorageSql::GetNextEventLevelReportTime(
    base::Time time) {
  return GetNextReportTime(
      SQL_FROM_HERE, attribution_queries::kNextEventLevelReportTimeSql, time);
}

std::vector<AttributionReport> AttributionStorageSql::GetReports(
    const std::vector<AttributionReport::Id>& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return {};
  }

  std::vector<AttributionReport> reports;
  for (AttributionReport::Id id : ids) {
    absl::optional<AttributionReport> report = absl::visit(
        [&](auto id) {
          DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
          return GetReport(id);
        },
        id);

    if (report.has_value()) {
      reports.push_back(std::move(*report));
    }
  }
  return reports;
}

absl::optional<AttributionReport> AttributionStorageSql::GetReport(
    AttributionReport::EventLevelData::Id conversion_id) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetEventLevelReportSql));
  statement.BindInt64(0, *conversion_id);

  if (!statement.Step()) {
    return absl::nullopt;
  }

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
      if (!statement.Succeeded()) {
        return false;
      }
      if (source_ids.empty()) {
        return true;
      }
      if (!DeleteSources(source_ids)) {
        return false;
      }
      // Deliberately retain the existing bound vars so that the limit, etc are
      // the same.
      statement.Reset(/*clear_bound_vars=*/false);
    }
  };

  // Delete all sources that have no associated reports and are past
  // their expiry time. Optimized by |kImpressionExpiryIndexSql|.
  sql::Statement select_expired_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kSelectExpiredSourcesSql));
  select_expired_statement.BindTime(0, base::Time::Now());
  select_expired_statement.BindInt(1, kMaxDeletesPerBatch);
  if (!delete_sources_from_paged_select(select_expired_statement)) {
    return false;
  }

  // Delete all sources that have no associated reports and are
  // inactive. This is done in a separate statement from
  // |kSelectExpiredSourcesSql| so that each query is optimized by an index.
  // Optimized by |kConversionDestinationIndexSql|.
  sql::Statement select_inactive_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kSelectInactiveSourcesSql));
  select_inactive_statement.BindInt(0, kMaxDeletesPerBatch);
  return delete_sources_from_paged_select(select_inactive_statement);
}

bool AttributionStorageSql::DeleteReport(AttributionReport::Id report_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return true;
  }

  return absl::visit(
      [&](auto id) {
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
        return DeleteReportInternal(id);
      },
      report_id);
}

bool AttributionStorageSql::DeleteReportInternal(
    AttributionReport::EventLevelData::Id report_id) {
  static constexpr char kDeleteReportSql[] =
      "DELETE FROM event_level_reports WHERE report_id=?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteReportSql));
  statement.BindInt64(0, *report_id);
  return statement.Run();
}

bool AttributionStorageSql::UpdateReportForSendFailure(
    AttributionReport::Id report_id,
    base::Time new_report_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return false;
  }

  auto [statement_id, sql_query, report_id_int] = absl::visit(
      base::Overloaded{
          [](AttributionReport::EventLevelData::Id id) {
            return std::make_tuple(
                SQL_FROM_HERE,
                attribution_queries::kUpdateFailedEventLevelReportSql, *id);
          },
          [](AttributionReport::AggregatableAttributionData::Id id) {
            return std::make_tuple(
                SQL_FROM_HERE,
                attribution_queries::kUpdateFailedAggregatableReportSql, *id);
          },
      },
      report_id);

  sql::Statement statement(db_->GetCachedStatement(statement_id, sql_query));
  statement.BindTime(0, new_report_time);
  statement.BindInt64(1, report_id_int);
  return statement.Run() && db_->GetLastChangeCount() == 1;
}

absl::optional<base::Time> AttributionStorageSql::AdjustOfflineReportTimes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto delay = delegate_->GetOfflineReportDelayConfig();

  // If no delay is being applied (i.e. debug mode is active), return the
  // earliest report time nonetheless so that it is scheduled properly.
  if (!delay.has_value()) {
    return GetNextReportTime(base::Time::Min());
  }

  DCHECK_GE(delay->min, base::TimeDelta());
  DCHECK_GE(delay->max, base::TimeDelta());
  DCHECK_LE(delay->min, delay->max);

  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return absl::nullopt;
  }

  base::Time now = base::Time::Now();

  absl::optional<base::Time> next_event_level_report_time =
      AdjustOfflineEventLevelReportTimes(delay->min, delay->max, now);
  absl::optional<base::Time> next_aggregatable_report_time =
      AdjustOfflineAggregatableAttributionReportTimes(delay->min, delay->max,
                                                      now);
  return AttributionReport::MinReportTime(next_event_level_report_time,
                                          next_aggregatable_report_time);
}

bool AttributionStorageSql::AdjustOfflineReportTimes(sql::StatementID id,
                                                     const char* sql,
                                                     base::TimeDelta min_delay,
                                                     base::TimeDelta max_delay,
                                                     base::Time now) {
  sql::Statement statement(db_->GetCachedStatement(id, sql));
  statement.BindTime(0, now + min_delay);
  statement.BindTimeDelta(1, max_delay - min_delay + base::Microseconds(1));
  statement.BindTime(2, now);
  return statement.Run();
}

absl::optional<base::Time>
AttributionStorageSql::AdjustOfflineEventLevelReportTimes(
    base::TimeDelta min_delay,
    base::TimeDelta max_delay,
    base::Time now) {
  if (!AdjustOfflineReportTimes(
          SQL_FROM_HERE, attribution_queries::kSetEventLevelReportTimeSql,
          min_delay, max_delay, now)) {
    return absl::nullopt;
  }

  return GetNextEventLevelReportTime(base::Time::Min());
}

void AttributionStorageSql::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    bool delete_rate_limit_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return;
  }

  SCOPED_UMA_HISTOGRAM_TIMER("Conversions.ClearDataTime");
  if (filter.is_null() && (delete_begin.is_null() || delete_begin.is_min()) &&
      delete_end.is_max()) {
    ClearAllDataAllTime(delete_rate_limit_data);
    return;
  }

  // Measure the time it takes to perform a clear with a filter separately from
  // the above histogram.
  SCOPED_UMA_HISTOGRAM_TIMER("Conversions.Storage.ClearDataWithFilterDuration");

  // Delete the data in a transaction to avoid cases where the source part
  // of a report is deleted without deleting the associated report, or
  // vice versa.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return;
  }

  // TODO(csharrison, johnidel): This query can be split up and optimized by
  // adding indexes on the time and trigger_time columns.
  // See this comment for more information:
  // crrev.com/c/2150071/4/content/browser/conversions/conversion_storage_sql.cc#342
  //
  // TODO(crbug.com/1290377): Look into optimizing origin filter callback.
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kScanCandidateData));
  statement.BindTime(0, delete_begin);
  statement.BindTime(1, delete_end);

  // TODO(apaseltiner): Consider wrapping `filter` such that it deletes
  // opaque/untrustworthy origins.

  std::vector<StoredSource::Id> source_ids_to_delete;
  int num_event_reports_deleted = 0;
  while (statement.Step()) {
    if (filter.is_null() || filter.Run(blink::StorageKey(DeserializeOrigin(
                                statement.ColumnString(0))))) {
      source_ids_to_delete.emplace_back(statement.ColumnInt64(1));
      if (statement.GetColumnType(2) != sql::ColumnType::kNull) {
        if (!DeleteReportInternal(AttributionReport::EventLevelData::Id(
                statement.ColumnInt64(2)))) {
          return;
        }

        ++num_event_reports_deleted;
      }
    }
  }

  // TODO(csharrison, johnidel): Should we consider poisoning the DB if some of
  // the delete operations fail?
  if (!statement.Succeeded()) {
    return;
  }

  int aggregatable_maybe_deleted =
      ClearAggregatableAttributionsForOriginsInRange(
          delete_begin, delete_end, filter, source_ids_to_delete);

  if (aggregatable_maybe_deleted < 0) {
    return;
  }
  int num_aggregatable_reports_deleted = aggregatable_maybe_deleted;

  // Since multiple reports can be associated with a single source,
  // deduplicate source IDs using a set to avoid redundant DB operations
  // below.
  source_ids_to_delete =
      base::flat_set<StoredSource::Id>(std::move(source_ids_to_delete))
          .extract();

  if (!DeleteSources(source_ids_to_delete)) {
    return;
  }

  // Careful! At this point we can still have some vestigial entries in the DB.
  // For example, if a source has two reports, and one report is
  // deleted, the above logic will delete the source as well, leaving the
  // second report in limbo (it was not in the deletion time range).
  // Delete all unattributed reports here to ensure everything is cleaned
  // up.
  sql::Statement delete_vestigial_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kDeleteVestigialConversionSql));
  for (StoredSource::Id source_id : source_ids_to_delete) {
    delete_vestigial_statement.Reset(/*clear_bound_vars=*/true);
    delete_vestigial_statement.BindInt64(0, *source_id);
    if (!delete_vestigial_statement.Run()) {
      return;
    }

    num_event_reports_deleted += db_->GetLastChangeCount();
  }

  // Careful! At this point we can still have some vestigial entries in the DB.
  // See comments above for event-level reports.
  aggregatable_maybe_deleted =
      ClearAggregatableAttributionsForSourceIds(source_ids_to_delete);

  if (aggregatable_maybe_deleted < 0) {
    return;
  }
  num_aggregatable_reports_deleted += aggregatable_maybe_deleted;

  if (delete_rate_limit_data && !rate_limit_table_.ClearDataForSourceIds(
                                    db_.get(), source_ids_to_delete)) {
    return;
  }

  if (delete_rate_limit_data &&
      !rate_limit_table_.ClearDataForOriginsInRange(db_.get(), delete_begin,
                                                    delete_end, filter)) {
    return;
  }

  if (!transaction.Commit()) {
    return;
  }

  RecordSourcesDeleted(static_cast<int>(source_ids_to_delete.size()));
  RecordReportsDeleted(num_event_reports_deleted,
                       num_aggregatable_reports_deleted);
}

void AttributionStorageSql::ClearAllDataAllTime(bool delete_rate_limit_data) {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return;
  }

  static constexpr char kDeleteAllReportsSql[] =
      "DELETE FROM event_level_reports";
  sql::Statement delete_all_reports_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllReportsSql));
  if (!delete_all_reports_statement.Run()) {
    return;
  }
  int num_event_reports_deleted = db_->GetLastChangeCount();

  static constexpr char kDeleteAllSourcesSql[] = "DELETE FROM sources";
  sql::Statement delete_all_sources_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllSourcesSql));
  if (!delete_all_sources_statement.Run()) {
    return;
  }
  int num_sources_deleted = db_->GetLastChangeCount();

  static constexpr char kDeleteAllDedupKeysSql[] = "DELETE FROM dedup_keys";
  sql::Statement delete_all_dedup_keys_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllDedupKeysSql));
  if (!delete_all_dedup_keys_statement.Run()) {
    return;
  }

  static constexpr char kDeleteAllAggregationsSql[] =
      "DELETE FROM aggregatable_report_metadata";
  sql::Statement delete_all_aggregations_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllAggregationsSql));
  if (!delete_all_aggregations_statement.Run()) {
    return;
  }

  static constexpr char kDeleteAllContributionsSql[] =
      "DELETE FROM aggregatable_contributions";
  sql::Statement delete_all_contributions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAllContributionsSql));
  if (!delete_all_contributions_statement.Run()) {
    return;
  }
  int num_aggregatable_reports_deleted = db_->GetLastChangeCount();

  if (delete_rate_limit_data &&
      !rate_limit_table_.ClearAllDataAllTime(db_.get())) {
    return;
  }

  if (!transaction.Commit()) {
    return;
  }

  RecordSourcesDeleted(num_sources_deleted);
  RecordReportsDeleted(num_event_reports_deleted,
                       num_aggregatable_reports_deleted);
}

bool AttributionStorageSql::HasCapacityForStoringSource(
    const std::string& serialized_origin) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kCountSourcesSql));
  statement.BindString(0, serialized_origin);
  if (!statement.Step()) {
    return false;
  }
  int64_t count = statement.ColumnInt64(0);
  return count < delegate_->GetMaxSourcesPerOrigin();
}

AttributionStorageSql::ReportAlreadyStoredStatus
AttributionStorageSql::ReportAlreadyStored(
    StoredSource::Id source_id,
    absl::optional<uint64_t> dedup_key,
    AttributionReport::Type report_type) {
  if (!dedup_key.has_value()) {
    return ReportAlreadyStoredStatus::kNotStored;
  }

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kCountReportsSql));
  statement.BindInt64(0, *source_id);
  statement.BindInt(1, SerializeReportType(report_type));
  statement.BindInt64(2, SerializeUint64(*dedup_key));

  // If there's an error, return true so `MaybeCreateAndStoreReport()`
  // returns early.
  if (!statement.Step()) {
    return ReportAlreadyStoredStatus::kError;
  }

  int64_t count = statement.ColumnInt64(0);
  return count > 0 ? ReportAlreadyStoredStatus::kStored
                   : ReportAlreadyStoredStatus::kNotStored;
}

AttributionStorageSql::ConversionCapacityStatus
AttributionStorageSql::CapacityForStoringReport(
    const AttributionTrigger& trigger,
    AttributionReport::Type report_type) {
  sql::Statement statement;
  switch (report_type) {
    case AttributionReport::Type::kEventLevel:
      statement.Assign(db_->GetCachedStatement(
          SQL_FROM_HERE, attribution_queries::kCountEventLevelReportsSql));
      break;
    case AttributionReport::Type::kAggregatableAttribution:
      statement.Assign(db_->GetCachedStatement(
          SQL_FROM_HERE, attribution_queries::kCountAggregatableReportsSql));
      break;
  }

  statement.BindString(
      0, net::SchemefulSite(trigger.destination_origin()).Serialize());
  if (!statement.Step()) {
    return ConversionCapacityStatus::kError;
  }
  int64_t count = statement.ColumnInt64(0);
  int max = delegate_->GetMaxReportsPerDestination(report_type);
  DCHECK_GT(max, 0);
  return count < max ? ConversionCapacityStatus::kHasCapacity
                     : ConversionCapacityStatus::kNoCapacity;
}

std::vector<StoredSource> AttributionStorageSql::GetActiveSources(int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return {};
  }

  // Negatives are treated as no limit
  // (https://sqlite.org/lang_select.html#limitoffset).

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetActiveSourcesSql));
  statement.BindTime(0, base::Time::Now());
  statement.BindInt(1, limit);

  std::vector<StoredSource> sources;
  while (statement.Step()) {
    absl::optional<StoredSourceData> source_data =
        ReadSourceFromStatement(statement);
    if (source_data.has_value()) {
      sources.push_back(std::move(source_data->source));
    }
  }
  if (!statement.Succeeded()) {
    return {};
  }

  for (auto& source : sources) {
    absl::optional<std::vector<uint64_t>> dedup_keys =
        ReadDedupKeys(source.source_id(), AttributionReport::Type::kEventLevel);
    if (!dedup_keys.has_value()) {
      return {};
    }
    source.SetDedupKeys(std::move(*dedup_keys));

    absl::optional<std::vector<uint64_t>> aggregatable_dedup_keys =
        ReadDedupKeys(source.source_id(),
                      AttributionReport::Type::kAggregatableAttribution);
    if (!aggregatable_dedup_keys.has_value()) {
      return {};
    }
    source.SetAggregatableDedupKeys(std::move(*aggregatable_dedup_keys));
  }

  return sources;
}

absl::optional<std::vector<uint64_t>> AttributionStorageSql::ReadDedupKeys(
    StoredSource::Id source_id,
    AttributionReport::Type report_type) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kDedupKeySql));
  statement.BindInt64(0, *source_id);
  statement.BindInt(1, SerializeReportType(report_type));

  std::vector<uint64_t> dedup_keys;
  while (statement.Step()) {
    dedup_keys.push_back(DeserializeUint64(statement.ColumnInt64(0)));
  }
  if (!statement.Succeeded()) {
    return absl ::nullopt;
  }

  return dedup_keys;
}

bool AttributionStorageSql::StoreDedupKey(StoredSource::Id source_id,
                                          uint64_t dedup_key,
                                          AttributionReport::Type report_type) {
  static constexpr char kInsertDedupKeySql[] =
      "INSERT INTO dedup_keys(source_id,report_type,dedup_key)VALUES(?,?,?)";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertDedupKeySql));
  statement.BindInt64(0, *source_id);
  statement.BindInt(1, SerializeReportType(report_type));
  statement.BindInt64(2, SerializeUint64(dedup_key));
  return statement.Run();
}

void AttributionStorageSql::HandleInitializationFailure(
    const InitStatus status) {
  RecordInitializationStatus(status);
  db_.reset();
  db_init_status_ = DbStatus::kClosed;
}

bool AttributionStorageSql::LazyInit(DbCreationPolicy creation_policy) {
  if (!db_init_status_) {
    if (path_to_database_.empty()) {
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
      if (creation_policy == DbCreationPolicy::kIgnoreIfAbsent) {
        return false;
      }
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

  // `base::Unretained()` is safe because the callback will only be called
  // while the `sql::Database` in `db_` is alive, and this instance owns `db_`.
  db_->set_error_callback(base::BindRepeating(
      &AttributionStorageSql::DatabaseErrorCallback, base::Unretained(this)));

  if (path_to_database_.empty()) {
    if (!db_->OpenInMemory()) {
      HandleInitializationFailure(InitStatus::kFailedToOpenDbInMemory);
      return false;
    }
  } else {
    const base::FilePath& dir = path_to_database_.DirName();
    const bool dir_exists_or_was_created = base::CreateDirectory(dir);
    if (!dir_exists_or_was_created) {
      DLOG(ERROR) << "Failed to create directory for Conversion database";
      HandleInitializationFailure(InitStatus::kFailedToCreateDir);
      return false;
    }
    if (!db_->Open(path_to_database_)) {
      DLOG(ERROR) << "Failed to open Conversion database";
      HandleInitializationFailure(InitStatus::kFailedToOpenDbFile);
      return false;
    }
  }

  if (!InitializeSchema(db_init_status_ == DbStatus::kDeferringCreation)) {
    DLOG(ERROR) << "Failed to initialize schema for Conversion database";
    HandleInitializationFailure(InitStatus::kFailedToInitializeSchema);
    return false;
  }

  db_init_status_ = DbStatus::kOpen;
  RecordInitializationStatus(InitStatus::kSuccess);
  return true;
}

bool AttributionStorageSql::InitializeSchema(bool db_empty) {
  if (db_empty) {
    return CreateSchema();
  }

  sql::MetaTable meta_table;

  // Create the meta table if it doesn't already exist. The only version for
  // which this is the case is version 1.
  if (!meta_table.Init(db_.get(), /*version=*/1, kCompatibleVersionNumber)) {
    return false;
  }

  int version = meta_table.GetVersionNumber();
  if (version == kCurrentVersionNumber) {
    return true;
  }

  // Recreate the DB if the version is deprecated or too new. In the latter
  // case, the DB will never work until Chrome is re-upgraded. Assume the user
  // will continue using this Chrome version and raze the DB to get attribution
  // reporting working.
  if (version <= kDeprecatedVersionNumber ||
      meta_table.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    // Note that this also razes the meta table, so it will need to be
    // initialized again.
    db_->Raze();
    return CreateSchema();
  }

  return UpgradeAttributionStorageSqlSchema(db_.get(), &meta_table);
}

bool AttributionStorageSql::CreateSchema() {
  base::ThreadTicks start_timestamp;
  if (base::ThreadTicks::IsSupported()) {
    start_timestamp = base::ThreadTicks::Now();
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return false;
  }

  // TODO(johnidel, csharrison): Many sources will share a target origin and
  // a reporting origin, so it makes sense to make a "shared string" table for
  // these to save disk / memory. However, this complicates the schema a lot, so
  // probably best to only do it if there's performance problems here.
  //
  // Origins usually aren't _that_ big compared to a 64 bit integer(8 bytes).
  //
  // All of the columns in this table are designed to be "const" except for
  // |num_attributions|, |aggregatable_budget_consumed|, |event_level_active|
  // and |aggregatable_active| which are updated when a new trigger is
  // received. |num_attributions| is the number of times an event-level report
  // has been created for a given source. |aggregatable_budget_consumed| is the
  // aggregatable budget that has been consumed for a given source. |delegate_|
  // can choose to enforce a maximum limit on them. |event_level_active| and
  // |aggregatable_active| indicate whether a source is able to create new
  // associated event-level and aggregatable reports. |event_level_active| and
  // |aggregatable_active| can be unset on a number of conditions:
  //   - A source converted too many times.
  //   - A new source was stored after a source converted, making it
  //     ineligible for new sources due to the attribution model documented
  //     in `StoreSource()`.
  //   - A source has expired but still has unsent reports in the
  //     event_level_reports table meaning it cannot be deleted yet.
  // |source_type| is the type of the source of the source, currently always
  // |kNavigation|.
  // |attribution_logic| corresponds to the
  // |StoredSource::AttributionLogic| enum.
  // |source_site| is used to optimize the lookup of sources;
  // |CommonSourceInfo::SourceSite| is always derived from the origin.
  // |filter_data| is a serialized `attribution_reporting::FilterData` used for
  // source matching.
  //
  // |source_id| uses AUTOINCREMENT to ensure that IDs aren't reused over
  // the lifetime of the DB.
  static constexpr char kImpressionTableSql[] =
      "CREATE TABLE sources("
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
  if (!db_->Execute(kImpressionTableSql)) {
    return false;
  }

  // Optimizes source lookup by conversion destination/reporting origin
  // during calls to `MaybeCreateAndStoreReport()`,
  // `StoreSource()`, `DeleteExpiredSources()`. Sources and
  // triggers are considered matching if they share this pair. These calls
  // need to distinguish between active and inactive reports, so include
  // |event_level_active| and |aggregatable_active| in the index.
  static constexpr char kConversionDestinationIndexSql[] =
      "CREATE INDEX sources_by_active_destination_site_reporting_origin "
      "ON sources(event_level_active,aggregatable_active,"
      "destination_site,reporting_origin)";
  if (!db_->Execute(kConversionDestinationIndexSql)) {
    return false;
  }

  // Optimizes calls to `DeleteExpiredSources()` and
  // `MaybeCreateAndStoreReport()` by indexing sources by expiry
  // time. Both calls require only returning sources that expire after a
  // given time.
  static constexpr char kImpressionExpiryIndexSql[] =
      "CREATE INDEX sources_by_expiry_time "
      "ON sources(expiry_time)";
  if (!db_->Execute(kImpressionExpiryIndexSql)) {
    return false;
  }

  // Optimizes counting active sources by source origin.
  static constexpr char kImpressionOriginIndexSql[] =
      "CREATE INDEX active_sources_by_source_origin "
      "ON sources(source_origin)"
      "WHERE event_level_active=1 OR aggregatable_active=1";
  if (!db_->Execute(kImpressionOriginIndexSql)) {
    return false;
  }

  // TODO: Remove this during the next DB migration.
  static constexpr char kImpressionSiteReportingOriginIndexSql[] =
      "CREATE INDEX active_unattributed_sources_by_site_reporting_origin "
      "ON sources(source_site,reporting_origin)"
      "WHERE event_level_active=1 AND num_attributions=0 AND "
      "aggregatable_active=1 AND aggregatable_budget_consumed=0";
  if (!db_->Execute(kImpressionSiteReportingOriginIndexSql)) {
    return false;
  }

  // All columns in this table are const except |report_time| and
  // |failed_send_attempts|,
  // which are updated when a report fails to send, as part of retries.
  // |source_id| is the primary key of a row in the [sources] table,
  // [sources.source_id]. |trigger_time| is the time at which the
  // trigger was registered, and should be used for clearing site data.
  // |report_time| is the time a <report, source> pair should be
  // reported, and is specified by |delegate_|.
  //
  // |id| uses AUTOINCREMENT to ensure that IDs aren't reused over
  // the lifetime of the DB.
  static constexpr char kConversionTableSql[] =
      "CREATE TABLE event_level_reports("
      "report_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "source_id INTEGER NOT NULL,"
      "trigger_data INTEGER NOT NULL,"
      "trigger_time INTEGER NOT NULL,"
      "report_time INTEGER NOT NULL,"
      "priority INTEGER NOT NULL,"
      "failed_send_attempts INTEGER NOT NULL,"
      "external_report_id TEXT NOT NULL,"
      "debug_key INTEGER)";
  if (!db_->Execute(kConversionTableSql)) {
    return false;
  }

  // Optimize sorting reports by report time for calls to
  // `GetAttributionReports()`. The reports with the earliest report times are
  // periodically fetched from storage to be sent.
  static constexpr char kConversionReportTimeIndexSql[] =
      "CREATE INDEX event_level_reports_by_report_time "
      "ON event_level_reports(report_time)";
  if (!db_->Execute(kConversionReportTimeIndexSql)) {
    return false;
  }

  // Want to optimize report look up by source id. This allows us to
  // quickly know if an expired source can be deleted safely if it has no
  // corresponding pending reports during calls to
  // `DeleteExpiredSources()`.
  static constexpr char kConversionImpressionIdIndexSql[] =
      "CREATE INDEX event_level_reports_by_source_id "
      "ON event_level_reports(source_id)";
  if (!db_->Execute(kConversionImpressionIdIndexSql)) {
    return false;
  }

  if (!rate_limit_table_.CreateTable(db_.get())) {
    return false;
  }

  static constexpr char kDedupKeyTableSql[] =
      "CREATE TABLE dedup_keys("
      "source_id INTEGER NOT NULL,"
      "report_type INTEGER NOT NULL,"
      "dedup_key INTEGER NOT NULL,"
      "PRIMARY KEY(source_id,report_type,dedup_key))WITHOUT ROWID";
  if (!db_->Execute(kDedupKeyTableSql)) {
    return false;
  }

  // ============================
  // AGGREGATE ATTRIBUTION SCHEMA
  // ============================

  // An attribution might make multiple histogram contributions. Therefore
  // multiple rows in |aggregatable_contributions| table might correspond to the
  // same row in |aggregatable_report_metadata| table.

  // All columns in this table are const except `report_time` and
  // `failed_send_attempts`, which are updated when a report fails to send, as
  // part of retries.
  // `source_id` is the primary key of a row in the [sources] table,
  // [sources.source_id].
  // `trigger_time` is the time at which the trigger was registered, and
  // should be used for clearing site data.
  // `external_report_id` is used for deduplicating reports received by the
  // reporting origin.
  // `report_time` is the time the aggregatable report should be reported.
  // `initial_report_time` is the report time initially scheduled by the
  // browser.
  static constexpr char kAggregatableReportMetadataTableSql[] =
      "CREATE TABLE aggregatable_report_metadata("
      "aggregation_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
      "source_id INTEGER NOT NULL,"
      "trigger_time INTEGER NOT NULL,"
      "debug_key INTEGER,"
      "external_report_id TEXT NOT NULL,"
      "report_time INTEGER NOT NULL,"
      "failed_send_attempts INTEGER NOT NULL,"
      "initial_report_time INTEGER NOT NULL,"
      "aggregation_coordinator INTEGER NOT NULL,"
      "attestation_token TEXT)";
  if (!db_->Execute(kAggregatableReportMetadataTableSql)) {
    return false;
  }

  // Optimizes aggregatable report look up by source id during calls to
  // `DeleteExpiredSources()`, `ClearAggregatableAttributionsForSourceIds()`.
  static constexpr char kAggregateSourceIdIndexSql[] =
      "CREATE INDEX aggregate_source_id_idx "
      "ON aggregatable_report_metadata(source_id)";
  if (!db_->Execute(kAggregateSourceIdIndexSql)) {
    return false;
  }

  // Optimizes aggregatable report look up by trigger time for clearing site
  // data during calls to
  // `ClearAggregatableAttributionsForOriginsInRange()`.
  static constexpr char kAggregateTriggerTimeIndexSql[] =
      "CREATE INDEX aggregate_trigger_time_idx "
      "ON aggregatable_report_metadata(trigger_time)";
  if (!db_->Execute(kAggregateTriggerTimeIndexSql)) {
    return false;
  }

  // Optimizes aggregatable report look up by report time to get reports in a
  // time range during calls to
  // `GetAggregatableAttributionReportsInternal()`.
  static constexpr char kAggregateReportTimeIndexSql[] =
      "CREATE INDEX aggregate_report_time_idx "
      "ON aggregatable_report_metadata(report_time)";
  if (!db_->Execute(kAggregateReportTimeIndexSql)) {
    return false;
  }

  // All columns in this table are const.
  // `aggregation_id` is the primary key of a row in the
  // [aggregatable_report_metadata] table.
  // `contribution_id` is an arbitrary integer that distinguishes rows with the
  // same `aggregation_id`.
  // `key_high_bits` and `key_low_bits` represent the histogram bucket key that
  // is a 128-bit unsigned integer.
  // `value` is the histogram value.
  static constexpr char kAggregatableContributionsTableSql[] =
      "CREATE TABLE aggregatable_contributions("
      "aggregation_id INTEGER NOT NULL,"
      "contribution_id INTEGER NOT NULL,"
      "key_high_bits INTEGER NOT NULL,"
      "key_low_bits INTEGER NOT NULL,"
      "value INTEGER NOT NULL,"
      "PRIMARY KEY(aggregation_id,contribution_id))WITHOUT ROWID";
  if (!db_->Execute(kAggregatableContributionsTableSql)) {
    return false;
  }

  if (sql::MetaTable meta_table; !meta_table.Init(
          db_.get(), kCurrentVersionNumber, kCompatibleVersionNumber)) {
    return false;
  }

  if (!transaction.Commit()) {
    return false;
  }

  if (base::ThreadTicks::IsSupported()) {
    base::UmaHistogramMediumTimes("Conversions.Storage.CreationTime",
                                  base::ThreadTicks::Now() - start_timestamp);
  }

  return true;
}

void AttributionStorageSql::DatabaseErrorCallback(int extended_error,
                                                  sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Attempt to recover a corrupt database, unless it is setup in memory.
  if (sql::Recovery::ShouldRecover(extended_error) &&
      !path_to_database_.empty()) {
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
      !ignore_errors_for_testing_) {
    DLOG(FATAL) << db_->GetErrorMessage();
  }

  // Consider the database closed if we did not attempt to recover so we did
  // not produce further errors.
  db_init_status_ = DbStatus::kClosed;
}

bool AttributionStorageSql::DeleteSources(
    const std::vector<StoredSource::Id>& source_ids) {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return false;
  }

  static constexpr char kDeleteSourcesSql[] =
      "DELETE FROM sources WHERE source_id=?";
  sql::Statement delete_impression_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteSourcesSql));

  for (StoredSource::Id source_id : source_ids) {
    delete_impression_statement.Reset(/*clear_bound_vars=*/true);
    delete_impression_statement.BindInt64(0, *source_id);
    if (!delete_impression_statement.Run()) {
      return false;
    }
  }

  static constexpr char kDeleteDedupKeySql[] =
      "DELETE FROM dedup_keys WHERE source_id=?";
  sql::Statement delete_dedup_key_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteDedupKeySql));

  for (StoredSource::Id source_id : source_ids) {
    delete_dedup_key_statement.Reset(/*clear_bound_vars=*/true);
    delete_dedup_key_statement.BindInt64(0, *source_id);
    if (!delete_dedup_key_statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

int AttributionStorageSql::ClearAggregatableAttributionsForOriginsInRange(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    std::vector<StoredSource::Id>& source_ids_to_delete) {
  DCHECK_LE(delete_begin, delete_end);

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return -1;
  }

  // TODO(linnan): Considering optimizing SQL query by moving some logic to C++.
  // See the comment in crrev.com/c/3379484 for more information.
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kScanCandidateDataAggregatable));
  statement.BindTime(0, delete_begin);
  statement.BindTime(1, delete_end);

  int num_aggregate_reports_deleted = 0;
  while (statement.Step()) {
    if (filter.is_null() || filter.Run(blink::StorageKey(DeserializeOrigin(
                                statement.ColumnString(0))))) {
      source_ids_to_delete.emplace_back(statement.ColumnInt64(1));
      if (statement.GetColumnType(2) != sql::ColumnType::kNull) {
        if (!DeleteReportInternal(
                AttributionReport::AggregatableAttributionData::Id(
                    statement.ColumnInt64(2)))) {
          return -1;
        }
        ++num_aggregate_reports_deleted;
      }
    }
  }

  if (!statement.Succeeded() || !transaction.Commit()) {
    return -1;
  }

  return num_aggregate_reports_deleted;
}

bool AttributionStorageSql::DeleteReportInternal(
    AttributionReport::AggregatableAttributionData::Id aggregation_id) {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return false;
  }

  static constexpr char kDeleteAggregationSql[] =
      "DELETE FROM aggregatable_report_metadata WHERE aggregation_id=?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteAggregationSql));
  statement.BindInt64(0, *aggregation_id);
  if (!statement.Run()) {
    return false;
  }

  if (!DeleteAggregatableContributions(aggregation_id)) {
    return false;
  }

  return transaction.Commit();
}

bool AttributionStorageSql::DeleteAggregatableContributions(
    AttributionReport::AggregatableAttributionData::Id aggregation_id) {
  static constexpr char kDeleteContributionsSql[] =
      "DELETE FROM aggregatable_contributions WHERE aggregation_id=?";
  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteContributionsSql));
  statement.BindInt64(0, *aggregation_id);
  return statement.Run();
}

int AttributionStorageSql::ClearAggregatableAttributionsForSourceIds(
    const std::vector<StoredSource::Id>& source_ids) {
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return -1;
  }

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kDeleteAggregationsSql));

  int num_aggregatable_reports_deleted = 0;

  for (StoredSource::Id id : source_ids) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, *id);

    while (statement.Step()) {
      if (!DeleteAggregatableContributions(
              AttributionReport::AggregatableAttributionData::Id(
                  statement.ColumnInt64(0)))) {
        return -1;
      }
    }

    if (!statement.Succeeded()) {
      return -1;
    }

    num_aggregatable_reports_deleted += db_->GetLastChangeCount();
  }

  if (!transaction.Commit()) {
    return -1;
  }

  return num_aggregatable_reports_deleted;
}

std::vector<AttributionReport>
AttributionStorageSql::GetAggregatableAttributionReportsInternal(
    base::Time max_report_time,
    int limit) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetAggregatableReportsSql));
  statement.BindTime(0, max_report_time);
  statement.BindInt(1, limit);

  std::vector<AttributionReport> reports;
  while (statement.Step()) {
    absl::optional<AttributionReport> report =
        ReadAggregatableAttributionReportFromStatement(statement);
    if (report.has_value()) {
      reports.push_back(std::move(*report));
    }
  }

  if (!statement.Succeeded()) {
    return {};
  }

  return reports;
}

std::vector<AggregatableHistogramContribution>
AttributionStorageSql::GetAggregatableContributions(
    AttributionReport::AggregatableAttributionData::Id aggregation_id) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetContributionsSql));
  statement.BindInt64(0, *aggregation_id);

  std::vector<AggregatableHistogramContribution> contributions;
  while (statement.Step()) {
    absl::uint128 bucket_key =
        absl::MakeUint128(DeserializeUint64(statement.ColumnInt64(0)),
                          DeserializeUint64(statement.ColumnInt64(1)));
    int64_t value = statement.ColumnInt64(2);
    if (value <= 0 || value > delegate_->GetAggregatableBudgetPerSource() ||
        value > std::numeric_limits<uint32_t>::max()) {
      return {};
    }

    contributions.emplace_back(bucket_key, static_cast<uint32_t>(value));
  }

  return contributions;
}

RateLimitResult
AttributionStorageSql::AggregatableAttributionAllowedForBudgetLimit(
    const AttributionReport::AggregatableAttributionData&
        aggregatable_attribution,
    int64_t aggregatable_budget_consumed) {
  const int64_t budget = delegate_->GetAggregatableBudgetPerSource();
  DCHECK_GT(budget, 0);

  const int64_t capacity = budget > aggregatable_budget_consumed
                               ? budget - aggregatable_budget_consumed
                               : 0;

  if (capacity == 0) {
    return RateLimitResult::kNotAllowed;
  }

  const base::CheckedNumeric<int64_t> budget_required =
      aggregatable_attribution.BudgetRequired();
  if (!budget_required.IsValid() || budget_required.ValueOrDie() > capacity) {
    return RateLimitResult::kNotAllowed;
  }

  return RateLimitResult::kAllowed;
}

bool AttributionStorageSql::AdjustBudgetConsumedForSource(
    StoredSource::Id source_id,
    int64_t additional_budget_consumed) {
  DCHECK_GE(additional_budget_consumed, 0);

  static constexpr char kAdjustBudgetConsumedForSourceSql[] =
      "UPDATE sources "
      "SET aggregatable_budget_consumed=aggregatable_budget_consumed+? "
      "WHERE source_id=?";
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, kAdjustBudgetConsumedForSourceSql));
  statement.BindInt64(0, additional_budget_consumed);
  statement.BindInt64(1, *source_id);
  return statement.Run() && db_->GetLastChangeCount() == 1;
}

absl::optional<base::Time>
AttributionStorageSql::GetNextAggregatableAttributionReportTime(
    base::Time time) {
  return GetNextReportTime(
      SQL_FROM_HERE, attribution_queries::kNextAggregatableReportTimeSql, time);
}

absl::optional<base::Time>
AttributionStorageSql::AdjustOfflineAggregatableAttributionReportTimes(
    base::TimeDelta min_delay,
    base::TimeDelta max_delay,
    base::Time now) {
  if (!AdjustOfflineReportTimes(
          SQL_FROM_HERE, attribution_queries::kSetAggregatableReportTimeSql,
          min_delay, max_delay, now)) {
    return absl::nullopt;
  }

  return GetNextAggregatableAttributionReportTime(base::Time::Min());
}

AggregatableResult
AttributionStorageSql::MaybeCreateAggregatableAttributionReport(
    const AttributionInfo& attribution_info,
    const AttributionTrigger& trigger,
    absl::optional<AttributionReport>& report,
    absl::optional<int>& max_aggregatable_reports_per_destination) {
  const attribution_reporting::TriggerRegistration& trigger_registration =
      trigger.registration();

  const CommonSourceInfo& common_info = attribution_info.source.common_info();

  if (attribution_info.time > common_info.aggregatable_report_window_time()) {
    return AggregatableResult::kReportWindowPassed;
  }

  std::vector<AggregatableHistogramContribution> contributions =
      CreateAggregatableHistogram(
          common_info.filter_data(), common_info.source_type(),
          common_info.aggregation_keys(),
          trigger_registration.aggregatable_trigger_data,
          trigger_registration.aggregatable_values);
  if (contributions.empty()) {
    return AggregatableResult::kNoHistograms;
  }

  switch (
      ReportAlreadyStored(attribution_info.source.source_id(),
                          trigger_registration.aggregatable_dedup_key,
                          AttributionReport::Type::kAggregatableAttribution)) {
    case ReportAlreadyStoredStatus::kNotStored:
      break;
    case ReportAlreadyStoredStatus::kStored:
      return AggregatableResult::kDeduplicated;
    case ReportAlreadyStoredStatus::kError:
      return AggregatableResult::kInternalError;
  }

  switch (CapacityForStoringReport(
      trigger, AttributionReport::Type::kAggregatableAttribution)) {
    case ConversionCapacityStatus::kHasCapacity:
      break;
    case ConversionCapacityStatus::kNoCapacity:
      max_aggregatable_reports_per_destination =
          delegate_->GetMaxReportsPerDestination(
              AttributionReport::Type::kAggregatableAttribution);
      return AggregatableResult::kNoCapacityForConversionDestination;
    case ConversionCapacityStatus::kError:
      return AggregatableResult::kInternalError;
  }

  base::Time report_time =
      delegate_->GetAggregatableReportTime(attribution_info.time);

  absl::optional<std::string> attestation_token;

  if (trigger.attestation().has_value()) {
    attestation_token = trigger.attestation()->token();
  }

  base::GUID external_report_id =
      trigger.attestation().has_value()
          ? trigger.attestation()->aggregatable_report_id()
          : delegate_->NewReportID();
  report = AttributionReport(
      attribution_info, report_time, std::move(external_report_id),
      /*failed_send_attempts=*/0,
      AttributionReport::AggregatableAttributionData(
          std::move(contributions),
          AttributionReport::AggregatableAttributionData::Id(kUnsetReportId),
          report_time, trigger_registration.aggregation_coordinator,
          std::move(attestation_token)));

  return AggregatableResult::kSuccess;
}

bool AttributionStorageSql::StoreAggregatableAttributionReport(
    AttributionReport& report) {
  auto* aggregatable_attribution =
      absl::get_if<AttributionReport::AggregatableAttributionData>(
          &report.data());
  DCHECK(aggregatable_attribution);

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return false;
  }

  const AttributionInfo& attribution_info = report.attribution_info();

  static constexpr char kInsertMetadataSql[] =
      "INSERT INTO aggregatable_report_metadata"
      "(source_id,trigger_time,debug_key,external_report_id,report_time,"
      "failed_send_attempts,initial_report_time,aggregation_coordinator,"
      "attestation_token)"
      "VALUES(?,?,?,?,?,0,?,?,?)";
  sql::Statement insert_metadata_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertMetadataSql));
  insert_metadata_statement.BindInt64(0, *attribution_info.source.source_id());
  insert_metadata_statement.BindTime(1, attribution_info.time);
  BindUint64OrNull(insert_metadata_statement, 2, attribution_info.debug_key);
  insert_metadata_statement.BindString(
      3, report.external_report_id().AsLowercaseString());
  insert_metadata_statement.BindTime(4, report.report_time());
  insert_metadata_statement.BindTime(
      5, aggregatable_attribution->initial_report_time);
  insert_metadata_statement.BindInt(
      6, SerializeAggregationCoordinator(
             aggregatable_attribution->aggregation_coordinator));
  BindStringOrNull(insert_metadata_statement, 7,
                   aggregatable_attribution->attestation_token);
  if (!insert_metadata_statement.Run()) {
    return false;
  }

  aggregatable_attribution->id =
      AttributionReport::AggregatableAttributionData::Id(
          db_->GetLastInsertRowId());

  static constexpr char kInsertContributionsSql[] =
      "INSERT INTO aggregatable_contributions"
      "(aggregation_id,contribution_id,key_high_bits,key_low_bits,value)"
      "VALUES(?,?,?,?,?)";
  sql::Statement insert_contributions_statement(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertContributionsSql));

  int contribution_id = 0;
  for (const auto& contribution : aggregatable_attribution->contributions) {
    insert_contributions_statement.Reset(/*clear_bound_vars=*/true);
    insert_contributions_statement.BindInt64(0, *aggregatable_attribution->id);
    insert_contributions_statement.BindInt(1, contribution_id);
    insert_contributions_statement.BindInt64(
        2, SerializeUint64(absl::Uint128High64(contribution.key())));
    insert_contributions_statement.BindInt64(
        3, SerializeUint64(absl::Uint128Low64(contribution.key())));
    insert_contributions_statement.BindInt64(
        4, static_cast<int64_t>(contribution.value()));
    if (!insert_contributions_statement.Run()) {
      return false;
    }
    ++contribution_id;
  }

  return transaction.Commit();
}

AggregatableResult
AttributionStorageSql::MaybeStoreAggregatableAttributionReport(
    AttributionReport& report,
    int64_t aggregatable_budget_consumed,
    absl::optional<uint64_t> dedup_key,
    absl::optional<int64_t>& aggregatable_budget_per_source) {
  const auto* aggregatable_attribution =
      absl::get_if<AttributionReport::AggregatableAttributionData>(
          &report.data());
  DCHECK(aggregatable_attribution);

  switch (AggregatableAttributionAllowedForBudgetLimit(
      *aggregatable_attribution, aggregatable_budget_consumed)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      aggregatable_budget_per_source =
          delegate_->GetAggregatableBudgetPerSource();
      return AggregatableResult::kInsufficientBudget;
    case RateLimitResult::kError:
      return AggregatableResult::kInternalError;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return AggregatableResult::kInternalError;
  }

  if (!StoreAggregatableAttributionReport(report)) {
    return AggregatableResult::kInternalError;
  }

  StoredSource::Id source_id = report.attribution_info().source.source_id();

  base::CheckedNumeric<int64_t> budget_required =
      aggregatable_attribution->BudgetRequired();
  // The value was already validated by
  // `AggregatableAttributionAllowedForBudgetLimit()` above.
  DCHECK(budget_required.IsValid());
  if (!AdjustBudgetConsumedForSource(source_id, budget_required.ValueOrDie())) {
    return AggregatableResult::kInternalError;
  }

  if (dedup_key.has_value() &&
      !StoreDedupKey(source_id, *dedup_key,
                     AttributionReport::Type::kAggregatableAttribution)) {
    return AggregatableResult::kInternalError;
  }

  if (!transaction.Commit()) {
    return AggregatableResult::kInternalError;
  }

  return AggregatableResult::kSuccess;
}

// Helper to deserialize report rows. See `GetReport()` for the expected
// ordering of columns used for the input to this function.
absl::optional<AttributionReport>
AttributionStorageSql::ReadAggregatableAttributionReportFromStatement(
    sql::Statement& statement) {
  DCHECK_EQ(statement.ColumnCount(), kSourceColumnCount + 9);

  absl::optional<StoredSourceData> source_data =
      ReadSourceFromStatement(statement);
  if (!source_data.has_value()) {
    return absl::nullopt;
  }

  int col = kSourceColumnCount;
  AttributionReport::AggregatableAttributionData::Id report_id(
      statement.ColumnInt64(col++));
  base::Time trigger_time = statement.ColumnTime(col++);
  base::Time report_time = statement.ColumnTime(col++);
  absl::optional<uint64_t> trigger_debug_key =
      ColumnUint64OrNull(statement, col++);
  base::GUID external_report_id =
      base::GUID::ParseLowercase(statement.ColumnString(col++));
  int failed_send_attempts = statement.ColumnInt(col++);
  base::Time initial_report_time = statement.ColumnTime(col++);
  absl::optional<::aggregation_service::mojom::AggregationCoordinator>
      aggregation_coordinator =
          DeserializeAggregationCoordinator(statement.ColumnInt(col++));

  absl::optional<std::string> attestation_token =
      ColumnStringOrNull(statement, col++);

  // Ensure data is valid before continuing. This could happen if there is
  // database corruption.
  if (!external_report_id.is_valid() || failed_send_attempts < 0 ||
      !aggregation_coordinator.has_value()) {
    return absl::nullopt;
  }

  std::vector<AggregatableHistogramContribution> contributions =
      GetAggregatableContributions(report_id);
  if (contributions.empty()) {
    return absl::nullopt;
  }

  return AttributionReport(
      AttributionInfo(std::move(source_data->source), trigger_time,
                      trigger_debug_key),
      report_time, std::move(external_report_id), failed_send_attempts,
      AttributionReport::AggregatableAttributionData(
          std::move(contributions), report_id, initial_report_time,
          *aggregation_coordinator, std::move(attestation_token)));
}

absl::optional<AttributionReport> AttributionStorageSql::GetReport(
    AttributionReport::AggregatableAttributionData::Id report_id) {
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetAggregatableReportSql));
  statement.BindInt64(0, *report_id);

  if (!statement.Step()) {
    return absl::nullopt;
  }

  return ReadAggregatableAttributionReportFromStatement(statement);
}

std::vector<AttributionDataModel::DataKey>
AttributionStorageSql::GetAllDataKeys() {
  // We don't bother creating the DB here if it doesn't exist, because it's not
  // possible for there to be any data to return if there's no DB
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return {};
  }

  std::vector<AttributionDataModel::DataKey> keys;
  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetSourcesDataKeysSql));

  while (statement.Step()) {
    url::Origin reporting_origin = DeserializeOrigin(statement.ColumnString(0));
    if (reporting_origin.opaque()) {
      continue;
    }
    keys.emplace_back(std::move(reporting_origin));
  }

  rate_limit_table_.AppendRateLimitDataKeys(db_.get(), keys);
  return base::flat_set<AttributionDataModel::DataKey>(std::move(keys))
      .extract();
}

void AttributionStorageSql::DeleteByDataKey(
    const AttributionDataModel::DataKey& key) {
  ClearData(base::Time::Min(), base::Time::Max(),
            base::BindRepeating(std::equal_to<blink::StorageKey>(),
                                blink::StorageKey(key.reporting_origin())),
            /*delete_rate_limit_data=*/true);
}

}  // namespace content
