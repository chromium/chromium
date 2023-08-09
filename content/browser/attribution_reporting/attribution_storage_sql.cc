// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql.h"

#include <stdint.h>

#include <functional>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/aggregation_service/features.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/aggregatable_histogram_contribution.h"
#include "content/browser/attribution_reporting/attribution_features.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/create_report_result.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "content/browser/attribution_reporting/sql_queries.h"
#include "content/browser/attribution_reporting/sql_utils.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/attribution_data_model.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/trigger_verification.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

namespace {

using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;
using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;

using ::attribution_reporting::EventReportWindows;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::SourceRegistrationTimeConfig;
using ::attribution_reporting::mojom::SourceType;

const base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("Conversions");

constexpr int64_t kUnsetRecordId = -1;

const base::FeatureParam<bool> kSourceDeactivationAfterFiltering{
    &blink::features::kConversionMeasurement,
    "source_deactivation_after_filtering", false};

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

int SerializeSourceType(SourceType val) {
  return static_cast<int>(val);
}

int SerializeReportType(AttributionReport::Type val) {
  return static_cast<int>(val);
}

absl::optional<AttributionReport::Type> DeserializeReportType(int val) {
  switch (val) {
    case static_cast<int>(AttributionReport::Type::kEventLevel):
      return AttributionReport::Type::kEventLevel;
    case static_cast<int>(AttributionReport::Type::kAggregatableAttribution):
      return AttributionReport::Type::kAggregatableAttribution;
    case static_cast<int>(AttributionReport::Type::kNullAggregatable):
      return AttributionReport::Type::kNullAggregatable;
    default:
      return absl::nullopt;
  }
}

std::string SerializeFilterData(
    const attribution_reporting::FilterData& filter_data) {
  proto::AttributionFilterData msg;

  for (const auto& [filter, values] : filter_data.filter_values()) {
    proto::AttributionFilterValues filter_values_msg;
    filter_values_msg.mutable_values()->Add(values.begin(), values.end());
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
    // Serialized source filter data can only contain these keys due to DB
    // corruption or deliberate modification.
    if (entry.first ==
            attribution_reporting::FilterData::kSourceTypeFilterKey ||
        entry.first ==
            attribution_reporting::FilterConfig::kLookbackWindowKey) {
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

absl::optional<proto::AttributionReadOnlySourceData>
DeserializeReadOnlySourceDataAsProto(sql::Statement& stmt, int col) {
  std::string str;
  if (!stmt.ColumnBlobAsString(col, &str)) {
    return absl::nullopt;
  }

  proto::AttributionReadOnlySourceData msg;
  if (!msg.ParseFromString(str)) {
    return absl::nullopt;
  }
  return msg;
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

bool IsValid(const proto::AttributionAggregationKey& key) {
  return key.has_high_bits() && key.has_low_bits();
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
    if (!IsValid(key)) {
      return absl::nullopt;
    }

    keys.emplace_back(id, absl::MakeUint128(key.high_bits(), key.low_bits()));
  }

  return attribution_reporting::AggregationKeys::FromKeys(std::move(keys));
}

std::string SerializeReportMetadata(
    const AttributionReport::EventLevelData& data) {
  proto::AttributionEventLevelMetadata msg;
  msg.set_trigger_data(data.trigger_data);
  msg.set_priority(data.priority);

  std::string str;
  bool success = msg.SerializeToString(&str);
  DCHECK(success);
  return str;
}

[[nodiscard]] bool DeserializeReportMetadata(const std::string& str,
                                             uint64_t& trigger_data,
                                             int64_t& priority) {
  proto::AttributionEventLevelMetadata msg;
  if (!msg.ParseFromString(str) || !msg.has_trigger_data() ||
      !msg.has_priority()) {
    return false;
  }

  trigger_data = msg.trigger_data();
  priority = msg.priority();
  return true;
}

void SerializeCommonAggregatableData(
    const AttributionReport::CommonAggregatableData& data,
    proto::AttributionCommonAggregatableMetadata& msg) {
  if (base::FeatureList::IsEnabled(
          aggregation_service::kAggregationServiceMultipleCloudProviders) &&
      data.aggregation_coordinator_origin.has_value()) {
    msg.set_coordinator_origin(
        data.aggregation_coordinator_origin->Serialize());
  }

  if (const auto& verification_token = data.verification_token;
      verification_token.has_value()) {
    msg.set_verification_token(*verification_token);
  }

  switch (data.source_registration_time_config) {
    case SourceRegistrationTimeConfig::kInclude:
      msg.set_source_registration_time_config(
          proto::AttributionCommonAggregatableMetadata::INCLUDE);
      break;
    case SourceRegistrationTimeConfig::kExclude:
      msg.set_source_registration_time_config(
          proto::AttributionCommonAggregatableMetadata::EXCLUDE);
      break;
  }
}

[[nodiscard]] bool DeserializeCommonAggregatableData(
    const proto::AttributionCommonAggregatableMetadata& msg,
    AttributionReport::CommonAggregatableData& data) {
  if (!msg.has_source_registration_time_config()) {
    return false;
  }

  if (base::FeatureList::IsEnabled(
          ::aggregation_service::kAggregationServiceMultipleCloudProviders) &&
      msg.has_coordinator_origin()) {
    absl::optional<SuitableOrigin> aggregation_coordinator_origin =
        SuitableOrigin::Deserialize(msg.coordinator_origin());
    if (!aggregation_coordinator_origin.has_value()) {
      return false;
    }
    data.aggregation_coordinator_origin =
        std::move(aggregation_coordinator_origin);
  }

  switch (msg.source_registration_time_config()) {
    case proto::AttributionCommonAggregatableMetadata::INCLUDE:
      data.source_registration_time_config =
          SourceRegistrationTimeConfig::kInclude;
      break;
    case proto::AttributionCommonAggregatableMetadata::EXCLUDE:
      data.source_registration_time_config =
          SourceRegistrationTimeConfig::kExclude;
      break;
    default:
      return false;
  }

  if (msg.has_verification_token()) {
    data.verification_token = msg.verification_token();
  }

  return true;
}

std::string SerializeReportMetadata(
    const AttributionReport::AggregatableAttributionData& data) {
  proto::AttributionAggregatableMetadata msg;

  SerializeCommonAggregatableData(data.common_data, *msg.mutable_common_data());

  msg.mutable_contributions()->Reserve(data.contributions.size());
  for (const auto& contribution : data.contributions) {
    proto::AttributionAggregatableMetadata_Contribution* contribution_msg =
        msg.add_contributions();
    contribution_msg->mutable_key()->set_high_bits(
        absl::Uint128High64(contribution.key()));
    contribution_msg->mutable_key()->set_low_bits(
        absl::Uint128Low64(contribution.key()));
    contribution_msg->set_value(contribution.value());
  }

  std::string str;
  bool success = msg.SerializeToString(&str);
  DCHECK(success);
  return str;
}

[[nodiscard]] bool DeserializeReportMetadata(
    const std::string& str,
    int64_t max_value,
    AttributionReport::AggregatableAttributionData& data) {
  proto::AttributionAggregatableMetadata msg;
  if (!msg.ParseFromString(str) || msg.contributions().empty() ||
      !msg.has_common_data() ||
      !DeserializeCommonAggregatableData(msg.common_data(), data.common_data)) {
    return false;
  }

  data.contributions.reserve(msg.contributions().size());
  for (const auto& contribution_msg : msg.contributions()) {
    if (!contribution_msg.has_key() || !contribution_msg.has_value() ||
        !IsValid(contribution_msg.key()) || contribution_msg.value() == 0 ||
        contribution_msg.value() > max_value) {
      return false;
    }
    data.contributions.emplace_back(
        absl::MakeUint128(contribution_msg.key().high_bits(),
                          contribution_msg.key().low_bits()),
        contribution_msg.value());
  }

  return true;
}

std::string SerializeReportMetadata(
    const AttributionReport::NullAggregatableData& data) {
  proto::AttributionNullAggregatableMetadata msg;

  SerializeCommonAggregatableData(data.common_data, *msg.mutable_common_data());

  msg.set_fake_source_time(
      data.fake_source_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  std::string str;
  bool success = msg.SerializeToString(&str);
  DCHECK(success);
  return str;
}

[[nodiscard]] bool DeserializeReportMetadata(
    const std::string& str,
    AttributionReport::NullAggregatableData& data) {
  proto::AttributionNullAggregatableMetadata msg;
  if (!msg.ParseFromString(str) || !msg.has_fake_source_time() ||
      !msg.has_common_data() ||
      !DeserializeCommonAggregatableData(msg.common_data(), data.common_data)) {
    return false;
  }

  data.fake_source_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(msg.fake_source_time()));

  return true;
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

absl::optional<uint64_t> ColumnUint64OrNull(sql::Statement& statement,
                                            int col) {
  return statement.GetColumnType(col) == sql::ColumnType::kNull
             ? absl::nullopt
             : absl::make_optional(
                   DeserializeUint64(statement.ColumnInt64(col)));
}

absl::optional<EventReportWindows> ValidateEventReportWindows(
    absl::optional<EventReportWindows> registered_windows,
    const EventReportWindows& default_windows) {
  if (!registered_windows.has_value()) {
    return default_windows;
  }
  base::TimeDelta default_end_duration = *default_windows.end_times().rbegin();
  if (registered_windows->start_time() > default_end_duration) {
    return absl::nullopt;
  }

  registered_windows->MaybeTruncate(default_end_duration);
  return registered_windows;
}

struct StoredSourceData {
  StoredSource source;
  int num_conversions;
  int num_aggregatable_reports;
};

constexpr int kSourceColumnCount = 19;
// Helper to deserialize source rows. See `GetActiveSources()` for the
// expected ordering of columns used for the input to this function.
absl::optional<StoredSourceData> ReadSourceFromStatement(
    sql::Statement& statement,
    sql::Database& db) {
  DCHECK_GE(statement.ColumnCount(), kSourceColumnCount);

  int col = 0;

  if (statement.GetColumnType(col) == sql::ColumnType::kNull) {
    return absl::nullopt;
  }

  StoredSource::Id source_id(statement.ColumnInt64(col++));
  uint64_t source_event_id = DeserializeUint64(statement.ColumnInt64(col++));
  absl::optional<SuitableOrigin> source_origin =
      SuitableOrigin::Deserialize(statement.ColumnString(col++));
  absl::optional<SuitableOrigin> reporting_origin =
      SuitableOrigin::Deserialize(statement.ColumnString(col++));
  base::Time source_time = statement.ColumnTime(col++);
  base::Time expiry_time = statement.ColumnTime(col++);
  base::Time aggregatable_report_window_time = statement.ColumnTime(col++);
  absl::optional<SourceType> source_type =
      DeserializeSourceType(statement.ColumnInt(col++));
  absl::optional<StoredSource::AttributionLogic> attribution_logic =
      DeserializeAttributionLogic(statement.ColumnInt(col++));
  int64_t priority = statement.ColumnInt64(col++);
  absl::optional<uint64_t> debug_key = ColumnUint64OrNull(statement, col++);
  int num_conversions = statement.ColumnInt(col++);
  int64_t aggregatable_budget_consumed = statement.ColumnInt64(col++);
  int num_aggregatable_reports = statement.ColumnInt(col++);
  absl::optional<attribution_reporting::AggregationKeys> aggregation_keys =
      DeserializeAggregationKeys(statement, col++);

  if (!source_origin || !reporting_origin || !source_type.has_value() ||
      !attribution_logic.has_value() || num_conversions < 0 ||
      aggregatable_budget_consumed < 0 || num_aggregatable_reports < 0 ||
      !aggregation_keys.has_value() ||
      !StoredSource::IsExpiryOrReportWindowTimeValid(expiry_time,
                                                     source_time) ||
      !StoredSource::IsExpiryOrReportWindowTimeValid(
          aggregatable_report_window_time, source_time)) {
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

  absl::optional<proto::AttributionReadOnlySourceData>
      read_only_source_data_msg =
          DeserializeReadOnlySourceDataAsProto(statement, col++);
  if (!read_only_source_data_msg.has_value()) {
    return absl::nullopt;
  }

  int max_event_level_reports =
      read_only_source_data_msg->max_event_level_reports();
  if (max_event_level_reports < 0) {
    return absl::nullopt;
  }

  std::vector<base::TimeDelta> end_times;
  for (int64_t time :
       read_only_source_data_msg->event_level_report_window_end_times()) {
    end_times.push_back(base::Microseconds(time));
  }

  absl::optional<EventReportWindows> event_report_windows =
      EventReportWindows::Create(
          base::Microseconds(read_only_source_data_msg
                                 ->event_level_report_window_start_time()),
          std::move(end_times));
  if (!event_report_windows.has_value()) {
    return absl::nullopt;
  }

  static constexpr char kDestinationSitesSql[] =
      "SELECT destination_site "
      "FROM source_destinations "
      "WHERE source_id=?";
  sql::Statement destination_sites_statement(
      db.GetCachedStatement(SQL_FROM_HERE, kDestinationSitesSql));
  destination_sites_statement.BindInt64(0, *source_id);

  std::vector<net::SchemefulSite> destination_sites;
  while (destination_sites_statement.Step()) {
    auto destination_site = net::SchemefulSite::Deserialize(
        destination_sites_statement.ColumnString(0));
    destination_sites.push_back(std::move(destination_site));
  }
  if (!destination_sites_statement.Succeeded()) {
    return absl::nullopt;
  }

  auto destination_set = attribution_reporting::DestinationSet::Create(
      std::move(destination_sites));
  if (!destination_set.has_value()) {
    return absl::nullopt;
  }

  return StoredSourceData{
      .source = StoredSource(
          CommonSourceInfo(std::move(*source_origin),
                           std::move(*reporting_origin), *source_type),
          source_event_id, std::move(*destination_set), source_time,
          expiry_time, std::move(*event_report_windows),
          aggregatable_report_window_time, max_event_level_reports, priority,
          std::move(*filter_data), debug_key, std::move(*aggregation_keys),
          *attribution_logic, *active_state, source_id,
          aggregatable_budget_consumed),
      .num_conversions = num_conversions,
      .num_aggregatable_reports = num_aggregatable_reports};
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

  return ReadSourceFromStatement(statement, *db);
}

base::FilePath DatabasePath(const base::FilePath& user_data_directory) {
  return user_data_directory.Append(kDatabasePath);
}

StorableSource::Result DestinationRateLimitResultToStorableSourceResult(
    RateLimitTable::DestinationRateLimitResult r) {
  switch (r) {
    case RateLimitTable::DestinationRateLimitResult::kAllowed:
      return StorableSource::Result::kSuccess;
    case RateLimitTable::DestinationRateLimitResult::kHitGlobalLimit:
      return StorableSource::Result::kDestinationGlobalLimitReached;
    case RateLimitTable::DestinationRateLimitResult::kHitReportingLimit:
      return StorableSource::Result::kDestinationReportingLimitReached;
    case RateLimitTable::DestinationRateLimitResult::kHitBothLimits:
      return StorableSource::Result::kDestinationBothLimitsReached;
    case RateLimitTable::DestinationRateLimitResult::kError:
      return StorableSource::Result::kInternalError;
  }
}

}  // namespace

AttributionStorageSql::AttributionStorageSql(
    const base::FilePath& user_data_directory,
    std::unique_ptr<AttributionStorageDelegate> delegate)
    : path_to_database_(user_data_directory.empty()
                            ? base::FilePath()
                            : DatabasePath(user_data_directory)),
      db_(sql::DatabaseOptions{.exclusive_locking = true,
                               .page_size = 4096,
                               .cache_size = 32}),
      delegate_(std::move(delegate)),
      rate_limit_table_(delegate_.get()) {
  DCHECK(delegate_);

  db_.set_histogram_tag("Conversions");
}

AttributionStorageSql::~AttributionStorageSql() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool AttributionStorageSql::DeactivateSources(
    const std::vector<StoredSource::Id>& sources) {
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  static constexpr char kDeactivateSourcesSql[] =
      "UPDATE sources "
      "SET event_level_active=0,aggregatable_active=0 "
      "WHERE source_id=?";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeactivateSourcesSql));

  for (StoredSource::Id id : sources) {
    statement.Reset(/*clear_bound_vars=*/true);
    statement.BindInt64(0, *id);
    if (!statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

StoreSourceResult AttributionStorageSql::StoreSource(
    const StorableSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Force the creation of the database if it doesn't exist, as we need to
  // persist the source.
  if (!LazyInit(DbCreationPolicy::kCreateIfAbsent)) {
    return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  const base::Time source_time = base::Time::Now();

  if (StorableSource::Result result =
          CheckDestinationRateLimit(source, source_time);
      result != StorableSource::Result::kSuccess) {
    StoreSourceResult store_result(result);
    if (result != StorableSource::Result::kInternalError) {
      store_result.max_destinations_per_rate_limit_window_reporting_origin =
          delegate_->GetDestinationRateLimit().max_per_reporting_site;
    }
    return store_result;
  }

  // Only delete expired impressions periodically to avoid excessive DB
  // operations.
  const base::TimeDelta delete_frequency =
      delegate_->GetDeleteExpiredSourcesFrequency();
  DCHECK_GE(delete_frequency, base::TimeDelta());
  if (source_time - last_deleted_expired_sources_ >= delete_frequency) {
    if (!DeleteExpiredSources()) {
      return StoreSourceResult(StorableSource::Result::kInternalError);
    }
    last_deleted_expired_sources_ = source_time;
  }

  const CommonSourceInfo& common_info = source.common_info();

  const std::string serialized_source_origin =
      common_info.source_origin().Serialize();
  if (!HasCapacityForStoringSource(serialized_source_origin)) {
    return StoreSourceResult(
        StorableSource::Result::kInsufficientSourceCapacity,
        /*min_fake_report_time=*/absl::nullopt,
        /*max_destinations_per_source_site_reporting_site=*/absl::nullopt,
        delegate_->GetMaxSourcesPerOrigin());
  }

  switch (rate_limit_table_.SourceAllowedForDestinationLimit(&db_, source,
                                                             source_time)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      return StoreSourceResult(
          StorableSource::Result::kInsufficientUniqueDestinationCapacity,
          /*min_fake_report_time=*/absl::nullopt,
          delegate_->GetMaxDestinationsPerSourceSiteReportingSite());
    case RateLimitResult::kError:
      return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  switch (rate_limit_table_.SourceAllowedForReportingOriginLimit(&db_, source,
                                                                 source_time)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      return StoreSourceResult(
          StorableSource::Result::kExcessiveReportingOrigins);
    case RateLimitResult::kError:
      return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  switch (rate_limit_table_.SourceAllowedForReportingOriginPerSiteLimit(
      &db_, source, source_time)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      return StoreSourceResult(
          StorableSource::Result::kReportingOriginsPerSiteLimitReached);
    case RateLimitResult::kError:
      return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  const attribution_reporting::SourceRegistration& reg = source.registration();

  const base::Time expiry_time = delegate_->GetExpiryTime(
      reg.expiry, source_time, common_info.source_type());
  const base::Time event_report_window_time = ComputeReportWindowTime(
      delegate_->GetReportWindowTime(reg.event_report_window, source_time),
      expiry_time);
  const base::Time aggregatable_report_window_time =
      ComputeReportWindowTime(delegate_->GetReportWindowTime(
                                  reg.aggregatable_report_window, source_time),
                              expiry_time);

  auto event_report_windows = ValidateEventReportWindows(
      std::move(reg.event_report_windows),
      delegate_->GetDefaultEventReportWindows(
          common_info.source_type(), event_report_window_time - source_time));
  if (!event_report_windows.has_value()) {
    return StoreSourceResult(
        StorableSource::Result::kEventReportWindowsInvalidStartTime);
  }

  int max_event_level_reports = reg.max_event_level_reports.value_or(
      delegate_->GetDefaultAttributionsPerSource(common_info.source_type()));

  double channel_capacity = delegate_->ComputeChannelCapacity(
      common_info, *event_report_windows, source_time, max_event_level_reports);
  if (channel_capacity >
      delegate_->GetMaxChannelCapacity(common_info.source_type())) {
    return StoreSourceResult(
        StorableSource::Result::kExceedsMaxChannelCapacity);
  }

  AttributionStorageDelegate::RandomizedResponse randomized_response =
      delegate_->GetRandomizedResponse(common_info, *event_report_windows,
                                       source_time, max_event_level_reports);
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
      "(source_event_id,source_origin,"
      "reporting_origin,source_time,"
      "expiry_time,aggregatable_report_window_time,"
      "source_type,attribution_logic,priority,source_site,"
      "num_attributions,event_level_active,aggregatable_active,debug_key,"
      "aggregatable_budget_consumed,num_aggregatable_reports,"
      "aggregatable_source,filter_data,read_only_source_data)"
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,0,0,?,?,?)";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertImpressionSql));
  statement.BindInt64(0, SerializeUint64(reg.source_event_id));
  statement.BindString(1, serialized_source_origin);
  statement.BindString(2, common_info.reporting_origin().Serialize());
  statement.BindTime(3, source_time);
  statement.BindTime(4, expiry_time);
  statement.BindTime(5, aggregatable_report_window_time);
  statement.BindInt(6, SerializeSourceType(common_info.source_type()));
  statement.BindInt(7, SerializeAttributionLogic(attribution_logic));
  statement.BindInt64(8, reg.priority);
  statement.BindString(9, common_info.source_site().Serialize());
  statement.BindInt(10, num_conversions);
  statement.BindBool(11, event_level_active);
  statement.BindBool(12, aggregatable_active);

  BindUint64OrNull(statement, 13, reg.debug_key);

  absl::optional<StoredSource::ActiveState> active_state =
      GetSourceActiveState(event_level_active, aggregatable_active);
  DCHECK(active_state.has_value());

  statement.BindBlob(14, SerializeAggregationKeys(reg.aggregation_keys));
  statement.BindBlob(15, SerializeFilterData(reg.filter_data));
  statement.BindBlob(16, SerializeReadOnlySourceData(*event_report_windows,
                                                     max_event_level_reports));

  if (!statement.Run()) {
    return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  const StoredSource::Id source_id(db_.GetLastInsertRowId());

  static constexpr char kInsertDestinationSql[] =
      "INSERT INTO source_destinations(source_id,destination_site)"
      "VALUES(?,?)";
  sql::Statement insert_destination_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertDestinationSql));
  insert_destination_statement.BindInt64(0, *source_id);
  for (const auto& site : reg.destination_set.destinations()) {
    insert_destination_statement.Reset(/*clear_bound_vars=*/false);
    insert_destination_statement.BindString(1, site.Serialize());
    if (!insert_destination_statement.Run()) {
      return StoreSourceResult(StorableSource::Result::kInternalError);
    }
  }

  const StoredSource stored_source(
      source.common_info(), reg.source_event_id, reg.destination_set,
      source_time, expiry_time, std::move(*event_report_windows),
      aggregatable_report_window_time, max_event_level_reports, reg.priority,
      reg.filter_data, reg.debug_key, reg.aggregation_keys, attribution_logic,
      *active_state, source_id,
      /*aggregatable_budget_consumed=*/0);

  if (!rate_limit_table_.AddRateLimitForSource(&db_, stored_source)) {
    return StoreSourceResult(StorableSource::Result::kInternalError);
  }

  absl::optional<base::Time> min_fake_report_time;

  if (attribution_logic == StoredSource::AttributionLogic::kFalsely) {
    for (const auto& fake_report : *randomized_response) {
      DCHECK_EQ(fake_report.trigger_data,
                delegate_->SanitizeTriggerData(fake_report.trigger_data,
                                               common_info.source_type()));

      DCHECK_LT(source_time, fake_report.trigger_time);
      DCHECK_LT(fake_report.trigger_time, fake_report.report_time);

      // Set the `context_origin` to be the source origin for fake reports,
      // as these reports are generated only via the source site's context.
      // The fake destinations are not relevant to the context that
      // actually created the report.
      AttributionReport fake_attribution_report(
          AttributionInfo(fake_report.trigger_time,
                          /*debug_key=*/absl::nullopt,
                          /*context_origin=*/common_info.source_origin()),
          AttributionReport::Id(kUnsetRecordId), fake_report.report_time,
          /*initial_report_time=*/fake_report.report_time,
          delegate_->NewReportID(), /*failed_send_attempts=*/0,
          AttributionReport::EventLevelData(fake_report.trigger_data,
                                            /*priority=*/0,
                                            /*randomized_trigger_rate=*/0,
                                            stored_source));
      if (!StoreAttributionReport(fake_attribution_report)) {
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
            &db_,
            AttributionInfo(/*time=*/source_time,
                            /*debug_key=*/absl::nullopt,
                            /*context_origin=*/common_info.source_origin()),
            stored_source)) {
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

StorableSource::Result AttributionStorageSql::CheckDestinationRateLimit(
    const StorableSource& source,
    base::Time source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RateLimitTable::DestinationRateLimitResult rate_limit_result =
      rate_limit_table_.SourceAllowedForDestinationRateLimit(&db_, source,
                                                             source_time);
  base::UmaHistogramEnumeration("Conversions.DestinationRateLimitResult",
                                rate_limit_result);

  return DestinationRateLimitResultToStorableSourceResult(rate_limit_result);
}

// Checks whether a new report is allowed to be stored for the given source
// based on `GetDefaultAttributionsPerSource()`. If there's sufficient capacity,
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
    int max_event_level_reports,
    int64_t conversion_priority,
    absl::optional<AttributionReport>& replaced_report) {
  DCHECK_GE(num_conversions, 0);
  DCHECK_GE(max_event_level_reports, 0);

  const auto* data =
      absl::get_if<AttributionReport::EventLevelData>(&report.data());
  DCHECK(data);

  const StoredSource& source = data->source;

  // If there's already capacity for the new report, there's nothing to do.
  if (num_conversions < max_event_level_reports) {
    return MaybeReplaceLowerPriorityEventLevelReportResult::kAddNewReport;
  }

  // Prioritization is scoped within report windows.
  // This is reasonably optimized as is because we only store a ~small number
  // of reports per source_id. Selects the report with lowest priority,
  // and uses the greatest trigger_time to break ties. This favors sending
  // reports for report closer to the source time.
  sql::Statement min_priority_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kMinPrioritySql));
  min_priority_statement.BindInt64(0, *source.source_id());
  min_priority_statement.BindTime(1, report.report_time());

  absl::optional<AttributionReport::Id> conversion_id_with_min_priority;
  int64_t min_priority;
  base::Time max_trigger_time;

  while (min_priority_statement.Step()) {
    std::string metadata;
    if (!min_priority_statement.ColumnBlobAsString(0, &metadata)) {
      continue;
    }

    uint64_t trigger_data;
    int64_t priority;
    if (!DeserializeReportMetadata(metadata, trigger_data, priority)) {
      continue;
    }
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
        db_.GetCachedStatement(SQL_FROM_HERE, kDeactivateSql));
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
  if (!DeleteReport(*conversion_id_with_min_priority)) {
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

bool HasAggregatableData(
    const attribution_reporting::TriggerRegistration& trigger_registration) {
  return !trigger_registration.aggregatable_trigger_data.empty() ||
         !trigger_registration.aggregatable_values.values().empty();
}

}  // namespace

CreateReportResult AttributionStorageSql::MaybeCreateAndStoreReport(
    const AttributionTrigger& trigger) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const attribution_reporting::TriggerRegistration& trigger_registration =
      trigger.registration();

  const base::Time trigger_time = base::Time::Now();

  AttributionInfo attribution_info(
      trigger_time, trigger_registration.debug_key,
      /*context_origin=*/trigger.destination_origin());

  // Declarations for all of the various pieces of information which may be
  // collected and/or returned as a result of computing new reports in order to
  // produce a `CreateReportResult`.
  absl::optional<EventLevelResult> event_level_status;
  absl::optional<AttributionReport> new_event_level_report;

  absl::optional<AggregatableResult> aggregatable_status;
  absl::optional<AttributionReport> new_aggregatable_report;

  absl::optional<AttributionReport> replaced_event_level_report;
  absl::optional<AttributionReport> dropped_event_level_report;

  absl::optional<StoredSourceData> source_to_attribute;

  absl::optional<base::Time> min_null_aggregatable_report_time;

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

        if (event_level_status ==
                AttributionTrigger::EventLevelResult::kInternalError ||
            aggregatable_status ==
                AttributionTrigger::AggregatableResult::kInternalError) {
          min_null_aggregatable_report_time.reset();
        }

        return CreateReportResult(
            trigger_time, *event_level_status, *aggregatable_status,
            std::move(replaced_event_level_report),
            std::move(new_event_level_report),
            std::move(new_aggregatable_report),
            source_to_attribute
                ? absl::make_optional(std::move(source_to_attribute->source))
                : absl::nullopt,
            limits, std::move(dropped_event_level_report),
            min_null_aggregatable_report_time);
      };

  auto generate_null_reports_and_assemble_report_result =
      [&](absl::optional<EventLevelResult> new_event_level_status,
          absl::optional<AggregatableResult> new_aggregatable_status)
          VALID_CONTEXT_REQUIRED(sequence_checker_) {
            DCHECK(!new_aggregatable_report.has_value());

            if (!GenerateNullAggregatableReportsAndStoreReports(
                    trigger, attribution_info, new_aggregatable_report,
                    min_null_aggregatable_report_time)) {
              min_null_aggregatable_report_time.reset();
            }

            return assemble_report_result(new_event_level_status,
                                          new_aggregatable_status);
          };

  if (trigger_registration.event_triggers.empty()) {
    event_level_status = EventLevelResult::kNotRegistered;
  }

  if (!HasAggregatableData(trigger_registration)) {
    aggregatable_status = AggregatableResult::kNotRegistered;
  }

  if (event_level_status.has_value() && aggregatable_status.has_value()) {
    return assemble_report_result(/*new_event_level_status=*/absl::nullopt,
                                  /*new_aggregaable_status=*/absl::nullopt);
  }

  if (!LazyInit(DbCreationPolicy::kCreateIfAbsent)) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
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
    return generate_null_reports_and_assemble_report_result(
        EventLevelResult::kNoMatchingImpressions,
        AggregatableResult::kNoMatchingImpressions);
  }

  source_to_attribute = ReadSourceToAttribute(&db_, *source_id_to_attribute);
  // This is only possible if there is a corrupt DB.
  if (!source_to_attribute.has_value()) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  const bool top_level_filters_match =
      source_to_attribute->source.filter_data().Matches(
          source_to_attribute->source.common_info().source_type(),
          source_to_attribute->source.source_time(), trigger_time,
          trigger_registration.filters);

  if (!top_level_filters_match) {
    return generate_null_reports_and_assemble_report_result(
        EventLevelResult::kNoMatchingSourceFilterData,
        AggregatableResult::kNoMatchingSourceFilterData);
  }

  const bool deactivate_after_filtering =
      kSourceDeactivationAfterFiltering.Get();

  if (deactivate_after_filtering) {
    // Delete all unattributed sources.
    if (!DeleteSources(source_ids_to_delete)) {
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
    }

    // Deactivate all attributed sources not used.
    if (!DeactivateSources(source_ids_to_deactivate)) {
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
    }
  }

  absl::optional<uint64_t> dedup_key;
  if (!event_level_status.has_value()) {
    if (EventLevelResult create_event_level_status =
            MaybeCreateEventLevelReport(
                attribution_info, source_to_attribute->source, trigger,
                new_event_level_report, dedup_key,
                limits.max_event_level_reports_per_destination);
        create_event_level_status != EventLevelResult::kSuccess) {
      event_level_status = create_event_level_status;
    }
  }

  absl::optional<uint64_t> aggregatable_dedup_key;
  if (!aggregatable_status.has_value()) {
    if (AggregatableResult create_aggregatable_status =
            MaybeCreateAggregatableAttributionReport(
                attribution_info, source_to_attribute->source, trigger,
                new_aggregatable_report, aggregatable_dedup_key,
                limits.max_aggregatable_reports_per_destination);
        create_aggregatable_status != AggregatableResult::kSuccess) {
      aggregatable_status = create_aggregatable_status;
    }
  }

  if (event_level_status == EventLevelResult::kInternalError ||
      aggregatable_status == AggregatableResult::kInternalError) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  if (event_level_status.has_value() && aggregatable_status.has_value()) {
    return generate_null_reports_and_assemble_report_result(
        /*new_event_level_status=*/absl::nullopt,
        /*new_aggregaable_status=*/absl::nullopt);
  }

  switch (rate_limit_table_.AttributionAllowedForAttributionLimit(
      &db_, attribution_info, source_to_attribute->source)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      limits.rate_limits_max_attributions =
          delegate_->GetRateLimits().max_attributions;
      new_aggregatable_report.reset();
      return generate_null_reports_and_assemble_report_result(
          EventLevelResult::kExcessiveAttributions,
          AggregatableResult::kExcessiveAttributions);
    case RateLimitResult::kError:
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
  }

  switch (rate_limit_table_.AttributionAllowedForReportingOriginLimit(
      &db_, attribution_info, source_to_attribute->source)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      limits.rate_limits_max_attribution_reporting_origins =
          delegate_->GetRateLimits().max_attribution_reporting_origins;
      new_aggregatable_report.reset();
      return generate_null_reports_and_assemble_report_result(
          EventLevelResult::kExcessiveReportingOrigins,
          AggregatableResult::kExcessiveReportingOrigins);
    case RateLimitResult::kError:
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
  }

  sql::Transaction transaction(&db_);
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
    store_aggregatable_status = MaybeStoreAggregatableAttributionReportData(
        *new_aggregatable_report,
        source_to_attribute->source.aggregatable_budget_consumed(),
        source_to_attribute->num_aggregatable_reports, aggregatable_dedup_key,
        limits.aggregatable_budget_per_source,
        limits.max_aggregatable_reports_per_source);
  }

  if (store_event_level_status == EventLevelResult::kInternalError ||
      store_aggregatable_status == AggregatableResult::kInternalError) {
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  if (!IsSuccessResult(store_aggregatable_status)) {
    new_aggregatable_report.reset();
  }

  // Stores null reports and the aggregatable report here to be in the same
  // transaction.
  if (!GenerateNullAggregatableReportsAndStoreReports(
          trigger, attribution_info, new_aggregatable_report,
          min_null_aggregatable_report_time)) {
    min_null_aggregatable_report_time.reset();
    return assemble_report_result(EventLevelResult::kInternalError,
                                  AggregatableResult::kInternalError);
  }

  // Early exit if done modifying the storage. Noised reports still need to
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

  if (!deactivate_after_filtering) {
    // Delete all unattributed sources.
    if (!DeleteSources(source_ids_to_delete)) {
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
    }

    // Deactivate all attributed sources not used.
    if (!DeactivateSources(source_ids_to_deactivate)) {
      return assemble_report_result(EventLevelResult::kInternalError,
                                    AggregatableResult::kInternalError);
    }
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

  if (!rate_limit_table_.AddRateLimitForAttribution(
          &db_, attribution_info, source_to_attribute->source)) {
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
  sql::Statement statement(db_.GetCachedStatement(
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

    // aggregatable_budget_consumed > 0 implies num_aggregatable_reports > 0 so
    // we don't check it here.
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
    const StoredSource& source,
    const AttributionTrigger& trigger,
    absl::optional<AttributionReport>& report,
    absl::optional<uint64_t>& dedup_key,
    absl::optional<int>& max_event_level_reports_per_destination) {
  if (source.attribution_logic() == StoredSource::AttributionLogic::kFalsely) {
    DCHECK_EQ(source.active_state(),
              StoredSource::ActiveState::kReachedEventLevelAttributionLimit);
    return EventLevelResult::kFalselyAttributedSource;
  }

  const CommonSourceInfo& common_info = source.common_info();

  switch (source.event_report_windows().FallsWithin(attribution_info.time -
                                                    source.source_time())) {
    case EventReportWindows::WindowResult::kFallsWithin:
      break;
    case EventReportWindows::WindowResult::kNotStarted:
      return EventLevelResult::kReportWindowNotStarted;
    case EventReportWindows::WindowResult::kPassed:
      return EventLevelResult::kReportWindowPassed;
  }

  const SourceType source_type = common_info.source_type();

  auto event_trigger = base::ranges::find_if(
      trigger.registration().event_triggers,
      [&](const attribution_reporting::EventTriggerData& event_trigger) {
        return source.filter_data().Matches(
            source_type, source.source_time(),
            /*trigger_time=*/attribution_info.time, event_trigger.filters);
      });

  if (event_trigger == trigger.registration().event_triggers.end()) {
    return EventLevelResult::kNoMatchingConfigurations;
  }

  switch (ReportAlreadyStored(source.source_id(), event_trigger->dedup_key,
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

  const EventReportWindows& event_report_windows =
      source.event_report_windows();

  const base::Time report_time = delegate_->GetEventLevelReportTime(
      event_report_windows, source.source_time(), attribution_info.time);

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
  const double randomized_response_rate = delegate_->GetRandomizedResponseRate(
      event_report_windows, source_type, source.max_event_level_reports());

  // TODO(apaseltiner): Consider informing the manager if the trigger
  // data was out of range for DevTools issue reporting.
  report = AttributionReport(
      attribution_info, AttributionReport::Id(kUnsetRecordId), report_time,
      /*initial_report_time=*/report_time, delegate_->NewReportID(),
      /*failed_send_attempts=*/0,
      AttributionReport::EventLevelData(
          delegate_->SanitizeTriggerData(event_trigger->data, source_type),
          event_trigger->priority, randomized_response_rate, source));

  dedup_key = event_trigger->dedup_key;

  return EventLevelResult::kSuccess;
}

EventLevelResult AttributionStorageSql::MaybeStoreEventLevelReport(
    AttributionReport& report,
    absl::optional<uint64_t> dedup_key,
    int num_conversions,
    absl::optional<AttributionReport>& replaced_report,
    absl::optional<AttributionReport>& dropped_report) {
  auto* event_level_data =
      absl::get_if<AttributionReport::EventLevelData>(&report.data());
  DCHECK(event_level_data);

  const StoredSource& source = event_level_data->source;
  if (source.active_state() ==
      StoredSource::ActiveState::kReachedEventLevelAttributionLimit) {
    dropped_report = std::move(report);
    return EventLevelResult::kExcessiveReports;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return EventLevelResult::kInternalError;
  }

  const auto maybe_replace_lower_priority_report_result =
      MaybeReplaceLowerPriorityEventLevelReport(
          report, num_conversions, source.max_event_level_reports(),
          event_level_data->priority, replaced_report);
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

  // Reports with `AttributionLogic::kNever` should be included in all
  // attribution operations and matching, but only `kTruthfully` should generate
  // reports that get sent.
  const bool create_report =
      source.attribution_logic() == StoredSource::AttributionLogic::kTruthfully;

  if (create_report) {
    if (!StoreAttributionReport(report)) {
      return EventLevelResult::kInternalError;
    }
  }

  // If a dedup key is present, store it. We do this regardless of whether
  // `create_report` is true to avoid leaking whether the report was actually
  // stored.
  if (dedup_key.has_value() &&
      !StoreDedupKey(source.source_id(), *dedup_key,
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
    sql::Statement impression_update_statement(db_.GetCachedStatement(
        SQL_FROM_HERE, kUpdateImpressionForConversionSql));

    // Update the attributed source.
    impression_update_statement.BindInt64(0, *source.source_id());
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

// Helper to deserialize report rows. See `GetReport()` for the expected
// ordering of columns used for the input to this function.
absl::optional<AttributionReport>
AttributionStorageSql::ReadReportFromStatement(sql::Statement& statement) {
  DCHECK_EQ(statement.ColumnCount(), kSourceColumnCount + 11);

  absl::optional<StoredSourceData> source_data =
      ReadSourceFromStatement(statement, db_);

  int col = kSourceColumnCount;
  int64_t report_id = statement.ColumnInt64(col++);
  base::Time trigger_time = statement.ColumnTime(col++);
  base::Time report_time = statement.ColumnTime(col++);
  base::Time initial_report_time = statement.ColumnTime(col++);
  int failed_send_attempts = statement.ColumnInt(col++);
  base::Uuid external_report_id =
      base::Uuid::ParseLowercase(statement.ColumnString(col++));
  absl::optional<uint64_t> trigger_debug_key =
      ColumnUint64OrNull(statement, col++);
  auto context_origin =
      SuitableOrigin::Deserialize(statement.ColumnString(col++));
  auto reporting_origin =
      SuitableOrigin::Deserialize(statement.ColumnString(col++));
  absl::optional<AttributionReport::Type> report_type =
      DeserializeReportType(statement.ColumnInt(col++));

  // Ensure data is valid before continuing. This could happen if there is
  // database corruption.
  // TODO(apaseltiner): Should we raze the DB if we've detected corruption?
  //
  // TODO(apaseltiner): Consider verifying that `context_origin` is valid for
  // the associated source.
  if (failed_send_attempts < 0 || !external_report_id.is_valid() ||
      !context_origin.has_value() || !reporting_origin.has_value() ||
      !report_type.has_value()) {
    return absl::nullopt;
  }

  if (source_data && *source_data->source.common_info().reporting_origin() !=
                         *reporting_origin) {
    return absl::nullopt;
  }

  std::string metadata;
  if (!statement.ColumnBlobAsString(col++, &metadata)) {
    return absl::nullopt;
  }

  absl::optional<AttributionReport::Data> data;
  switch (*report_type) {
    case AttributionReport::Type::kEventLevel: {
      if (!source_data) {
        return absl::nullopt;
      }
      uint64_t trigger_data;
      int64_t priority;
      if (!DeserializeReportMetadata(metadata, trigger_data, priority)) {
        return absl::nullopt;
      }

      double randomized_response_rate = delegate_->GetRandomizedResponseRate(
          source_data->source.event_report_windows(),
          source_data->source.common_info().source_type(),
          source_data->source.max_event_level_reports());

      data = AttributionReport::EventLevelData(trigger_data, priority,
                                               randomized_response_rate,
                                               std::move(source_data->source));
      break;
    }
    case AttributionReport::Type::kAggregatableAttribution: {
      if (!source_data) {
        return absl::nullopt;
      }
      data = AttributionReport::AggregatableAttributionData(
          AttributionReport::CommonAggregatableData(),
          /*contributions=*/{}, std::move(source_data->source));
      if (!DeserializeReportMetadata(
              metadata, delegate_->GetAggregatableBudgetPerSource(),
              absl::get<AttributionReport::AggregatableAttributionData>(
                  *data))) {
        return absl::nullopt;
      }
      break;
    }
    case AttributionReport::Type::kNullAggregatable:
      if (source_data) {
        return absl::nullopt;
      }
      data = AttributionReport::NullAggregatableData(
          AttributionReport::CommonAggregatableData(),
          /*reporting_origin=*/std::move(*reporting_origin),
          /*fake_source_time=*/base::Time());
      if (!DeserializeReportMetadata(
              metadata,
              absl::get<AttributionReport::NullAggregatableData>(*data))) {
        return absl::nullopt;
      }
      break;
  }

  DCHECK(data.has_value());
  return AttributionReport(AttributionInfo(trigger_time, trigger_debug_key,
                                           std::move(*context_origin)),
                           AttributionReport::Id(report_id), report_time,
                           initial_report_time, std::move(external_report_id),
                           failed_send_attempts, std::move(*data));
}

std::vector<AttributionReport> AttributionStorageSql::GetAttributionReports(
    base::Time max_report_time,
    int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return {};
  }

  std::vector<AttributionReport> reports =
      GetReportsInternal(max_report_time, limit);

  delegate_->ShuffleReports(reports);
  return reports;
}

std::vector<AttributionReport> AttributionStorageSql::GetReportsInternal(
    base::Time max_report_time,
    int limit) {
  // Get at most |limit| entries in the reports table with a
  // |report_time| no greater than |max_report_time| and their matching
  // information from the impression table. Negatives are treated as no limit
  // (https://sqlite.org/lang_select.html#limitoffset).
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetReportsSql));
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

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kNextReportTimeSql));
  statement.BindTime(0, time);

  if (statement.Step() &&
      statement.GetColumnType(0) != sql::ColumnType::kNull) {
    return statement.ColumnTime(0);
  }

  return absl::nullopt;
}

std::vector<AttributionReport> AttributionStorageSql::GetReports(
    const std::vector<AttributionReport::Id>& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return {};
  }

  std::vector<AttributionReport> reports;
  for (AttributionReport::Id id : ids) {
    absl::optional<AttributionReport> report = GetReport(id);
    if (report.has_value()) {
      reports.push_back(std::move(*report));
    }
  }
  return reports;
}

absl::optional<AttributionReport> AttributionStorageSql::GetReport(
    AttributionReport::Id id) {
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetReportSql));
  statement.BindInt64(0, *id);

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
  sql::Statement select_expired_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kSelectExpiredSourcesSql));
  select_expired_statement.BindTime(0, base::Time::Now());
  select_expired_statement.BindInt(1, kMaxDeletesPerBatch);
  if (!delete_sources_from_paged_select(select_expired_statement)) {
    return false;
  }

  // Delete all sources that have no associated reports and are
  // inactive. This is done in a separate statement from
  // |kSelectExpiredSourcesSql| so that each query is optimized by an index.
  // Optimized by |kSourcesByReportingOriginActiveIndexSql|.
  sql::Statement select_inactive_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kSelectInactiveSourcesSql));
  select_inactive_statement.BindInt(0, kMaxDeletesPerBatch);
  return delete_sources_from_paged_select(select_inactive_statement);
}

bool AttributionStorageSql::DeleteReport(AttributionReport::Id report_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return true;
  }

  static constexpr char kDeleteReportSql[] =
      "DELETE FROM reports WHERE report_id=?";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteReportSql));
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

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kUpdateFailedReportSql));
  statement.BindTime(0, new_report_time);
  statement.BindInt64(1, *report_id);
  return statement.Run() && db_.GetLastChangeCount() == 1;
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

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kSetReportTimeSql));
  statement.BindTime(0, now + delay->min);
  statement.BindTimeDelta(1, delay->max - delay->min + base::Microseconds(1));
  statement.BindTime(2, now);
  if (!statement.Run()) {
    return absl::nullopt;
  }

  return GetNextReportTime(base::Time::Min());
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
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return;
  }

  // TODO(csharrison, johnidel): This query can be split up and optimized by
  // adding indexes on the time and trigger_time columns.
  // See this comment for more information:
  // crrev.com/c/2150071/4/content/browser/conversions/conversion_storage_sql.cc#342
  //
  // TODO(crbug.com/1290377): Look into optimizing origin filter callback.
  // TODO(apaseltiner): Consider wrapping `filter` such that it deletes
  // opaque/untrustworthy origins.

  std::vector<StoredSource::Id> source_ids_to_delete;

  int num_event_reports_deleted = 0;
  int num_aggregatable_reports_deleted = 0;

  if (!ClearReportsForOriginsInRange(
          delete_begin, delete_end, filter, source_ids_to_delete,
          num_event_reports_deleted, num_aggregatable_reports_deleted)) {
    return;
  }

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
  if (!ClearReportsForSourceIds(source_ids_to_delete, num_event_reports_deleted,
                                num_aggregatable_reports_deleted)) {
    return;
  }

  if (delete_rate_limit_data &&
      !rate_limit_table_.ClearDataForSourceIds(&db_, source_ids_to_delete)) {
    return;
  }

  if (delete_rate_limit_data && !rate_limit_table_.ClearDataForOriginsInRange(
                                    &db_, delete_begin, delete_end, filter)) {
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
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return;
  }

  int num_event_reports_deleted = 0;
  int num_aggregatable_reports_deleted = 0;

  static constexpr char kDeleteAllReportsSql[] =
      "DELETE FROM reports RETURNING report_type";
  sql::Statement delete_all_reports_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteAllReportsSql));
  while (delete_all_reports_statement.Step()) {
    absl::optional<AttributionReport::Type> report_type =
        DeserializeReportType(delete_all_reports_statement.ColumnInt(0));
    if (!report_type) {
      continue;
    }
    switch (*report_type) {
      case AttributionReport::Type::kEventLevel:
        ++num_event_reports_deleted;
        break;
      case AttributionReport::Type::kAggregatableAttribution:
        ++num_aggregatable_reports_deleted;
        break;
      case AttributionReport::Type::kNullAggregatable:
        break;
    }
  }

  if (!delete_all_reports_statement.Succeeded()) {
    return;
  }

  static constexpr char kDeleteAllSourcesSql[] = "DELETE FROM sources";
  sql::Statement delete_all_sources_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteAllSourcesSql));
  if (!delete_all_sources_statement.Run()) {
    return;
  }
  int num_sources_deleted = db_.GetLastChangeCount();

  static constexpr char kDeleteAllDedupKeysSql[] = "DELETE FROM dedup_keys";
  sql::Statement delete_all_dedup_keys_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteAllDedupKeysSql));
  if (!delete_all_dedup_keys_statement.Run()) {
    return;
  }

  static constexpr char kDeleteAllSourceDestinationsSql[] =
      "DELETE FROM source_destinations";
  sql::Statement delete_all_source_destinations(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteAllSourceDestinationsSql));
  if (!delete_all_source_destinations.Run()) {
    return;
  }

  if (delete_rate_limit_data && !rate_limit_table_.ClearAllDataAllTime(&db_)) {
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
  sql::Statement statement(db_.GetCachedStatement(
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

  sql::Statement statement(db_.GetCachedStatement(
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
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kCountReportsForDestinationSql));
  statement.BindString(
      0, net::SchemefulSite(trigger.destination_origin()).Serialize());
  statement.BindInt(1, SerializeReportType(report_type));

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

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetActiveSourcesSql));
  statement.BindTime(0, base::Time::Now());
  statement.BindInt(1, limit);

  std::vector<StoredSource> sources;
  while (statement.Step()) {
    absl::optional<StoredSourceData> source_data =
        ReadSourceFromStatement(statement, db_);
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
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, attribution_queries::kDedupKeySql));
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
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertDedupKeySql));
  statement.BindInt64(0, *source_id);
  statement.BindInt(1, SerializeReportType(report_type));
  statement.BindInt64(2, SerializeUint64(dedup_key));
  return statement.Run();
}

