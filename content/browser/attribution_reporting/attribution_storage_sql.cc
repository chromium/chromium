// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_sql.h"

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
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
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/uuid.h"
#include "components/attribution_reporting/aggregatable_dedup_key.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/aggregatable_utils.h"
#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/features.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/privacy_math.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_time_config.mojom-forward.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_data_matching.mojom.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/aggregatable_attribution_utils.h"
#include "content/browser/attribution_reporting/aggregatable_debug_rate_limit_table.h"
#include "content/browser/attribution_reporting/aggregatable_debug_report.h"
#include "content/browser/attribution_reporting/attribution_features.h"
#include "content/browser/attribution_reporting/attribution_info.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/attribution_resolver_delegate.h"
#include "content/browser/attribution_reporting/attribution_storage_sql_migrations.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
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
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/statement_id.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

using AggregatableResult = ::content::AttributionTrigger::AggregatableResult;
using EventLevelResult = ::content::AttributionTrigger::EventLevelResult;

using ::attribution_reporting::AggregatableTriggerConfig;
using ::attribution_reporting::AttributionScopesData;
using ::attribution_reporting::EventReportWindows;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::SourceType;
using ::attribution_reporting::mojom::TriggerDataMatching;

const base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("Conversions");

constexpr int64_t kUnsetRecordId = -1;

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

std::optional<StoredSource::AttributionLogic> DeserializeAttributionLogic(
    int val) {
  switch (val) {
    case static_cast<int>(StoredSource::AttributionLogic::kNever):
      return StoredSource::AttributionLogic::kNever;
    case static_cast<int>(StoredSource::AttributionLogic::kTruthfully):
      return StoredSource::AttributionLogic::kTruthfully;
    case static_cast<int>(StoredSource::AttributionLogic::kFalsely):
      return StoredSource::AttributionLogic::kFalsely;
    default:
      return std::nullopt;
  }
}

int SerializeSourceType(SourceType val) {
  return static_cast<int>(val);
}

int SerializeReportType(AttributionReport::Type val) {
  return static_cast<int>(val);
}

std::optional<AttributionReport::Type> DeserializeReportType(int val) {
  switch (val) {
    case static_cast<int>(AttributionReport::Type::kEventLevel):
      return AttributionReport::Type::kEventLevel;
    case static_cast<int>(AttributionReport::Type::kAggregatableAttribution):
      return AttributionReport::Type::kAggregatableAttribution;
    case static_cast<int>(AttributionReport::Type::kNullAggregatable):
      return AttributionReport::Type::kNullAggregatable;
    default:
      return std::nullopt;
  }
}

std::optional<StoredSource::ActiveState> GetSourceActiveState(
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
  return std::nullopt;
}

void BindUint64OrNull(sql::Statement& statement,
                      int col,
                      std::optional<uint64_t> value) {
  if (value.has_value()) {
    statement.BindInt64(col, SerializeUint64(*value));
  } else {
    statement.BindNull(col);
  }
}

std::optional<uint64_t> ColumnUint64OrNull(sql::Statement& statement, int col) {
  return statement.GetColumnType(col) == sql::ColumnType::kNull
             ? std::nullopt
             : std::make_optional(
                   DeserializeUint64(statement.ColumnInt64(col)));
}

constexpr int kSourceColumnCount = 21;

int64_t GetStorageFileSizeKB(const base::FilePath& path_to_database) {
  int64_t file_size = -1;
  if (!path_to_database.empty() &&
      base::GetFileSize(path_to_database, &file_size)) {
    file_size = file_size / 1024;
  }
  return file_size;
}

}  // namespace

struct AttributionStorageSql::ReportCorruptionStatusSetAndIds {
  ReportCorruptionStatusSet status_set;
  absl::variant<absl::monostate, StoredSource::Id, AttributionReport::Id>
      source_or_report_id;

  ReportCorruptionStatusSetAndIds(
      ReportCorruptionStatusSet set,
      absl::variant<absl::monostate, StoredSource::Id, AttributionReport::Id>
          id)
      : status_set(set), source_or_report_id(id) {}
};

// Helper to deserialize source rows. See `GetActiveSources()` for the
// expected ordering of columns used for the input to this function.
base::expected<AttributionStorageSql::StoredSourceData,
               AttributionStorageSql::ReportCorruptionStatusSetAndIds>
AttributionStorageSql::ReadSourceFromStatement(sql::Statement& statement) {
  DCHECK_GE(statement.ColumnCount(), kSourceColumnCount);

  int col = 0;

  if (statement.GetColumnType(col) == sql::ColumnType::kNull) {
    return base::unexpected(ReportCorruptionStatusSetAndIds(
        ReportCorruptionStatusSet{ReportCorruptionStatus::kSourceNotFound},
        absl::monostate()));
  }

  StoredSource::Id source_id(statement.ColumnInt64(col++));
  uint64_t source_event_id = DeserializeUint64(statement.ColumnInt64(col++));
  std::optional<SuitableOrigin> source_origin =
      SuitableOrigin::Deserialize(statement.ColumnStringView(col++));
  std::optional<SuitableOrigin> reporting_origin =
      SuitableOrigin::Deserialize(statement.ColumnStringView(col++));
  base::Time source_time = statement.ColumnTime(col++);
  base::Time expiry_time = statement.ColumnTime(col++);
  base::Time aggregatable_report_window_time = statement.ColumnTime(col++);
  std::optional<SourceType> source_type =
      DeserializeSourceType(statement.ColumnInt(col++));
  std::optional<StoredSource::AttributionLogic> attribution_logic =
      DeserializeAttributionLogic(statement.ColumnInt(col++));
  int64_t priority = statement.ColumnInt64(col++);
  std::optional<uint64_t> debug_key = ColumnUint64OrNull(statement, col++);
  int num_attributions = statement.ColumnInt(col++);
  int remaining_aggregatable_attribution_budget = statement.ColumnInt(col++);
  int num_aggregatable_attribution_reports = statement.ColumnInt(col++);
  int remaining_aggregatable_debug_budget = statement.ColumnInt(col++);
  std::optional<attribution_reporting::AggregationKeys> aggregation_keys =
      DeserializeAggregationKeys(statement, col++);

  ReportCorruptionStatusSet corruption_causes;

  if (!source_origin) {
    corruption_causes.Put(ReportCorruptionStatus::kSourceInvalidSourceOrigin);
  }

  if (!reporting_origin) {
    corruption_causes.Put(
        ReportCorruptionStatus::kSourceInvalidReportingOrigin);
  }

  if (!source_type.has_value()) {
    corruption_causes.Put(ReportCorruptionStatus::kSourceInvalidSourceType);
  }

  if (!attribution_logic.has_value()) {
    corruption_causes.Put(
        ReportCorruptionStatus::kSourceInvalidAttributionLogic);
  }

  if (num_attributions < 0) {
    corruption_causes.Put(ReportCorruptionStatus::kSourceInvalidNumConversions);
  }

  if (num_aggregatable_attribution_reports < 0) {
    corruption_causes.Put(
        ReportCorruptionStatus::kSourceInvalidNumAggregatableReports);
  }

  if (!aggregation_keys.has_value()) {
    corruption_causes.Put(
        ReportCorruptionStatus::kSourceInvalidAggregationKeys);
  }

  std::optional<attribution_reporting::FilterData> filter_data =
      DeserializeFilterData(statement, col++);
  if (!filter_data) {
    corruption_causes.Put(ReportCorruptionStatus::kSourceInvalidFilterData);
  }

  base::expected<std::optional<AttributionScopesData>, absl::monostate>
      attribution_scopes_data =
          DeserializeAttributionScopesData(statement, col++);
  if (!attribution_scopes_data.has_value()) {
    corruption_causes.Put(
        ReportCorruptionStatus::kSourceInvalidAttributionScopesData);
  }

  bool event_level_active = statement.ColumnBool(col++);
  bool aggregatable_active = statement.ColumnBool(col++);
  std::optional<StoredSource::ActiveState> active_state =
      GetSourceActiveState(event_level_active, aggregatable_active);
  if (!active_state.has_value()) {
    corruption_causes.Put(ReportCorruptionStatus::kSourceInvalidActiveState);
  }

  attribution_reporting::MaxEventLevelReports max_event_level_reports;
  std::optional<attribution_reporting::TriggerSpecs> trigger_specs;
  attribution_reporting::EventLevelEpsilon event_level_epsilon;

  std::optional<proto::AttributionReadOnlySourceData>
      read_only_source_data_msg =
          DeserializeReadOnlySourceDataAsProto(statement, col++);
  if (!read_only_source_data_msg.has_value()) {
    corruption_causes.Put(
        ReportCorruptionStatus::kSourceInvalidReadOnlySourceData);
  } else {
    if (!max_event_level_reports.SetIfValid(
            read_only_source_data_msg->max_event_level_reports())) {
      corruption_causes.Put(
          ReportCorruptionStatus::kSourceInvalidMaxEventLevelReports);
    }

    if (source_type.has_value()) {
      trigger_specs = DeserializeTriggerSpecs(
          *read_only_source_data_msg, *source_type, max_event_level_reports);
      if (!trigger_specs.has_value()) {
        corruption_causes.Put(
            ReportCorruptionStatus::kSourceInvalidTriggerSpecs);
      }
    }

    if (read_only_source_data_msg->has_event_level_epsilon() &&
        !event_level_epsilon.SetIfValid(
            read_only_source_data_msg->event_level_epsilon())) {
      corruption_causes.Put(
          ReportCorruptionStatus::kSourceInvalidEventLevelEpsilon);
    }
  }

  static constexpr char kDestinationSitesSql[] =
      "SELECT destination_site "
      "FROM source_destinations "
      "WHERE source_id=?";
  sql::Statement destination_sites_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDestinationSitesSql));
  destination_sites_statement.BindInt64(0, *source_id);

  std::vector<net::SchemefulSite> destination_sites;
  while (destination_sites_statement.Step()) {
    auto destination_site = net::SchemefulSite::Deserialize(
        destination_sites_statement.ColumnStringView(0));
    destination_sites.push_back(std::move(destination_site));
  }
  if (!destination_sites_statement.Succeeded()) {
    corruption_causes.Put(
        ReportCorruptionStatus::kSourceDestinationSitesQueryFailed);
  }

  auto destination_set = attribution_reporting::DestinationSet::Create(
      std::move(destination_sites));
  if (!destination_set.has_value()) {
    corruption_causes.Put(
        ReportCorruptionStatus::kSourceInvalidDestinationSites);
  }

  std::vector<uint64_t> event_level_dedup_keys;
  std::vector<uint64_t> aggregatable_dedup_keys;
  if (!ReadDedupKeys(source_id, event_level_dedup_keys,
                     aggregatable_dedup_keys)) {
    corruption_causes.Put(ReportCorruptionStatus::kSourceDedupKeyQueryFailed);
  }

  if (!corruption_causes.empty()) {
    return base::unexpected(
        ReportCorruptionStatusSetAndIds(corruption_causes, source_id));
  }

  TriggerDataMatching trigger_data_matching;
  switch (read_only_source_data_msg->trigger_data_matching()) {
    case proto::AttributionReadOnlySourceData::EXACT:
      trigger_data_matching = TriggerDataMatching::kExact;
      break;
    case proto::AttributionReadOnlySourceData::MODULUS:
      trigger_data_matching = TriggerDataMatching::kModulus;
      break;
  }
  // If "debug_cookie_set" field was not set in earlier versions, set the
  // value to whether the debug key was set for the source.
  bool debug_cookie_set = read_only_source_data_msg->has_debug_cookie_set()
                              ? read_only_source_data_msg->debug_cookie_set()
                              : debug_key.has_value();

  absl::uint128 aggregatable_debug_key_piece = absl::MakeUint128(
      read_only_source_data_msg->aggregatable_debug_key_piece().high_bits(),
      read_only_source_data_msg->aggregatable_debug_key_piece().low_bits());

  std::optional<double> randomized_response_rate =
      read_only_source_data_msg->has_randomized_response_rate()
          ? read_only_source_data_msg->randomized_response_rate()
          : delegate_->GetRandomizedResponseRate(*trigger_specs,
                                                 event_level_epsilon);
  if (!randomized_response_rate.has_value()) {
    return base::unexpected(ReportCorruptionStatusSetAndIds(
        ReportCorruptionStatusSet{
            ReportCorruptionStatus::kSourceInvalidRandomizedResponseRate},
        source_id));
  }

  std::optional<StoredSource> stored_source = StoredSource::Create(
      CommonSourceInfo(*std::move(source_origin), *std::move(reporting_origin),
                       *source_type, debug_cookie_set),
      source_event_id, *std::move(destination_set), source_time, expiry_time,
      *std::move(trigger_specs), aggregatable_report_window_time, priority,
      *std::move(filter_data), debug_key, *std::move(aggregation_keys),
      *attribution_logic, *active_state, source_id,
      remaining_aggregatable_attribution_budget, *randomized_response_rate,
      trigger_data_matching, event_level_epsilon, aggregatable_debug_key_piece,
      remaining_aggregatable_debug_budget, *std::move(attribution_scopes_data));
  if (!stored_source.has_value()) {
    // TODO(crbug.com/40287459): Consider enumerating errors from StoredSource.
    return base::unexpected(ReportCorruptionStatusSetAndIds(
        ReportCorruptionStatusSet{
            ReportCorruptionStatus::kStoredSourceConstructionFailed},
        source_id));
  }

  stored_source->dedup_keys() = std::move(event_level_dedup_keys);
  stored_source->aggregatable_dedup_keys() = std::move(aggregatable_dedup_keys);

  return StoredSourceData{.source = *std::move(stored_source),
                          .num_attributions = num_attributions,
                          .num_aggregatable_attribution_reports =
                              num_aggregatable_attribution_reports};
}

std::optional<AttributionStorageSql::StoredSourceData>
AttributionStorageSql::ReadSourceToAttribute(StoredSource::Id source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kReadSourceToAttributeSql));
  statement.BindInt64(0, *source_id);
  if (!statement.Step()) {
    return std::nullopt;
  }

  auto source = ReadSourceFromStatement(statement);
  return source.has_value() ? std::make_optional(*std::move(source))
                            : std::nullopt;
}

namespace {

base::FilePath DatabasePath(const base::FilePath& user_data_directory) {
  return user_data_directory.Append(kDatabasePath);
}

void AssignSourceForDeactivationOrDeletion(
    StoredSource::Id source_id,
    bool has_reports,
    std::vector<StoredSource::Id>& source_ids_to_delete,
    std::vector<StoredSource::Id>& source_ids_to_deactivate) {
  if (has_reports) {
    source_ids_to_deactivate.push_back(source_id);
  } else {
    source_ids_to_delete.push_back(source_id);
  }
}

}  // namespace

// static
std::unique_ptr<AttributionStorageSql::Transaction>
AttributionStorageSql::Transaction::CreateAndStart(sql::Database& db) {
  std::unique_ptr<Transaction> transaction(new Transaction(db));

  return transaction->transaction_.Begin() ? std::move(transaction) : nullptr;
}

AttributionStorageSql::Transaction::Transaction(sql::Database& db)
    : transaction_(&db) {}

AttributionStorageSql::Transaction::~Transaction() = default;

bool AttributionStorageSql::Transaction::Commit() {
  return transaction_.Commit();
}

AttributionStorageSql::AttributionStorageSql(
    const base::FilePath& user_data_directory,
    AttributionResolverDelegate* delegate)
    : path_to_database_(user_data_directory.empty()
                            ? base::FilePath()
                            : DatabasePath(user_data_directory)),
      db_(sql::DatabaseOptions{.page_size = 4096, .cache_size = 32}),
      delegate_(delegate),
      rate_limit_table_(delegate_),
      aggregatable_debug_rate_limit_table_(delegate_) {
  DCHECK(delegate_);

  db_.set_histogram_tag("Conversions");
}

AttributionStorageSql::~AttributionStorageSql() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<AttributionStorageSql::Transaction>
AttributionStorageSql::StartTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return nullptr;
  }
  return Transaction::CreateAndStart(db_);
}

bool AttributionStorageSql::DeactivateSources(
    base::span<const StoredSource::Id> sources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

bool AttributionStorageSql::DeactivateSourcesForDestinationLimit(
    base::span<const StoredSource::Id> sources,
    base::Time now) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sources.empty()) {
    return true;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  if (!DeactivateSources(sources)) {
    return false;
  }

  if (!rate_limit_table_.DeactivateSourcesForDestinationLimit(&db_, sources)) {
    return false;
  }

  if (!DeleteEventLevelReportsTriggeredLaterThanForSources(sources, now)) {
    return false;
  }

  sql::Statement delete_aggregatable_reports_statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      attribution_queries::kDeleteAggregatableReportsForDestinationLimitSql));

  for (StoredSource::Id id : sources) {
    delete_aggregatable_reports_statement.Reset(/*clear_bound_vars=*/true);
    delete_aggregatable_reports_statement.BindInt64(0, *id);
    while (delete_aggregatable_reports_statement.Step()) {
      AttributionReport::Id report_id(
          delete_aggregatable_reports_statement.ColumnInt64(0));
      if (!rate_limit_table_.DeleteAttributionRateLimit(
              &db_, RateLimitTable::Scope::kAggregatableAttribution,
              report_id)) {
        return false;
      }
    }
    if (!delete_aggregatable_reports_statement.Succeeded()) {
      return false;
    }
  }

  return transaction.Commit();
}