void AttributionStorageSql::HandleInitializationFailure(
    const InitStatus status) {
  RecordInitializationStatus(status);
  db_.Close();
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

  if (!db_.has_error_callback()) {
    // The error callback may be reset if recovery was attempted, so ensure the
    // callback is re-set when the database is re-opened.
    //
    // `base::Unretained()` is safe because the callback will only be called
    // while `db_` is alive, and this instance owns `db_`.
    db_.set_error_callback(base::BindRepeating(
        &AttributionStorageSql::DatabaseErrorCallback, base::Unretained(this)));
  }

  if (path_to_database_.empty()) {
    if (!db_.OpenInMemory()) {
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
    if (!db_.Open(path_to_database_)) {
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
  if (!meta_table.Init(&db_, /*version=*/1, /*compatible_version=*/1)) {
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
    return db_.Raze() && CreateSchema();
  }

  return UpgradeAttributionStorageSqlSchema(db_, meta_table);
}

bool AttributionStorageSql::CreateSchema() {
  base::ThreadTicks start_timestamp;
  if (base::ThreadTicks::IsSupported()) {
    start_timestamp = base::ThreadTicks::Now();
  }

  sql::Transaction transaction(&db_);
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
  // |num_attributions|, |aggregatable_budget_consumed|,
  // |num_aggregatable_reports|, |event_level_active|
  // and |aggregatable_active| which are updated when a new trigger is
  // received. |num_attributions| is the number of times an event-level report
  // has been created for a given source. |aggregatable_budget_consumed| is the
  // aggregatable budget that has been consumed for a given source.
  // |num_aggregatable_reports| is the number of times an aggregatable report
  // has been created for a given source. |delegate_| can choose to enforce a
  // maximum limit on them. |event_level_active| and |aggregatable_active|
  // indicate whether a source is able to create new associated event-level and
  // aggregatable reports. |event_level_active| and |aggregatable_active| can be
  // unset on a number of conditions:
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
  // |read_only_source_data| is a serialized
  // |proto::AttributionReadOnlySourceData| containing the source's
  // |attribution_reporting::EventReportWindows| as well as its max number of
  // event level reports.
  //
  // |source_id| uses AUTOINCREMENT to ensure that IDs aren't reused over
  // the lifetime of the DB.
  static constexpr char kImpressionTableSql[] =
      "CREATE TABLE sources("
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
  if (!db_.Execute(kImpressionTableSql)) {
    return false;
  }

  // Optimizes source lookup by reporting origin
  // during calls to `MaybeCreateAndStoreReport()`,
  // `StoreSource()`, `DeleteExpiredSources()`. Sources and
  // triggers are considered matching if
  // they share a reporting origin and a destination site.
  // These calls need to distinguish between active and
  // inactive reports, so include |event_level_active| and
  // |aggregatable_active| in the index.
  static constexpr char kSourcesByActiveReportingOriginIndexSql[] =
      "CREATE INDEX sources_by_active_reporting_origin "
      "ON sources(event_level_active,"
      "aggregatable_active,reporting_origin)";
  if (!db_.Execute(kSourcesByActiveReportingOriginIndexSql)) {
    return false;
  }

  // Optimizes calls to `DeleteExpiredSources()` and
  // `MaybeCreateAndStoreReport()` by indexing sources by expiry
  // time. Both calls require only returning sources that expire after a
  // given time.
  static constexpr char kImpressionExpiryIndexSql[] =
      "CREATE INDEX sources_by_expiry_time "
      "ON sources(expiry_time)";
  if (!db_.Execute(kImpressionExpiryIndexSql)) {
    return false;
  }

  // Optimizes counting active sources by source origin.
  static constexpr char kImpressionOriginIndexSql[] =
      "CREATE INDEX active_sources_by_source_origin "
      "ON sources(source_origin)"
      "WHERE event_level_active=1 OR aggregatable_active=1";
  if (!db_.Execute(kImpressionOriginIndexSql)) {
    return false;
  }

  // Optimizes data deletion by source time.
  static constexpr char kSourcesSourceTimeIndexSql[] =
      "CREATE INDEX sources_by_source_time "
      "ON sources(source_time)";
  if (!db_.Execute(kSourcesSourceTimeIndexSql)) {
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
  // |context_origin| is the origin that secondarily owns the report for
  // data-deletion purposes. For real reports, it is the destination origin on
  // which the trigger was registered. For fake reports, it is the source
  // origin.
  // |reporting_origin| is the reporting origin for the report and is the same
  // as the |reporting_origin| of its associated source.
  // |report_type| indicates whether it's an event-level or aggregatable report.
  // |metadata| encodes the report type-specific data.
  //
  // |id| uses AUTOINCREMENT to ensure that IDs aren't reused over
  // the lifetime of the DB.
  static constexpr char kReportsTableSql[] =
      "CREATE TABLE reports("
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
      "metadata BLOB NOT NULL)";
  if (!db_.Execute(kReportsTableSql)) {
    return false;
  }

  // Optimize sorting reports by report time for calls to
  // `GetAttributionReports()`. The reports with the earliest report times are
  // periodically fetched from storage to be sent.
  static constexpr char kReportsReportTimeIndexSql[] =
      "CREATE INDEX reports_by_report_time "
      "ON reports(report_time)";
  if (!db_.Execute(kReportsReportTimeIndexSql)) {
    return false;
  }

  // Want to optimize report look up by source id. This allows us to
  // quickly know if an expired source can be deleted safely if it has no
  // corresponding pending reports during calls to
  // `DeleteExpiredSources()`.
  static constexpr char kReportsSourceIdReportTypeIndexSql[] =
      "CREATE INDEX reports_by_source_id_report_type "
      "ON reports(source_id,report_type)";
  if (!db_.Execute(kReportsSourceIdReportTypeIndexSql)) {
    return false;
  }

  // Optimizes data deletion by trigger time.
  static constexpr char kReportsTriggerTimeIndexSql[] =
      "CREATE INDEX reports_by_trigger_time "
      "ON reports(trigger_time)";
  if (!db_.Execute(kReportsTriggerTimeIndexSql)) {
    return false;
  }

  // Optimizes data keys retrieval for null reports.
  static_assert(
      static_cast<int>(AttributionReport::Type::kNullAggregatable) == 2,
      "update `report_type=2` clause below");
  static constexpr char kReportsReportTypeReportingOriginIndexSql[] =
      "CREATE INDEX reports_by_reporting_origin "
      "ON reports(reporting_origin)"
      "WHERE report_type=2";
  if (!db_.Execute(kReportsReportTypeReportingOriginIndexSql)) {
    return false;
  }

  if (!rate_limit_table_.CreateTable(&db_)) {
    return false;
  }

  static constexpr char kDedupKeyTableSql[] =
      "CREATE TABLE dedup_keys("
      "source_id INTEGER NOT NULL,"
      "report_type INTEGER NOT NULL,"
      "dedup_key INTEGER NOT NULL,"
      "PRIMARY KEY(source_id,report_type,dedup_key))WITHOUT ROWID";
  if (!db_.Execute(kDedupKeyTableSql)) {
    return false;
  }

  static constexpr char kSourceDestinationsTableSql[] =
      "CREATE TABLE source_destinations("
      "source_id INTEGER NOT NULL,"
      "destination_site TEXT NOT NULL,"
      "PRIMARY KEY(source_id,destination_site))WITHOUT ROWID";
  if (!db_.Execute(kSourceDestinationsTableSql)) {
    return false;
  }

  static constexpr char kSourceDestinationsIndexSql[] =
      "CREATE INDEX sources_by_destination_site "
      "ON source_destinations(destination_site)";
  if (!db_.Execute(kSourceDestinationsIndexSql)) {
    return false;
  }

  if (sql::MetaTable meta_table;
      !meta_table.Init(&db_, kCurrentVersionNumber, kCompatibleVersionNumber)) {
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
  // Attempt to recover a corrupt database, if it is eligible to be recovered.
  if (sql::BuiltInRecovery::RecoverIfPossible(
          &db_, extended_error,
          sql::BuiltInRecovery::Strategy::kRecoverWithMetaVersionOrRaze,
          &kAttributionStorageUseBuiltInRecoveryIfSupported)) {
    // Recovery was attempted. The database handle has been poisoned and the
    // error callback has been reset.

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
    DLOG(FATAL) << db_.GetErrorMessage();
  }

  // Consider the database closed if we did not attempt to recover so we did
  // not produce further errors.
  db_init_status_ = DbStatus::kClosed;
}

bool AttributionStorageSql::DeleteSources(
    const std::vector<StoredSource::Id>& source_ids) {
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  static constexpr char kDeleteSourcesSql[] =
      "DELETE FROM sources WHERE source_id=?";
  sql::Statement delete_impression_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSourcesSql));

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
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteDedupKeySql));

  for (StoredSource::Id source_id : source_ids) {
    delete_dedup_key_statement.Reset(/*clear_bound_vars=*/true);
    delete_dedup_key_statement.BindInt64(0, *source_id);
    if (!delete_dedup_key_statement.Run()) {
      return false;
    }
  }

  static constexpr char kDeleteSourceDestinationsSql[] =
      "DELETE FROM source_destinations WHERE source_id=?";
  sql::Statement delete_source_destinations_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteSourceDestinationsSql));

  for (StoredSource::Id source_id : source_ids) {
    delete_source_destinations_statement.Reset(/*clear_bound_vars=*/true);
    delete_source_destinations_statement.BindInt64(0, *source_id);
    if (!delete_source_destinations_statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

bool AttributionStorageSql::ClearReportsForOriginsInRange(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    std::vector<StoredSource::Id>& source_ids_to_delete,
    int& num_event_reports_deleted,
    int& num_aggregatable_reports_deleted) {
  DCHECK_LE(delete_begin, delete_end);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  auto match_filter = [&](const std::string& str) {
    return filter.is_null() || filter.Run(blink::StorageKey::CreateFirstParty(
                                   DeserializeOrigin(str)));
  };

  sql::Statement scan_sources_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kScanSourcesData));
  scan_sources_statement.BindTime(0, delete_begin);
  scan_sources_statement.BindTime(1, delete_end);

  while (scan_sources_statement.Step()) {
    if (match_filter(scan_sources_statement.ColumnString(0))) {
      source_ids_to_delete.emplace_back(scan_sources_statement.ColumnInt64(1));
    }
  }

  if (!scan_sources_statement.Succeeded()) {
    return false;
  }

  sql::Statement scan_reports_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kScanReportsData));
  scan_reports_statement.BindTime(0, delete_begin);
  scan_reports_statement.BindTime(1, delete_end);

  while (scan_reports_statement.Step()) {
    if (!match_filter(scan_reports_statement.ColumnString(0))) {
      continue;
    }
    source_ids_to_delete.emplace_back(scan_reports_statement.ColumnInt64(1));
    absl::optional<AttributionReport::Type> report_type =
        DeserializeReportType(scan_reports_statement.ColumnInt(3));
    if (report_type) {
      switch (*report_type) {
        case AttributionReport::Type::kEventLevel:
          ++num_event_reports_deleted;
          break;
        case AttributionReport::Type::kAggregatableAttribution:
          ++num_aggregatable_reports_deleted;
          break;
        case AttributionReport::Type::kNullAggregatable:
          break;
      }
    }
    if (!DeleteReport(
            AttributionReport::Id(scan_reports_statement.ColumnInt64(2)))) {
      return false;
    }
  }

  if (!scan_reports_statement.Succeeded()) {
    return false;
  }

  return transaction.Commit();
}