std::optional<StoredSource> AttributionStorageSql::InsertSource(
    const StorableSource& source,
    base::Time source_time,
    int num_attributions,
    bool event_level_active,
    double randomized_response_rate,
    StoredSource::AttributionLogic attribution_logic,
    base::Time aggregatable_report_window_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const CommonSourceInfo& common_info = source.common_info();
  const attribution_reporting::SourceRegistration& reg = source.registration();

  // Aggregatable reports are not subject to `attribution_logic`.
  const bool aggregatable_active = true;

  const base::Time expiry_time = source_time + reg.expiry;

  static constexpr char kInsertImpressionSql[] =
      "INSERT INTO sources"
      "(source_event_id,source_origin,"
      "reporting_origin,source_time,"
      "expiry_time,aggregatable_report_window_time,"
      "source_type,attribution_logic,priority,source_site,"
      "num_attributions,event_level_active,aggregatable_active,debug_key,"
      "remaining_aggregatable_attribution_budget,"
      "num_aggregatable_attribution_reports,"
      "aggregatable_source,filter_data,read_only_source_data,"
      "remaining_aggregatable_debug_budget,num_aggregatable_debug_reports,"
      "attribution_scopes_data)"
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,0,?,?,?,?,0,?)";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertImpressionSql));
  statement.BindInt64(0, SerializeUint64(reg.source_event_id));
  statement.BindString(1, common_info.source_origin().Serialize());
  statement.BindString(2, common_info.reporting_origin().Serialize());
  statement.BindTime(3, source_time);
  statement.BindTime(4, expiry_time);
  statement.BindTime(5, aggregatable_report_window_time);
  statement.BindInt(6, SerializeSourceType(common_info.source_type()));
  statement.BindInt(7, SerializeAttributionLogic(attribution_logic));
  statement.BindInt64(8, reg.priority);
  statement.BindString(9, common_info.source_site().Serialize());
  statement.BindInt(10, num_attributions);
  statement.BindBool(11, event_level_active);
  statement.BindBool(12, aggregatable_active);

  BindUint64OrNull(statement, 13, reg.debug_key);

  std::optional<StoredSource::ActiveState> active_state =
      GetSourceActiveState(event_level_active, aggregatable_active);
  DCHECK(active_state.has_value());

  const int remaining_aggregatable_debug_budget =
      reg.aggregatable_debug_reporting_config.budget();
  const int remaining_aggregatable_attribution_budget =
      attribution_reporting::kMaxAggregatableValue -
      remaining_aggregatable_debug_budget;

  statement.BindInt(14, remaining_aggregatable_attribution_budget);
  statement.BindBlob(15, SerializeAggregationKeys(reg.aggregation_keys));
  statement.BindBlob(16, SerializeFilterData(reg.filter_data));
  statement.BindBlob(
      17, SerializeReadOnlySourceData(
              reg.trigger_specs, randomized_response_rate,
              reg.trigger_data_matching, common_info.debug_cookie_set(),
              reg.aggregatable_debug_reporting_config.config().key_piece));
  statement.BindInt(18, remaining_aggregatable_debug_budget);

  if (reg.attribution_scopes_data.has_value()) {
    statement.BindBlob(19, SerializeAttributionScopesData(
                               reg.attribution_scopes_data.value()));
  } else {
    statement.BindNull(19);
  }

  if (!statement.Run()) {
    return std::nullopt;
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
      return std::nullopt;
    }
  }

  // TODO(apaseltiner): Avoid as many of these copies as possible, since the
  // `StoredSource` is only used within this method.
  return StoredSource::Create(
      source.common_info(), reg.source_event_id, reg.destination_set,
      source_time, expiry_time, reg.trigger_specs,
      aggregatable_report_window_time, reg.priority, reg.filter_data,
      reg.debug_key, reg.aggregation_keys, attribution_logic, *active_state,
      source_id, remaining_aggregatable_attribution_budget,
      randomized_response_rate, reg.trigger_data_matching,
      reg.event_level_epsilon,
      reg.aggregatable_debug_reporting_config.config().key_piece,
      remaining_aggregatable_debug_budget, reg.attribution_scopes_data);
}

base::expected<std::optional<AttributionStorageSql::ReportIdAndPriority>,
               AttributionStorageSql::Error>
AttributionStorageSql::GetReportWithMinPriority(StoredSource::Id source_id,
                                                base::Time report_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Prioritization is scoped within report windows.
  // This is reasonably optimized as is because we only store a ~small number
  // of reports per source_id. Selects the report with lowest priority,
  // and uses the greatest rowid to break ties. This favors sending
  // reports for report closer to the source time. report_id is used instead of
  // trigger time because the former is strictly increasing while the latter is
  // subject to clock adjustments. This property is only guaranteed because of
  // the use of AUTOINCREMENT on the report_id column, which prevents reuse upon
  // row deletion.
  sql::Statement min_priority_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kMinPrioritySql));
  min_priority_statement.BindInt64(0, *source_id);
  min_priority_statement.BindTime(1, report_time);

  std::optional<AttributionReport::Id> conversion_id_with_min_priority;
  int64_t min_priority;

  while (min_priority_statement.Step()) {
    std::optional<int64_t> priority =
        DeserializeEventLevelPriority(min_priority_statement.ColumnBlob(0));
    if (!priority.has_value()) {
      continue;
    }

    AttributionReport::Id report_id(min_priority_statement.ColumnInt64(1));

    if (!conversion_id_with_min_priority.has_value() ||
        *priority < min_priority ||
        (*priority == min_priority &&
         report_id > *conversion_id_with_min_priority)) {
      conversion_id_with_min_priority = report_id;
      min_priority = *priority;
    }
  }

  if (!min_priority_statement.Succeeded()) {
    return base::unexpected(Error());
  }

  if (!conversion_id_with_min_priority.has_value()) {
    return std::nullopt;
  }

  return ReportIdAndPriority{
      .id = *conversion_id_with_min_priority,
      .priority = min_priority,
  };
}

bool AttributionStorageSql::DeactivateSourceAtEventLevel(StoredSource::Id id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kDeactivateSql[] =
      "UPDATE sources SET event_level_active=0 WHERE source_id=?";
  sql::Statement deactivate_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeactivateSql));
  deactivate_statement.BindInt64(0, *id);
  return deactivate_statement.Run();
}

bool AttributionStorageSql::RemoveScopesDataForSource(
    StoredSource::Id source_id) {
  static constexpr char kUpdateScopeFields[] =
      "UPDATE sources SET attribution_scopes_data=NULL WHERE source_id=?";
  sql::Statement set_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kUpdateScopeFields));
  set_statement.BindInt64(0, *source_id);
  return set_statement.Run();
}

namespace {

// At least 1 destination must always be bound. When there are fewer than 3,
// NULL is used as a placeholder . `A IN (B,C,D)` evaluates to true iff A=B or
// A=C or A=D, since A is always non-NULL. Otherwise, it evaluates to NULL if
// any of B, C, or D is NULL, but that is fine, as the JOIN on the destination
// is only satisfied when the ON clause evaluates to true.
//
// https://www.sqlite.org/lang_expr.html#the_in_and_not_in_operators
void PrepareGetMatchingSourcesStatement(
    sql::Statement& stmt,
    base::span<const net::SchemefulSite> destinations) {
  CHECK_GE(destinations.size(), 1u);
  CHECK_LE(destinations.size(), 3u);

  int col = 2;
  stmt.BindString(col++, destinations[0].Serialize());

  if (destinations.size() >= 2) {
    stmt.BindString(col++, destinations[1].Serialize());
  } else {
    stmt.BindNull(col++);
  }

  if (destinations.size() == 3) {
    stmt.BindString(col++, destinations[2].Serialize());
  } else {
    stmt.BindNull(col++);
  }
}

}  // namespace

bool AttributionStorageSql::UpdateOrRemoveSourcesWithIncompatibleScopeFields(
    const StorableSource& pending_source,
    base::Time source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  const auto& registration = pending_source.registration();

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetMatchingSourcesSql));
  statement.BindString(
      0, pending_source.common_info().reporting_origin().Serialize());
  statement.BindTime(1, source_time);

  PrepareGetMatchingSourcesStatement(
      statement, registration.destination_set.destinations());

  const std::optional<AttributionScopesData>& pending_scopes =
      registration.attribution_scopes_data;

  std::vector<StoredSource::Id> source_ids_to_delete;
  std::vector<StoredSource::Id> source_ids_to_deactivate;
  while (statement.Step()) {
    // Note: This causes a single corrupt source to fail every
    // `AttributionResolverImpl::StoreSource()` operation with a matching
    // reporting origin and destination until that source expires. We should
    // address this in a consistent way with the rest of our corruption
    // handling.
    ASSIGN_OR_RETURN(std::optional<AttributionScopesData> existing_scopes,
                     DeserializeAttributionScopesData(statement, 3),
                     [](absl::monostate) { return false; });

    StoredSource::Id source_id(statement.ColumnInt64(1));

    if (!pending_scopes.has_value()) {
      if (existing_scopes.has_value() &&
          !RemoveScopesDataForSource(source_id)) {
        return false;
      }
    } else if (!existing_scopes.has_value() ||
               existing_scopes->max_event_states() !=
                   pending_scopes->max_event_states() ||
               existing_scopes->attribution_scope_limit() <
                   pending_scopes->attribution_scope_limit()) {
      AssignSourceForDeactivationOrDeletion(
          source_id, /*has_reports=*/statement.ColumnBool(2),
          source_ids_to_delete, source_ids_to_deactivate);
    }
  }

  return statement.Succeeded() &&
         DeleteEventLevelReportsTriggeredLaterThanForSources(
             source_ids_to_deactivate, source_time) &&
         DeleteSources(source_ids_to_delete) &&
         DeactivateSources(source_ids_to_deactivate) && transaction.Commit();
}

namespace {

struct ScopeData {
  base::Time max_source_time = base::Time::Min();
  std::vector<StoredSource::Id> sources_without_reports;
  std::vector<StoredSource::Id> sources_with_reports;

  ScopeData() = default;

  ScopeData(const ScopeData&) = delete;
  ScopeData& operator=(const ScopeData&) = delete;

  ScopeData(ScopeData&&) = default;
  ScopeData& operator=(ScopeData&&) = default;

  void Assign(std::vector<StoredSource::Id>& source_ids_to_delete,
              std::vector<StoredSource::Id>& source_ids_to_deactivate) && {
    if (source_ids_to_delete.empty()) {
      source_ids_to_delete = std::move(sources_without_reports);
    } else {
      source_ids_to_delete.insert(source_ids_to_delete.end(),
                                  sources_without_reports.begin(),
                                  sources_without_reports.end());
    }

    if (source_ids_to_deactivate.empty()) {
      source_ids_to_deactivate = std::move(sources_with_reports);
    } else {
      source_ids_to_deactivate.insert(source_ids_to_deactivate.end(),
                                      sources_with_reports.begin(),
                                      sources_with_reports.end());
    }
  }
};

using ScopeDataMap = std::map<std::string, ScopeData>;

// Assigns sources for all but the top `remaining_scopes_allowed` scopes to be
// deleted or deactivated.
void SelectScopes(ScopeDataMap scope_datas,
                  size_t remaining_scopes_allowed,
                  std::vector<StoredSource::Id>& source_ids_to_delete,
                  std::vector<StoredSource::Id>& source_ids_to_deactivate) {
  CHECK_GT(scope_datas.size(), remaining_scopes_allowed);

  // It can be more efficient to find the bottom scopes than the top.
  size_t to_select;
  bool keep_selected;
  if (size_t diff = scope_datas.size() - remaining_scopes_allowed;
      remaining_scopes_allowed < diff) {
    to_select = remaining_scopes_allowed;
    keep_selected = true;
  } else {
    to_select = diff;
    keep_selected = false;
  }

  const auto cmp = [keep_selected](const ScopeDataMap::node_type& a,
                                   const ScopeDataMap::node_type& b) {
    return (std::tie(a.mapped().max_source_time, a.key()) >
            std::tie(b.mapped().max_source_time, b.key())) == keep_selected;
  };

  std::vector<ScopeDataMap::node_type> selected;
  selected.reserve(to_select);

  while (!scope_datas.empty() && selected.size() < to_select) {
    selected.emplace_back(scope_datas.extract(scope_datas.begin()));
  }

  base::ranges::make_heap(selected, cmp);

  while (!scope_datas.empty()) {
    auto scope = scope_datas.extract(scope_datas.begin());

    if (cmp(scope, selected.front())) {
      // Unfortunately, there is no existing function for replacing the top
      // of the heap, necessitating pop-then-push here.
      base::ranges::pop_heap(selected, cmp);
      std::swap(selected.back(), scope);
      base::ranges::push_heap(selected, cmp);
    }

    if (keep_selected) {
      std::move(scope.mapped())
          .Assign(source_ids_to_delete, source_ids_to_deactivate);
    }
  }

  if (!keep_selected) {
    for (auto& scope : selected) {
      std::move(scope.mapped())
          .Assign(source_ids_to_delete, source_ids_to_deactivate);
    }
  }

  DeduplicateSourceIds(source_ids_to_delete);
  DeduplicateSourceIds(source_ids_to_deactivate);
}

}  // namespace

// TODO(apaseltiner): This logic is very similar to that of
// `UpdateOrRemoveSourcesWithIncompatibleScopeFields()`. Can we deduplicate some
// of the logic?
bool AttributionStorageSql::RemoveSourcesWithOutdatedScopes(
    const StorableSource& source,
    base::Time source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& registration = source.registration();

  const std::optional<AttributionScopesData>& pending_scopes_data =
      registration.attribution_scopes_data;
  if (!pending_scopes_data.has_value()) {
    return true;
  }

  const size_t remaining_scopes_allowed =
      pending_scopes_data->attribution_scope_limit() -
      pending_scopes_data->attribution_scopes_set().scopes().size();

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetMatchingSourcesSql));
  statement.BindString(0, source.common_info().reporting_origin().Serialize());
  statement.BindTime(1, source_time);

  // TODO(apaseltiner): Can we make one matching-sources query instead of up to
  // 3 separate ones?
  for (const auto& destination : registration.destination_set.destinations()) {
    statement.Reset(/*clear_bound_vars=*/false);

    PrepareGetMatchingSourcesStatement(statement,
                                       base::span_from_ref(destination));

    ScopeDataMap scope_datas;
    std::vector<StoredSource::Id> source_ids_to_delete;
    std::vector<StoredSource::Id> source_ids_to_deactivate;

    while (statement.Step()) {
      // Note: This causes a single corrupt source to fail every
      // `AttributionResolverImpl::StoreSource()` operation with a matching
      // reporting origin and destination until that source expires. We should
      // address this in a consistent way with the rest of our corruption
      // handling.
      ASSIGN_OR_RETURN(std::optional<AttributionScopesData> scopes_data,
                       DeserializeAttributionScopesData(statement, 3),
                       [](absl::monostate) { return false; });
      if (!scopes_data.has_value()) {
        continue;
      }

      const StoredSource::Id source_id(statement.ColumnInt64(1));
      const bool has_reports = statement.ColumnBool(2);
      const base::Time this_source_time = statement.ColumnTime(4);

      base::flat_set<std::string> scopes =
          (*std::move(scopes_data)).TakeAttributionScopesSet().TakeScopes();

      for (std::string& scope : scopes) {
        // Reduce memory consumption when we already know that the scope is
        // selected.
        if (pending_scopes_data->attribution_scopes_set().scopes().contains(
                scope)) {
          continue;
        }

        if (remaining_scopes_allowed == 0) {
          AssignSourceForDeactivationOrDeletion(source_id, has_reports,
                                                source_ids_to_delete,
                                                source_ids_to_deactivate);
          // Ensure that each source is only assigned once, avoiding the need to
          // call `DeduplicateSourceIds()` below.
          break;
        }

        auto [scope_data, _] =
            scope_datas.try_emplace(std::move(scope), ScopeData());
        scope_data->second.max_source_time =
            std::max(scope_data->second.max_source_time, this_source_time);

        AssignSourceForDeactivationOrDeletion(
            source_id, has_reports,
            /*source_ids_to_delete=*/scope_data->second.sources_without_reports,
            /*source_ids_to_deactivate=*/
            scope_data->second.sources_with_reports);
      }
    }
    if (!statement.Succeeded()) {
      return false;
    }

    // It's only necessary to compute the top scopes when there are more scopes
    // than allowed.
    if (scope_datas.size() > remaining_scopes_allowed) {
      SelectScopes(std::move(scope_datas), remaining_scopes_allowed,
                   source_ids_to_delete, source_ids_to_deactivate);
    }

    if (!DeleteEventLevelReportsTriggeredLaterThanForSources(
            source_ids_to_deactivate, source_time) ||
        !DeleteSources(source_ids_to_delete) ||
        !DeactivateSources(source_ids_to_deactivate)) {
      return false;
    }
  }

  return transaction.Commit();
}

bool AttributionStorageSql::FindMatchingSourceForTrigger(
    const AttributionTrigger& trigger,
    base::Time trigger_time,
    std::optional<StoredSource::Id>& source_id_to_attribute,
    std::vector<StoredSource::Id>& source_ids_to_delete,
    std::vector<StoredSource::Id>& source_ids_to_deactivate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SCOPED_UMA_HISTOGRAM_TIMER("Conversions.Storage.FindMatchingSourceTime");

  // Get all sources that match this <reporting_origin,
  // conversion_destination> pair. Only get sources that are active and not
  // past their expiry time. The sources are fetched in order so that the
  // first one is the one that will be attributed; the others will be deleted or
  // deactivated, depending on whether they have ever been attributed.
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetMatchingSourcesSql));

  const net::SchemefulSite destination(trigger.destination_origin());
  PrepareGetMatchingSourcesStatement(statement,
                                     base::span_from_ref(destination));

  statement.BindString(0, trigger.reporting_origin().Serialize());
  statement.BindTime(1, trigger_time);

  // The highest-priority source with at least one matching scope will be
  // attributed. Any others will be deleted or deactivated based on whether they
  // have reports associated with them. If the trigger has no scopes, the
  // highest-priority source is selected. If no source has any matching scopes,
  // the trigger is dropped and sources stay active.

  struct MatchingSourceData {
    int64_t priority;
    StoredSource::Id id;
    bool has_reports;

    explicit MatchingSourceData(sql::Statement& stmt)
        : priority(stmt.ColumnInt64(0)),
          id(stmt.ColumnInt64(1)),
          has_reports(stmt.ColumnBool(2)) {}

    bool operator>(const MatchingSourceData& other) const {
      return std::make_pair(priority, id) >
             std::make_pair(other.priority, other.id);
    }
  };

  const auto& trigger_scopes = trigger.registration().attribution_scopes;
  std::optional<MatchingSourceData> highest_prio_source;

  while (statement.Step()) {
    std::optional<MatchingSourceData> source(statement);

    bool matching_scopes = false;
    if (trigger_scopes.scopes().empty()) {
      matching_scopes = true;
    } else {
      ASSIGN_OR_RETURN(std::optional<AttributionScopesData> scopes_data,
                       DeserializeAttributionScopesData(statement, 3),
                       [](absl::monostate) { return false; });

      matching_scopes =
          scopes_data.has_value() &&
          trigger_scopes.HasIntersection(scopes_data->attribution_scopes_set());
    }

    if (matching_scopes && source > highest_prio_source) {
      highest_prio_source.swap(source);
    }

    if (source.has_value()) {
      AssignSourceForDeactivationOrDeletion(source->id, source->has_reports,
                                            source_ids_to_delete,
                                            source_ids_to_deactivate);
    }
  }

  if (!statement.Succeeded()) {
    return false;
  }

  if (highest_prio_source.has_value()) {
    source_id_to_attribute = highest_prio_source->id;
  } else {
    source_ids_to_delete.clear();
    source_ids_to_deactivate.clear();
  }
  return true;
}