bool AttributionStorageSql::ClearReportsForSourceIds(
    const std::vector<StoredSource::Id>& source_ids,
    int& num_event_reports_deleted,
    int& num_aggregatable_reports_deleted) {
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kDeleteVestigialConversionSql));

  for (StoredSource::Id id : source_ids) {
    statement.Reset(/*clear_bound_vars=*/false);
    statement.BindInt64(0, *id);

    while (statement.Step()) {
      absl::optional<AttributionReport::Type> report_type =
          DeserializeReportType(statement.ColumnInt(0));
      if (!report_type) {
        continue;
      }
      switch (*report_type) {
        case AttributionReport::Type::kEventLevel:
          ++num_event_reports_deleted;
          break;
        case AttributionReport::Type::kAggregatableAttribution:
          ++num_aggregatable_reports_deleted;
          break;
        case AttributionReport::Type::kNullAggregatable:
          break;
      }
    }

    if (!statement.Succeeded()) {
      return false;
    }
  }

  return transaction.Commit();
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
      "SET aggregatable_budget_consumed=aggregatable_budget_consumed+?,"
      "num_aggregatable_reports=num_aggregatable_reports+1 "
      "WHERE source_id=?";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kAdjustBudgetConsumedForSourceSql));
  statement.BindInt64(0, additional_budget_consumed);
  statement.BindInt64(1, *source_id);
  return statement.Run() && db_.GetLastChangeCount() == 1;
}