bool AttributionStorageSql::IncrementNumAttributions(StoredSource::Id id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr char kUpdateImpressionForConversionSql[] =
      "UPDATE sources SET num_attributions=num_attributions+1 "
      "WHERE source_id=?";
  sql::Statement impression_update_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kUpdateImpressionForConversionSql));
  impression_update_statement.BindInt64(0, *id);
  return impression_update_statement.Run();
}

// Helper to deserialize report rows. See `GetReport()` for the expected
// ordering of columns used for the input to this function.
base::expected<AttributionReport,
               AttributionStorageSql::ReportCorruptionStatusSetAndIds>
AttributionStorageSql::ReadReportFromStatement(sql::Statement& statement) {
  DCHECK_EQ(statement.ColumnCount(), kSourceColumnCount + 11);

  int col = kSourceColumnCount;
  AttributionReport::Id report_id(statement.ColumnInt64(col++));
  base::Time trigger_time = statement.ColumnTime(col++);
  base::Time report_time = statement.ColumnTime(col++);
  base::Time initial_report_time = statement.ColumnTime(col++);
  int failed_send_attempts = statement.ColumnInt(col++);
  base::Uuid external_report_id =
      base::Uuid::ParseLowercase(statement.ColumnStringView(col++));
  std::optional<uint64_t> trigger_debug_key =
      ColumnUint64OrNull(statement, col++);
  auto context_origin =
      SuitableOrigin::Deserialize(statement.ColumnStringView(col++));
  auto reporting_origin =
      SuitableOrigin::Deserialize(statement.ColumnStringView(col++));
  std::optional<AttributionReport::Type> report_type =
      DeserializeReportType(statement.ColumnInt(col++));

  base::expected<StoredSourceData, ReportCorruptionStatusSetAndIds>
      source_data = ReadSourceFromStatement(statement);
  ReportCorruptionStatusSetAndIds corruptions =
      source_data.error_or(ReportCorruptionStatusSetAndIds({}, report_id));
  if (corruptions.status_set.Has(ReportCorruptionStatus::kSourceNotFound)) {
    corruptions.source_or_report_id = report_id;
  }

  // Ensure data is valid before continuing. This could happen if there is
  // database corruption.
  // TODO(apaseltiner): Should we raze the DB if we've detected corruption?
  //
  // TODO(apaseltiner): Consider verifying that `context_origin` is valid for
  // the associated source.

  if (failed_send_attempts < 0) {
    corruptions.status_set.Put(
        ReportCorruptionStatus::kInvalidFailedSendAttempts);
  }

  if (!external_report_id.is_valid()) {
    corruptions.status_set.Put(
        ReportCorruptionStatus::kInvalidExternalReportID);
  }

  if (!context_origin.has_value()) {
    corruptions.status_set.Put(ReportCorruptionStatus::kInvalidContextOrigin);
  }

  if (!reporting_origin.has_value()) {
    corruptions.status_set.Put(ReportCorruptionStatus::kInvalidReportingOrigin);
  } else if (source_data.has_value() &&
             *source_data->source.common_info().reporting_origin() !=
                 *reporting_origin) {
    corruptions.status_set.Put(
        ReportCorruptionStatus::kReportingOriginMismatch);
  }

  if (!report_type.has_value()) {
    corruptions.status_set.Put(ReportCorruptionStatus::kInvalidReportType);
    corruptions.status_set.Put(ReportCorruptionStatus::kAnyFieldCorrupted);
    return base::unexpected(std::move(corruptions));
  }

  std::optional<AttributionReport::Data> data;

  switch (base::span<const uint8_t> metadata = statement.ColumnBlob(col++);
          *report_type) {
    case AttributionReport::Type::kEventLevel: {
      if (!source_data.has_value()) {
        corruptions.status_set.Put(
            ReportCorruptionStatus::kSourceDataMissingEventLevel);
        break;
      }
      data = DeserializeEventLevelReportMetadata(metadata, source_data->source);
      if (!data.has_value()) {
        corruptions.status_set.Put(ReportCorruptionStatus::kInvalidMetadata);
      }
      break;
    }
    case AttributionReport::Type::kAggregatableAttribution: {
      if (!source_data.has_value()) {
        corruptions.status_set.Put(
            ReportCorruptionStatus::kSourceDataMissingAggregatable);
        break;
      }
      data =
          DeserializeAggregatableReportMetadata(metadata, source_data->source);
      if (!data.has_value()) {
        corruptions.status_set.Put(ReportCorruptionStatus::kInvalidMetadata);
      }
      break;
    }
    case AttributionReport::Type::kNullAggregatable:
      if (corruptions.status_set.Has(ReportCorruptionStatus::kSourceNotFound)) {
        corruptions.status_set.Remove(ReportCorruptionStatus::kSourceNotFound);
      } else {
        corruptions.status_set.Put(
            ReportCorruptionStatus::kSourceDataFoundNullAggregatable);
      }
      data = DeserializeNullAggregatableReportMetadata(metadata);
      if (!data.has_value()) {
        corruptions.status_set.Put(ReportCorruptionStatus::kInvalidMetadata);
      }
      break;
  }

  if (!corruptions.status_set.empty()) {
    corruptions.status_set.Put(ReportCorruptionStatus::kAnyFieldCorrupted);
    return base::unexpected(std::move(corruptions));
  }

  DCHECK(data.has_value());
  DCHECK(reporting_origin.has_value());

  return AttributionReport(AttributionInfo(trigger_time, trigger_debug_key,
                                           *std::move(context_origin)),
                           report_id, report_time, initial_report_time,
                           std::move(external_report_id), failed_send_attempts,
                           *std::move(data), *std::move(reporting_origin));
}

std::vector<AttributionReport> AttributionStorageSql::GetAttributionReports(
    base::Time max_report_time,
    int limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return {};
  }

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
    base::expected<AttributionReport, ReportCorruptionStatusSetAndIds> report =
        ReadReportFromStatement(statement);
    if (report.has_value()) {
      reports.emplace_back(*std::move(report));
    }
  }

  if (!statement.Succeeded()) {
    return {};
  }

  return reports;
}

std::optional<base::Time> AttributionStorageSql::GetNextReportTime(
    base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return std::nullopt;
  }

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kNextReportTimeSql));
  statement.BindTime(0, time);

  if (statement.Step() &&
      statement.GetColumnType(0) != sql::ColumnType::kNull) {
    return statement.ColumnTime(0);
  }

  return std::nullopt;
}

std::optional<AttributionReport> AttributionStorageSql::GetReport(
    AttributionReport::Id id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return std::nullopt;
  }

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetReportSql));
  statement.BindInt64(0, *id);

  if (!statement.Step()) {
    return std::nullopt;
  }
  auto report = ReadReportFromStatement(statement);
  return report.has_value() ? std::make_optional(*std::move(report))
                            : std::nullopt;
}

bool AttributionStorageSql::DeleteExpiredSources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

  bool success = DeleteReportInternal(report_id);
  if (success) {
    base::UmaHistogramCustomCounts(
        "Conversions.DbVersionOnReportSentAndDeleted", kCurrentVersionNumber,
        /*min=*/58,
        /*exclusive_max=*/88, /*buckets=*/30);
  }
  return success;
}

bool AttributionStorageSql::DeleteReportInternal(
    AttributionReport::Id report_id) {
  static constexpr char kDeleteReportSql[] =
      "DELETE FROM reports WHERE report_id=?";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteReportSql));
  statement.BindInt64(0, *report_id);
  return statement.Run();
}

bool AttributionStorageSql::DeleteEventLevelReportsTriggeredLaterThanForSources(
    base::span<const StoredSource::Id> sources,
    base::Time source_time) {
  DCHECK(db_.HasActiveTransactions());
  // Note that this may also delete true reports if the user configured the
  // clock between the trigger time and now.
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      attribution_queries::kDeletePendingEventLevelReportsForSourceSql));

  statement.BindTime(1, source_time);

  for (StoredSource::Id id : sources) {
    statement.Reset(/*clear_bound_vars=*/false);
    statement.BindInt64(0, *id);
    while (statement.Step()) {
      // Note that this is a no-op for fake reports whose report IDs were not
      // stored in the rate-limits record.
      AttributionReport::Id report_id(statement.ColumnInt64(0));
      if (!rate_limit_table_.DeleteAttributionRateLimit(
              &db_, RateLimitTable::Scope::kEventLevelAttribution, report_id)) {
        return false;
      }
    }
    if (!statement.Succeeded()) {
      return false;
    }
  }
  return true;
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