AggregatableResult
AttributionStorageSql::MaybeCreateAggregatableAttributionReport(
    const AttributionInfo& attribution_info,
    const StoredSource& source,
    const AttributionTrigger& trigger,
    absl::optional<AttributionReport>& report,
    absl::optional<uint64_t>& dedup_key,
    absl::optional<int>& max_aggregatable_reports_per_destination) {
  const attribution_reporting::TriggerRegistration& trigger_registration =
      trigger.registration();

  const CommonSourceInfo& common_info = source.common_info();

  if (attribution_info.time > source.aggregatable_report_window_time()) {
    return AggregatableResult::kReportWindowPassed;
  }

  const SourceType source_type = common_info.source_type();

  auto matched_dedup_key = base::ranges::find_if(
      trigger.registration().aggregatable_dedup_keys,
      [&](const attribution_reporting::AggregatableDedupKey&
              aggregatable_dedup_key) {
        return source.filter_data().Matches(
            source_type, source.source_time(),
            /*trigger_time=*/attribution_info.time,
            aggregatable_dedup_key.filters);
      });

  if (matched_dedup_key !=
      trigger.registration().aggregatable_dedup_keys.end()) {
    dedup_key = matched_dedup_key->dedup_key;
  }

  switch (
      ReportAlreadyStored(source.source_id(), dedup_key,
                          AttributionReport::Type::kAggregatableAttribution)) {
    case ReportAlreadyStoredStatus::kNotStored:
      break;
    case ReportAlreadyStoredStatus::kStored:
      return AggregatableResult::kDeduplicated;
    case ReportAlreadyStoredStatus::kError:
      return AggregatableResult::kInternalError;
  }

  std::vector<AggregatableHistogramContribution> contributions =
      CreateAggregatableHistogram(
          source.filter_data(), source_type, source.source_time(),
          /*trigger_time=*/attribution_info.time, source.aggregation_keys(),
          trigger_registration.aggregatable_trigger_data,
          trigger_registration.aggregatable_values);
  if (contributions.empty()) {
    return AggregatableResult::kNoHistograms;
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

  report = AttributionReport(
      attribution_info, AttributionReport::Id(kUnsetRecordId), report_time,
      /*initial_report_time=*/report_time, delegate_->NewReportID(),
      /*failed_send_attempts=*/0,
      AttributionReport::AggregatableAttributionData(
          AttributionReport::CommonAggregatableData(
              trigger_registration.aggregation_coordinator_origin,
              /*verification_token=*/absl::nullopt,
              trigger_registration.source_registration_time_config),
          std::move(contributions), source));

  return AggregatableResult::kSuccess;
}

bool AttributionStorageSql::StoreAttributionReport(AttributionReport& report) {
  static constexpr char kStoreReportSql[] =
      "INSERT INTO reports"
      "(source_id,trigger_time,report_time,initial_report_time,"
      "failed_send_attempts,external_report_id,debug_key,context_origin,"
      "reporting_origin,report_type,metadata)"
      "VALUES(?,?,?,?,0,?,?,?,?,?,?)";
  sql::Statement store_report_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kStoreReportSql));

  const AttributionInfo& attribution_info = report.attribution_info();

  const StoredSource* source = report.GetStoredSource();

  // Per https://www.sqlite.org/autoinc.html, if no negative ROWID values are
  // inserted explicitly, then automatically generated ROWID values will always
  // be greater than zero. Therefore it's fine to use -1 as sentinel value for a
  // null source.
  store_report_statement.BindInt64(
      0, source ? *source->source_id() : kUnsetRecordId);
  store_report_statement.BindTime(1, attribution_info.time);
  store_report_statement.BindTime(2, report.report_time());
  store_report_statement.BindTime(3, report.initial_report_time());
  store_report_statement.BindString(
      4, report.external_report_id().AsLowercaseString());
  BindUint64OrNull(store_report_statement, 5, attribution_info.debug_key);
  store_report_statement.BindString(
      6, attribution_info.context_origin.Serialize());
  store_report_statement.BindString(7, report.GetReportingOrigin().Serialize());
  store_report_statement.BindInt(8,
                                 SerializeReportType(report.GetReportType()));

  std::string metadata = absl::visit(
      [](const auto& data) { return SerializeReportMetadata(data); },
      report.data());

  store_report_statement.BindBlob(9, metadata);
  if (!store_report_statement.Run()) {
    return false;
  }

  report.set_id(AttributionReport::Id(db_.GetLastInsertRowId()));
  return true;
}