bool AttributionStorageSql::AdjustOfflineReportTimes(
    base::TimeDelta min_delay,
    base::TimeDelta max_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_GE(min_delay, base::TimeDelta());
  DCHECK_GE(max_delay, base::TimeDelta());
  DCHECK_LE(min_delay, max_delay);

  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return false;
  }

  base::Time now = base::Time::Now();

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kSetReportTimeSql));
  statement.BindTime(0, now + min_delay);
  statement.BindTimeDelta(1, max_delay - min_delay + base::Microseconds(1));
  statement.BindTime(2, now);
  return statement.Run();
}

void AttributionStorageSql::ClearDataWithFilter(
    base::Time delete_begin,
    base::Time delete_end,
    StoragePartition::StorageKeyMatcherFunction filter,
    bool delete_rate_limit_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return;
  }

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
  // TODO(crbug.com/40212333): Look into optimizing origin filter callback.

  std::vector<StoredSource::Id> source_ids_to_delete;

  int num_event_reports_deleted = 0;
  int num_aggregatable_reports_deleted = 0;

  if (!ClearReportsForOriginsInRange(
          delete_begin, delete_end, filter, source_ids_to_delete,
          num_event_reports_deleted, num_aggregatable_reports_deleted)) {
    return;
  }

  // Since multiple reports can be associated with a single source,
  // deduplicate source IDs to avoid redundant DB operations below.
  DeduplicateSourceIds(source_ids_to_delete);

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

  if (delete_rate_limit_data &&
      !aggregatable_debug_rate_limit_table_.ClearDataForOriginsInRange(
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return;
  }

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
    std::optional<AttributionReport::Type> report_type =
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

  if (delete_rate_limit_data &&
      !aggregatable_debug_rate_limit_table_.ClearAllDataAllTime(&db_)) {
    return;
  }

  if (!transaction.Commit()) {
    return;
  }

  RecordSourcesDeleted(num_sources_deleted);
  RecordReportsDeleted(num_event_reports_deleted,
                       num_aggregatable_reports_deleted);
}

int64_t AttributionStorageSql::CountActiveSourcesWithSourceOrigin(
    const SuitableOrigin& origin,
    const base::Time now) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE,
      attribution_queries::kCountActiveSourcesFromSourceOriginSql));
  statement.BindString(0, origin.Serialize());
  statement.BindTime(1, now);
  if (!statement.Step()) {
    return -1;
  }
  return statement.ColumnInt64(0);
}

int64_t AttributionStorageSql::CountReportsWithDestinationSite(
    const net::SchemefulSite& destination,
    AttributionReport::Type report_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kCountReportsForDestinationSql));
  statement.BindString(0, destination.Serialize());
  statement.BindInt(1, SerializeReportType(report_type));

  if (!statement.Step()) {
    return -1;
  }
  return statement.ColumnInt64(0);
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
    base::expected<StoredSourceData, ReportCorruptionStatusSetAndIds>
        source_data = ReadSourceFromStatement(statement);
    if (source_data.has_value()) {
      sources.push_back(std::move(source_data->source));
    }
  }
  if (!statement.Succeeded()) {
    return {};
  }

  return sources;
}

bool AttributionStorageSql::ReadDedupKeys(
    StoredSource::Id source_id,
    std::vector<uint64_t>& event_level_dedup_keys,
    std::vector<uint64_t>& aggregatable_dedup_keys) {
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, attribution_queries::kDedupKeySql));
  statement.BindInt64(0, *source_id);

  while (statement.Step()) {
    uint64_t dedup_key = DeserializeUint64(statement.ColumnInt64(0));

    std::optional<AttributionReport::Type> report_type =
        DeserializeReportType(statement.ColumnInt(1));
    if (!report_type.has_value()) {
      continue;
    }
    switch (*report_type) {
      case AttributionReport::Type::kEventLevel:
        event_level_dedup_keys.push_back(dedup_key);
        break;
      case AttributionReport::Type::kAggregatableAttribution:
        aggregatable_dedup_keys.push_back(dedup_key);
        break;
      case AttributionReport::Type::kNullAggregatable:
        break;
    }
  }
  return statement.Succeeded();
}

bool AttributionStorageSql::StoreDedupKey(StoredSource::Id source_id,
                                          uint64_t dedup_key,
                                          AttributionReport::Type report_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

  // It's possible that `db_status_` was set by `DatabaseErrorCallback()` during
  // a call to `sql::Database::Open()`. Some databases attempt recovery at this
  // point, but we opt to delete the database from disk. Recovery can always
  // result in partial data loss, even when it appears to succeed. SQLite's
  // documentation discusses how some use cases can tolerate partial data loss,
  // while others cannot: <https://www.sqlite.org/recovery.html>.
  if (db_status_ == DbStatus::kClosedDueToCatastrophicError) {
    const bool delete_ok = sql::Database::Delete(path_to_database_);
    LOG_IF(WARNING, !delete_ok)
        << "Failed to delete database after catastrophic SQLite error";
  }

  db_status_ = DbStatus::kClosed;
}

bool AttributionStorageSql::LazyInit(DbCreationPolicy creation_policy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_status_) {
    if (path_to_database_.empty()) {
      db_status_ = DbStatus::kDeferringCreation;
    } else {
      db_status_ = base::PathExists(path_to_database_)
                       ? DbStatus::kDeferringOpen
                       : DbStatus::kDeferringCreation;
    }
  }

  switch (*db_status_) {
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
    case DbStatus::kOpen:
      return true;
    case DbStatus::kClosed:
    case DbStatus::kClosedDueToCatastrophicError:
      return false;
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

  if (!InitializeSchema(db_status_ == DbStatus::kDeferringCreation)) {
    DLOG(ERROR) << "Failed to initialize schema for Conversion database";
    HandleInitializationFailure(InitStatus::kFailedToInitializeSchema);
    return false;
  }

  db_status_ = DbStatus::kOpen;
  RecordInitializationStatus(InitStatus::kSuccess);

  if (int64_t file_size = GetStorageFileSizeKB(path_to_database_);
      file_size > -1) {
    base::UmaHistogramCounts10M("Conversions.Storage.Sql.FileSize2", file_size);
    std::optional<int64_t> number_of_sources = NumberOfSources();
    if (number_of_sources.has_value() && *number_of_sources > 0) {
      base::UmaHistogramCounts1M("Conversions.Storage.Sql.FileSize2.PerSource",
                                 file_size * 1024 / *number_of_sources);
    }
  }

  VerifyReports(/*deletion_counts=*/nullptr);

  return true;
}

std::optional<int64_t> AttributionStorageSql::NumberOfSources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kCountSourcesSql));
  if (!statement.Step()) {
    return std::nullopt;
  }
  return statement.ColumnInt64(0);
}

// Deletes corrupt sources/reports if `deletion_counts` is not `nullptr`.
void AttributionStorageSql::VerifyReports(DeletionCounts* deletion_counts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kGetReportsSql));
  statement.BindTime(0, base::Time::Max());
  statement.BindInt(1, -1);

  int valid_reports = 0;
  while (statement.Step()) {
    base::expected<AttributionReport, ReportCorruptionStatusSetAndIds> report =
        ReadReportFromStatement(statement);
    if (report.has_value()) {
      valid_reports++;
    } else {
      ReportCorruptionStatusSetAndIds corruption_case = report.error();
      for (ReportCorruptionStatus corruption_cause :
           corruption_case.status_set) {
        base::UmaHistogramEnumeration("Conversions.CorruptReportsInDatabase5",
                                      corruption_cause);
      }
      if (deletion_counts) {
        sql::Transaction transaction(&db_);
        if (!transaction.Begin()) {
          return;
        }
        int num_sources_deleted = 0;
        int num_event_reports_deleted = 0;
        int num_aggregatable_reports_deleted = 0;
        bool ok = absl::visit(
            base::Overloaded{[](absl::monostate) { return true; },
                             [&](const StoredSource::Id id)
                                 VALID_CONTEXT_REQUIRED(sequence_checker_) {
                                   auto ids = base::span_from_ref(id);
                                   if (!DeleteSources(ids)) {
                                     return false;
                                   }
                                   num_sources_deleted++;

                                   return ClearReportsForSourceIds(
                                       ids, num_event_reports_deleted,
                                       num_aggregatable_reports_deleted);
                                 },
                             [&](AttributionReport::Id id)
                                 VALID_CONTEXT_REQUIRED(sequence_checker_) {
                                   bool success = DeleteReportInternal(id);
                                   if (success) {
                                     // Increment arbitrary counter.
                                     num_event_reports_deleted++;
                                   }
                                   return success;
                                 }},
            corruption_case.source_or_report_id);
        if (!ok || !transaction.Commit()) {
          return;
        }
        deletion_counts->sources += num_sources_deleted;
        deletion_counts->reports +=
            num_event_reports_deleted + num_aggregatable_reports_deleted;
      }
    }
  }
  base::UmaHistogramCounts1000("Conversions.ValidReportsInDatabase",
                               valid_reports);
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

  return UpgradeAttributionStorageSqlSchema(*this, db_, meta_table);
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
  // |num_attributions|, |remaining_aggregatable_attribution_budget|,
  // |num_aggregatable_attribution_reports|, |num_aggregatable_debug_budget|,
  // |num_aggregatable_debug_reports|, |event_level_active| and
  // |aggregatable_active| which are updated when a new trigger is received.
  // |num_attributions| is the number of times an event-level report has been
  // created for a given source. |remaining_aggregatable_attribution_budget| is
  // the aggregatable attribution budget that remains for a given source.
  // |num_aggregatable_attribution_reports| is the number of times an
  // aggregatable attribution report has been created for a given source.
  // |remaining_aggregatable_debug_budget| is the aggregatable debug budget
  // that remains for a given source. |num_aggregatable_debug_reports| is the
  // number of times an aggregatable debug report has been created for a given
  // source.
  // |delegate_| can choose to enforce a maximum limit on them.
  // |event_level_active| and |aggregatable_active| indicate whether a source is
  // able to create new associated event-level and aggregatable reports.
  // |event_level_active| and |aggregatable_active| can be unset on a number of
  // conditions:
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
  // |attribution_scopes_data| is a serialized
  // `attribution_reporting::AttributionScopeData` used for pre-attribution
  // source matching.
  //
  // |source_id| uses AUTOINCREMENT to ensure that IDs aren't reused over
  // the lifetime of the DB.
  //
  // TODO(linnan): Read and update |num_aggregatable_debug_reports| when
  // creating an aggregatable debug report for the source.
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
      "remaining_aggregatable_attribution_budget INTEGER NOT NULL,"
      "num_aggregatable_attribution_reports INTEGER NOT NULL,"
      "aggregatable_source BLOB NOT NULL,"
      "filter_data BLOB NOT NULL,"
      "read_only_source_data BLOB NOT NULL,"
      "remaining_aggregatable_debug_budget INTEGER NOT NULL,"
      "num_aggregatable_debug_reports INTEGER NOT NULL,"
      "attribution_scopes_data BLOB)";
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
  // |context_origin| is the origin where the report was created. For
  // real reports and null reports, it is the destination origin on which the
  // trigger was registered. For fake event-level reports, it is the source
  // origin. Used for checking settings and included in aggregatable reports.
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

  if (!aggregatable_debug_rate_limit_table_.CreateTable(&db_)) {
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

// The interaction between this error callback and `sql::Database` is complex.
// Here are just a few of the sharp edges:
//
// 1. This callback would become reentrant if it called a `sql::Database` method
//    that could encounter an error.
//
// 2. This callback may be invoked multiple times by a single call to a
//    `sql::Database` method.
//
// 3. This callback may see phantom errors that do not otherwise bubble up via
//    return values. This can happen because `sql::Database` runs the error
//    callback eagerly despite the fact that some of its methods ignore certain
//    errors.
//
//    A concrete example: opening the database may run the error callback *and*
//    return true if `sql::Database::Open()` encounters a transient error, but
//    opens the database successfully on the second try.
//
// Reducing this complexity will likely require a redesign of `sql::Database`'s
// error handling interface. See <https://crbug.com/40199997>.
void AttributionStorageSql::DatabaseErrorCallback(int extended_error,
                                                  sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Inform the test framework that we encountered this error.
  std::ignore = sql::Database::IsExpectedSqliteError(extended_error);

  // Consider the database closed to avoid further errors. Note that the value
  // we write to `db_status_` may be subsequently overwritten elsewhere if
  // `sql::Database` ignores the error (see sharp edge #3 above).
  if (sql::IsErrorCatastrophic(extended_error)) {
    db_status_ = DbStatus::kClosedDueToCatastrophicError;
  } else {
    db_status_ = DbStatus::kClosed;
  }

  // Prevent future uses of `db_` from having any effect until we unpoison it
  // with `db_.Close()`.
  if (db_.is_open()) {
    db_.Poison();
  }
}

bool AttributionStorageSql::DeleteSources(
    base::span<const StoredSource::Id> source_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

  auto match_filter = [&](std::string_view str) {
    return filter.is_null() || filter.Run(blink::StorageKey::CreateFirstParty(
                                   DeserializeOrigin(str)));
  };

  sql::Statement scan_sources_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, attribution_queries::kScanSourcesData));
  scan_sources_statement.BindTime(0, delete_begin);
  scan_sources_statement.BindTime(1, delete_end);

  while (scan_sources_statement.Step()) {
    if (match_filter(scan_sources_statement.ColumnStringView(0))) {
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
    if (!match_filter(scan_reports_statement.ColumnStringView(0))) {
      continue;
    }
    source_ids_to_delete.emplace_back(scan_reports_statement.ColumnInt64(1));
    std::optional<AttributionReport::Type> report_type =
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
    if (!DeleteReportInternal(
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
    base::span<const StoredSource::Id> source_ids,
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
      std::optional<AttributionReport::Type> report_type =
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

namespace {

RateLimitResult AggregatableAttributionAllowedForBudgetLimit(
    const AttributionReport::AggregatableAttributionData&
        aggregatable_attribution,
    int remaining_aggregatable_attribution_budget) {
  if (remaining_aggregatable_attribution_budget <= 0) {
    return RateLimitResult::kNotAllowed;
  }

  const base::CheckedNumeric<int64_t> budget_required =
      aggregatable_attribution.BudgetRequired();
  if (!budget_required.IsValid() ||
      budget_required.ValueOrDie() >
          remaining_aggregatable_attribution_budget) {
    return RateLimitResult::kNotAllowed;
  }

  return RateLimitResult::kAllowed;
}

}  // namespace

bool AttributionStorageSql::AdjustBudgetConsumedForSource(
    StoredSource::Id source_id,
    int additional_budget_consumed) {
  DCHECK_GE(additional_budget_consumed, 0);

  static constexpr char kAdjustBudgetConsumedForSourceSql[] =
      "UPDATE sources "
      "SET "
      "remaining_aggregatable_attribution_budget="
      "remaining_aggregatable_attribution_budget-?,"
      "num_aggregatable_attribution_reports="
      "num_aggregatable_attribution_reports+1 "
      "WHERE source_id=?";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kAdjustBudgetConsumedForSourceSql));
  statement.BindInt64(0, additional_budget_consumed);
  statement.BindInt64(1, *source_id);
  return statement.Run() && db_.GetLastChangeCount() == 1;
}

std::optional<AttributionReport::Id>
AttributionStorageSql::StoreAttributionReport(
    StoredSource::Id source_id,
    base::Time trigger_time,
    base::Time initial_report_time,
    const base::Uuid& external_report_id,
    std::optional<uint64_t> trigger_debug_key,
    const SuitableOrigin& context_origin,
    const SuitableOrigin& reporting_origin,
    uint32_t trigger_data,
    int64_t priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return StoreAttributionReport(
      *source_id, trigger_time, initial_report_time, external_report_id,
      trigger_debug_key, context_origin, reporting_origin,
      AttributionReport::Type::kEventLevel,
      SerializeEventLevelReportMetadata(trigger_data, priority));
}

[[nodiscard]] std::optional<AttributionReport::Id>
AttributionStorageSql::StoreNullReport(
    base::Time trigger_time,
    base::Time initial_report_time,
    const base::Uuid& external_report_id,
    std::optional<uint64_t> trigger_debug_key,
    const attribution_reporting::SuitableOrigin& context_origin,
    const attribution_reporting::SuitableOrigin& reporting_origin,
    const std::optional<SuitableOrigin>& coordinator_origin,
    const AggregatableTriggerConfig& trigger_config,
    base::Time fake_source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return StoreAttributionReport(
      kUnsetRecordId, trigger_time, initial_report_time, external_report_id,
      trigger_debug_key, context_origin, reporting_origin,
      AttributionReport::Type::kNullAggregatable,
      SerializeNullAggregatableReportMetadata(
          coordinator_origin, trigger_config, fake_source_time));
}

[[nodiscard]] std::optional<AttributionReport::Id>
AttributionStorageSql::StoreAggregatableReport(
    StoredSource::Id source_id,
    base::Time trigger_time,
    base::Time initial_report_time,
    const base::Uuid& external_report_id,
    std::optional<uint64_t> trigger_debug_key,
    const attribution_reporting::SuitableOrigin& context_origin,
    const attribution_reporting::SuitableOrigin& reporting_origin,
    const std::optional<SuitableOrigin>& coordinator_origin,
    const AggregatableTriggerConfig& trigger_config,
    const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
        contributions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return StoreAttributionReport(
      *source_id, trigger_time, initial_report_time, external_report_id,
      trigger_debug_key, context_origin, reporting_origin,
      AttributionReport::Type::kAggregatableAttribution,
      SerializeAggregatableReportMetadata(coordinator_origin, trigger_config,
                                          contributions));
}

std::optional<AttributionReport::Id>
AttributionStorageSql::StoreAttributionReport(
    int64_t source_id,
    base::Time trigger_time,
    base::Time initial_report_time,
    const base::Uuid& external_report_id,
    std::optional<uint64_t> trigger_debug_key,
    const SuitableOrigin& context_origin,
    const SuitableOrigin& reporting_origin,
    AttributionReport::Type report_type,
    const std::string& serialized_metadata) {
  static constexpr char kStoreReportSql[] =
      "INSERT INTO reports"
      "(source_id,trigger_time,report_time,initial_report_time,"
      "failed_send_attempts,external_report_id,debug_key,context_origin,"
      "reporting_origin,report_type,metadata)"
      "VALUES(?,?,?,?,0,?,?,?,?,?,?)";
  sql::Statement store_report_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kStoreReportSql));

  // Per https://www.sqlite.org/autoinc.html, if no negative ROWID values are
  // inserted explicitly, then automatically generated ROWID values will always
  // be greater than zero. Therefore it's fine to use -1 as sentinel value for a
  // null source.
  store_report_statement.BindInt64(0, source_id);
  store_report_statement.BindTime(1, trigger_time);
  store_report_statement.BindTime(2, initial_report_time);
  store_report_statement.BindTime(3, initial_report_time);
  store_report_statement.BindString(4, external_report_id.AsLowercaseString());
  BindUint64OrNull(store_report_statement, 5, trigger_debug_key);
  store_report_statement.BindString(6, context_origin.Serialize());
  store_report_statement.BindString(7, reporting_origin.Serialize());
  store_report_statement.BindInt(8, SerializeReportType(report_type));
  store_report_statement.BindBlob(9, serialized_metadata);

  if (!store_report_statement.Run()) {
    return std::nullopt;
  }

  return AttributionReport::Id(db_.GetLastInsertRowId());
}