AggregatableResult
AttributionStorageSql::MaybeStoreAggregatableAttributionReportData(
    AttributionReport& report,
    int64_t aggregatable_budget_consumed,
    int num_aggregatable_reports,
    absl::optional<uint64_t> dedup_key,
    absl::optional<int64_t>& aggregatable_budget_per_source,
    absl::optional<int>& max_aggregatable_reports_per_source) {
  const auto* aggregatable_attribution =
      absl::get_if<AttributionReport::AggregatableAttributionData>(
          &report.data());
  DCHECK(aggregatable_attribution);

  if (num_aggregatable_reports >=
      delegate_->GetMaxAggregatableReportsPerSource()) {
    max_aggregatable_reports_per_source =
        delegate_->GetMaxAggregatableReportsPerSource();
    return AggregatableResult::kExcessiveReports;
  }

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

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return AggregatableResult::kInternalError;
  }

  StoredSource::Id source_id = aggregatable_attribution->source.source_id();

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

bool AttributionStorageSql::GenerateNullAggregatableReportsAndStoreReports(
    const AttributionTrigger& trigger,
    const AttributionInfo& attribution_info,
    absl::optional<AttributionReport>& new_aggregatable_report,
    absl::optional<base::Time>& min_null_aggregatable_report_time) {
  absl::optional<base::Time> attributed_source_time;
  std::vector<AttributionReport> reports;

  if (new_aggregatable_report) {
    const auto* data =
        absl::get_if<AttributionReport::AggregatableAttributionData>(
            &new_aggregatable_report->data());
    DCHECK(data);
    attributed_source_time = data->source.source_time();

    reports.push_back(std::move(*new_aggregatable_report));
    new_aggregatable_report.reset();
  }

  if (HasAggregatableData(trigger.registration())) {
    std::vector<AttributionStorageDelegate::NullAggregatableReport>
        null_aggregatable_reports = delegate_->GetNullAggregatableReports(
            trigger, attribution_info.time, attributed_source_time);
    for (const auto& null_aggregatable_report : null_aggregatable_reports) {
      base::Time report_time =
          delegate_->GetAggregatableReportTime(attribution_info.time);
      min_null_aggregatable_report_time = AttributionReport::MinReportTime(
          min_null_aggregatable_report_time, report_time);
      reports.emplace_back(
          attribution_info, AttributionReport::Id(kUnsetRecordId), report_time,
          /*initial_report_time=*/report_time, delegate_->NewReportID(),
          /*failed_send_attempts=*/0,
          AttributionReport::NullAggregatableData(
              AttributionReport::CommonAggregatableData(
                  trigger.registration().aggregation_coordinator_origin,
                  /*verification_token=*/absl::nullopt,
                  trigger.registration().source_registration_time_config),
              trigger.reporting_origin(),
              null_aggregatable_report.fake_source_time));
    }
  }

  if (reports.empty()) {
    return true;
  }

  AssignTriggerVerificationData(reports, trigger);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  for (AttributionReport& report : reports) {
    if (!StoreAttributionReport(report)) {
      return false;
    }

    if (report.GetReportType() ==
        AttributionReport::Type::kAggregatableAttribution) {
      DCHECK(!new_aggregatable_report.has_value());
      new_aggregatable_report = std::move(report);
    }
  }

  return transaction.Commit();
}