AggregatableResult
AttributionStorageSql::MaybeStoreAggregatableAttributionReportData(
    AttributionReport& report,
    StoredSource::Id source_id,
    int remaining_aggregatable_attribution_budget,
    int num_aggregatable_attribution_reports,
    std::optional<uint64_t> dedup_key,
    std::optional<int>& max_aggregatable_reports_per_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto* aggregatable_attribution =
      absl::get_if<AttributionReport::AggregatableAttributionData>(
          &report.data());
  DCHECK(aggregatable_attribution);

  if (num_aggregatable_attribution_reports >=
      delegate_->GetMaxAggregatableReportsPerSource()) {
    max_aggregatable_reports_per_source =
        delegate_->GetMaxAggregatableReportsPerSource();
    return AggregatableResult::kExcessiveReports;
  }

  switch (AggregatableAttributionAllowedForBudgetLimit(
      *aggregatable_attribution, remaining_aggregatable_attribution_budget)) {
    case RateLimitResult::kAllowed:
      break;
    case RateLimitResult::kNotAllowed:
      return AggregatableResult::kInsufficientBudget;
    case RateLimitResult::kError:
      return AggregatableResult::kInternalError;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return AggregatableResult::kInternalError;
  }

  base::CheckedNumeric<int64_t> budget_required =
      aggregatable_attribution->BudgetRequired();
  // The value was already validated by
  // `AggregatableAttributionAllowedForBudgetLimit()` above.
  CHECK(budget_required.IsValid());
  int64_t budget_required_value = budget_required.ValueOrDie();
  CHECK(base::IsValueInRangeForNumericType<int>(budget_required_value));
  if (!AdjustBudgetConsumedForSource(source_id,
                                     static_cast<int>(budget_required_value))) {
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
          DeserializeOrigin(statement.ColumnStringView(0));
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

std::optional<AttributionStorageSql::AggregatableDebugSourceData>
AttributionStorageSql::GetAggregatableDebugSourceData(
    StoredSource::Id source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return std::nullopt;
  }

  static constexpr char kSelectSourceDataSql[] =
      "SELECT remaining_aggregatable_debug_budget,"
      "num_aggregatable_debug_reports "
      "FROM sources WHERE source_id=?";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSelectSourceDataSql));
  statement.BindInt64(0, *source_id);

  if (!statement.Step()) {
    return std::nullopt;
  }

  return AggregatableDebugSourceData{
      .remaining_budget = statement.ColumnInt(0),
      .num_reports = statement.ColumnInt(1),
  };
}

int64_t AttributionStorageSql::StorageFileSizeKB() {
  return GetStorageFileSizeKB(path_to_database_);
}

AggregatableDebugRateLimitTable::Result
AttributionStorageSql::AggregatableDebugReportAllowedForRateLimit(
    const AggregatableDebugReport& report) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Rate-limits are not hit in an empty database.
  if (!LazyInit(DbCreationPolicy::kIgnoreIfAbsent)) {
    return AggregatableDebugRateLimitTable::Result::kAllowed;
  }

  return aggregatable_debug_rate_limit_table_.AllowedForRateLimit(&db_, report);
}

bool AttributionStorageSql::AdjustAggregatableDebugSourceData(
    StoredSource::Id source_id,
    int additional_budget_consumed) {
  static constexpr char kAdjustSourceDataSql[] =
      "UPDATE sources "
      "SET "
      "remaining_aggregatable_debug_budget="
      "remaining_aggregatable_debug_budget-?,"
      "num_aggregatable_debug_reports=num_aggregatable_debug_reports+1 "
      "WHERE source_id=?";

  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kAdjustSourceDataSql));
  statement.BindInt(0, additional_budget_consumed);
  statement.BindInt64(1, *source_id);

  return statement.Run() && db_.GetLastChangeCount() == 1;
}

bool AttributionStorageSql::AdjustForAggregatableDebugReport(
    const AggregatableDebugReport& report,
    std::optional<StoredSource::Id> source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit(DbCreationPolicy::kCreateIfAbsent)) {
    return false;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  if (source_id.has_value() &&
      !AdjustAggregatableDebugSourceData(*source_id, report.BudgetRequired())) {
    return false;
  }

  if (!aggregatable_debug_rate_limit_table_.AddRateLimit(&db_, report)) {
    return false;
  }

  return transaction.Commit();
}

void AttributionStorageSql::SetDelegate(AttributionResolverDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate);
  aggregatable_debug_rate_limit_table_.SetDelegate(*delegate);
  rate_limit_table_.SetDelegate(*delegate);
  delegate_ = delegate;
}

bool AttributionStorageSql::AddRateLimitForSource(
    const StoredSource& source,
    int64_t destination_limit_priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(db_.HasActiveTransactions());

  return rate_limit_table_.AddRateLimitForSource(&db_, source,
                                                 destination_limit_priority);
}

bool AttributionStorageSql::AddRateLimitForAttribution(
    const AttributionInfo& attribution_info,
    const StoredSource& source,
    RateLimitTable::Scope scope,
    AttributionReport::Id id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(db_.HasActiveTransactions());

  return rate_limit_table_.AddRateLimitForAttribution(&db_, attribution_info,
                                                      source, scope, id);
}

RateLimitResult AttributionStorageSql::SourceAllowedForReportingOriginLimit(
    const StorableSource& source,
    base::Time source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return rate_limit_table_.SourceAllowedForReportingOriginLimit(&db_, source,
                                                                source_time);
}

RateLimitResult
AttributionStorageSql::SourceAllowedForReportingOriginPerSiteLimit(
    const StorableSource& source,
    base::Time source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return rate_limit_table_.SourceAllowedForReportingOriginPerSiteLimit(
      &db_, source, source_time);
}

RateLimitTable::DestinationRateLimitResult
AttributionStorageSql::SourceAllowedForDestinationRateLimit(
    const StorableSource& source,
    base::Time source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return rate_limit_table_.SourceAllowedForDestinationRateLimit(&db_, source,
                                                                source_time);
}

RateLimitResult
AttributionStorageSql::SourceAllowedForDestinationPerDayRateLimit(
    const StorableSource& source,
    base::Time source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return rate_limit_table_.SourceAllowedForDestinationPerDayRateLimit(
      &db_, source, source_time);
}

RateLimitResult
AttributionStorageSql::AttributionAllowedForReportingOriginLimit(
    const AttributionInfo& attribution_info,
    const StoredSource& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return rate_limit_table_.AttributionAllowedForReportingOriginLimit(
      &db_, attribution_info, source);
}

RateLimitResult AttributionStorageSql::AttributionAllowedForAttributionLimit(
    const AttributionInfo& attribution_info,
    const StoredSource& source,
    RateLimitTable::Scope scope) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return rate_limit_table_.AttributionAllowedForAttributionLimit(
      &db_, attribution_info, source, scope);
}

base::expected<std::vector<StoredSource::Id>, RateLimitTable::Error>
AttributionStorageSql::GetSourcesToDeactivateForDestinationLimit(
    const StorableSource& source,
    base::Time source_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return rate_limit_table_.GetSourcesToDeactivateForDestinationLimit(
      &db_, source, source_time);
}

bool AttributionStorageSql::DeleteAttributionRateLimit(
    RateLimitTable::Scope scope,
    AttributionReport::Id report_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return rate_limit_table_.DeleteAttributionRateLimit(&db_, scope, report_id);
}

}  // namespace content