void AttributionStorageSql::AssignTriggerVerificationData(
    std::vector<AttributionReport>& reports,
    const AttributionTrigger& trigger) {
  DCHECK(!reports.empty());

  // TODO(https://crbug.com/1442578): Add metric to understand the number of
  // reports sent with a verification token.

  if (trigger.verifications().empty()) {
    return;
  }

  // Assign verification tokens according to:
  // https://wicg.github.io/attribution-reporting-api/#assign-private-state-tokens
  delegate_->ShuffleReports(reports);

  std::vector<network::TriggerVerification> verifications =
      trigger.verifications();
  delegate_->ShuffleTriggerVerifications(verifications);

  for (size_t i = 0; i < verifications.size() && i < reports.size(); ++i) {
    network::TriggerVerification& verification = verifications.at(i);
    AttributionReport& report = reports.at(i);

    report.set_external_report_id(
        std::move(verification.aggregatable_report_id()));
    absl::visit(
        base::Overloaded{
            [](const AttributionReport::EventLevelData&) { NOTREACHED(); },
            [&](AttributionReport::AggregatableAttributionData& data) {
              data.common_data.verification_token =
                  std::move(verification.token());
            },
            [&](AttributionReport::NullAggregatableData& data) {
              data.common_data.verification_token =
                  std::move(verification.token());
            }},
        report.data());
  }
}

std::set<AttributionDataModel::DataKey>
AttributionStorageSql::GetAllDataKeys() {
  // We don't bother creating the DB here if it doesn't exist, because it's not
  // possible for there to be any data to return if there's no DB
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return {};
  }

  std::set<AttributionDataModel::DataKey> keys;

  const auto get_data_keys = [&](sql::Statement& statement) {
    while (statement.Step()) {
      url::Origin reporting_origin =
          DeserializeOrigin(statement.ColumnString(0));
      if (reporting_origin.opaque()) {
        continue;
      }
      keys.emplace(std::move(reporting_origin));
    }
  };

  sql::Statement sources_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetSourcesDataKeysSql));
  get_data_keys(sources_statement);

  sql::Statement null_reports_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetNullReportsDataKeysSql));
  get_data_keys(null_reports_statement);

  rate_limit_table_.AppendRateLimitDataKeys(&db_, keys);
  return keys;
}

void AttributionStorageSql::DeleteByDataKey(
    const AttributionDataModel::DataKey& key) {
  ClearData(base::Time::Min(), base::Time::Max(),
            base::BindRepeating(
                std::equal_to<blink::StorageKey>(),
                blink::StorageKey::CreateFirstParty(key.reporting_origin())),
            /*delete_rate_limit_data=*/true);
}

void AttributionStorageSql::SetDelegate(
    std::unique_ptr<AttributionStorageDelegate> delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate);
  rate_limit_table_.SetDelegate(*delegate);
  delegate_ = std::move(delegate);
}

}  // namespace content
