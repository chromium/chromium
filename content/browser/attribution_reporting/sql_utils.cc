// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/sql_utils.h"

#include <stdint.h>

#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/aggregatable_filtering_id_max_bytes.h"
#include "components/attribution_reporting/aggregatable_trigger_config.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_data_matching.mojom.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "sql/statement.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::AggregatableTriggerConfig;
using ::attribution_reporting::EventReportWindows;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::TriggerSpec;
using ::attribution_reporting::TriggerSpecs;
using ::attribution_reporting::mojom::SourceRegistrationTimeConfig;
using ::attribution_reporting::mojom::SourceType;
using ::attribution_reporting::mojom::TriggerDataMatching;

bool IsValid(const proto::AttributionAggregationKey& key) {
  return key.has_high_bits() && key.has_low_bits();
}

void SerializeCommonAggregatableData(
    proto::AttributionCommonAggregatableMetadata& msg,
    const std::optional<SuitableOrigin>& aggregation_coordinator_origin,
    const AggregatableTriggerConfig& trigger_config) {
  if (aggregation_coordinator_origin.has_value()) {
    msg.set_coordinator_origin(aggregation_coordinator_origin->Serialize());
  }

  switch (trigger_config.source_registration_time_config()) {
    case SourceRegistrationTimeConfig::kInclude:
      msg.set_source_registration_time_config(
          proto::AttributionCommonAggregatableMetadata::INCLUDE);
      break;
    case SourceRegistrationTimeConfig::kExclude:
      msg.set_source_registration_time_config(
          proto::AttributionCommonAggregatableMetadata::EXCLUDE);
      break;
  }

  if (const auto& trigger_context_id = trigger_config.trigger_context_id();
      trigger_context_id.has_value()) {
    msg.set_trigger_context_id(*trigger_context_id);
  }

  msg.set_filtering_id_max_bytes(
      trigger_config.aggregatable_filtering_id_max_bytes().value());
}

std::optional<AttributionReport::CommonAggregatableData>
DeserializeCommonAggregatableData(
    const proto::AttributionCommonAggregatableMetadata& msg) {
  if (!msg.has_source_registration_time_config()) {
    return std::nullopt;
  }

  std::optional<attribution_reporting::SuitableOrigin>
      aggregation_coordinator_origin;
  if (msg.has_coordinator_origin()) {
    aggregation_coordinator_origin =
        attribution_reporting::SuitableOrigin::Deserialize(
            msg.coordinator_origin());
    if (!aggregation_coordinator_origin.has_value()) {
      return std::nullopt;
    }
  }

  SourceRegistrationTimeConfig source_registration_time_config;

  switch (msg.source_registration_time_config()) {
    case proto::AttributionCommonAggregatableMetadata::INCLUDE:
      source_registration_time_config = SourceRegistrationTimeConfig::kInclude;
      break;
    case proto::AttributionCommonAggregatableMetadata::EXCLUDE:
      source_registration_time_config = SourceRegistrationTimeConfig::kExclude;
      break;
    default:
      return std::nullopt;
  }

  std::optional<std::string> trigger_context_id;
  if (msg.has_trigger_context_id()) {
    trigger_context_id = msg.trigger_context_id();
  }

  attribution_reporting::AggregatableFilteringIdsMaxBytes max_bytes;
  if (msg.has_filtering_id_max_bytes()) {
    auto read_max_bytes =
        attribution_reporting::AggregatableFilteringIdsMaxBytes::Create(
            msg.filtering_id_max_bytes());
    if (!read_max_bytes.has_value()) {
      return std::nullopt;
    }
    max_bytes = read_max_bytes.value();
  }

  auto aggregatable_trigger_config = AggregatableTriggerConfig::Create(
      source_registration_time_config, trigger_context_id, max_bytes);
  if (!aggregatable_trigger_config.has_value()) {
    return std::nullopt;
  }

  return AttributionReport::CommonAggregatableData(
      std::move(aggregation_coordinator_origin),
      *std::move(aggregatable_trigger_config));
}

}  // namespace

url::Origin DeserializeOrigin(std::string_view origin) {
  return url::Origin::Create(GURL(origin));
}

std::optional<SourceType> DeserializeSourceType(int val) {
  switch (val) {
    case static_cast<int>(SourceType::kNavigation):
      return SourceType::kNavigation;
    case static_cast<int>(SourceType::kEvent):
      return SourceType::kEvent;
    default:
      return std::nullopt;
  }
}

void SetReadOnlySourceData(
    const EventReportWindows* event_report_windows,
    attribution_reporting::MaxEventLevelReports max_event_level_reports,
    proto::AttributionReadOnlySourceData& msg) {
  msg.set_max_event_level_reports(max_event_level_reports);

  if (event_report_windows) {
    msg.set_event_level_report_window_start_time(
        event_report_windows->start_time().InMicroseconds());

    for (base::TimeDelta time : event_report_windows->end_times()) {
      msg.add_event_level_report_window_end_times(time.InMicroseconds());
    }
  }
}

std::string SerializeReadOnlySourceData(
    const attribution_reporting::TriggerSpecs& trigger_specs,
    double randomized_response_rate,
    TriggerDataMatching trigger_data_matching,
    bool debug_cookie_set,
    absl::uint128 aggregatable_debug_key_piece) {
  DCHECK_GE(randomized_response_rate, 0);
  DCHECK_LE(randomized_response_rate, 1);

  proto::AttributionReadOnlySourceData msg;

  if (
      // Calling `mutable_trigger_data()` forces creation of the field, even
      // when `trigger_specs.empty()` below, so that the presence check in
      // `DeserializeTriggerSpecs()` doesn't mistakenly use the defaults
      // corresponding to the field being absent, as opposed to its inner list
      // being empty.
      auto* mutable_trigger_data = msg.mutable_trigger_data();
      const TriggerSpec* trigger_spec = trigger_specs.SingleSharedSpec()) {
    SetReadOnlySourceData(&trigger_spec->event_report_windows(),
                          trigger_specs.max_event_level_reports(), msg);

    for (auto [trigger_data, _] : trigger_specs.trigger_data_indices()) {
      mutable_trigger_data->add_trigger_data(trigger_data);
    }
  } else {
    // TODO(crbug.com/40287976): Support multiple specs.
    DCHECK(trigger_specs.empty());

    SetReadOnlySourceData(/*event_report_windows=*/nullptr,
                          trigger_specs.max_event_level_reports(), msg);
  }

  msg.set_randomized_response_rate(randomized_response_rate);

  switch (trigger_data_matching) {
    case TriggerDataMatching::kExact:
      msg.set_trigger_data_matching(
          proto::AttributionReadOnlySourceData::EXACT);
      break;
    case TriggerDataMatching::kModulus:
      msg.set_trigger_data_matching(
          proto::AttributionReadOnlySourceData::MODULUS);
      break;
  }

  msg.set_debug_cookie_set(debug_cookie_set);

  proto::AttributionAggregationKey* key_msg =
      msg.mutable_aggregatable_debug_key_piece();
  key_msg->set_high_bits(absl::Uint128High64(aggregatable_debug_key_piece));
  key_msg->set_low_bits(absl::Uint128Low64(aggregatable_debug_key_piece));

  return msg.SerializeAsString();
}

std::optional<proto::AttributionReadOnlySourceData>
DeserializeReadOnlySourceDataAsProto(sql::Statement& stmt, int col) {
  proto::AttributionReadOnlySourceData msg;
  if (base::span<const uint8_t> blob = stmt.ColumnBlob(col);
      !msg.ParseFromArray(blob.data(), blob.size())) {
    return std::nullopt;
  }
  return msg;
}

std::string SerializeFilterData(
    const attribution_reporting::FilterData& filter_data) {
  proto::AttributionFilterData msg;

  for (const auto& [filter, values] : filter_data.filter_values()) {
    proto::AttributionFilterValues filter_values_msg;
    filter_values_msg.mutable_values()->Add(values.begin(), values.end());
    (*msg.mutable_filter_values())[filter] = std::move(filter_values_msg);
  }

  return msg.SerializeAsString();
}

std::optional<attribution_reporting::FilterData> DeserializeFilterData(
    sql::Statement& stmt,
    int col) {
  proto::AttributionFilterData msg;
  if (base::span<const uint8_t> blob = stmt.ColumnBlob(col);
      !msg.ParseFromArray(blob.data(), blob.size())) {
    return std::nullopt;
  }

  attribution_reporting::FilterValues::container_type filter_values;
  filter_values.reserve(msg.filter_values_size());

  for (auto& entry : *msg.mutable_filter_values()) {
    // Serialized source filter data can only contain these keys due to DB
    // corruption or deliberate modification.
    if (entry.first ==
            attribution_reporting::FilterData::kSourceTypeFilterKey ||
        entry.first.starts_with(
            attribution_reporting::FilterConfig::kReservedKeyPrefix)) {
      continue;
    }

    auto* values = entry.second.mutable_values();

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

  return msg.SerializeAsString();
}

std::optional<attribution_reporting::AggregationKeys>
DeserializeAggregationKeys(sql::Statement& stmt, int col) {
  proto::AttributionAggregatableSource msg;
  if (base::span<const uint8_t> blob = stmt.ColumnBlob(col);
      !msg.ParseFromArray(blob.data(), blob.size())) {
    return std::nullopt;
  }

  attribution_reporting::AggregationKeys::Keys::container_type keys;
  keys.reserve(msg.keys_size());

  for (const auto& [id, key] : msg.keys()) {
    if (!IsValid(key)) {
      return std::nullopt;
    }

    keys.emplace_back(id, absl::MakeUint128(key.high_bits(), key.low_bits()));
  }

  return attribution_reporting::AggregationKeys::FromKeys(std::move(keys));
}

std::string SerializeEventLevelReportMetadata(uint32_t trigger_data,
                                              int64_t priority) {
  proto::AttributionEventLevelMetadata msg;
  msg.set_trigger_data(trigger_data);
  msg.set_priority(priority);
  return msg.SerializeAsString();
}

std::optional<AttributionReport::EventLevelData>
DeserializeEventLevelReportMetadata(base::span<const uint8_t> blob,
                                    const StoredSource& source) {
  proto::AttributionEventLevelMetadata msg;
  if (!msg.ParseFromArray(blob.data(), blob.size()) ||
      !msg.has_trigger_data() || !msg.has_priority()) {
    return std::nullopt;
  }

  return AttributionReport::EventLevelData(msg.trigger_data(), msg.priority(),
                                           source);
}

std::optional<int64_t> DeserializeEventLevelPriority(
    base::span<const uint8_t> blob) {
  proto::AttributionEventLevelMetadata msg;

  // Strictly the `has_trigger_data()` check is unnecessary, but to avoid
  // changing which reports are considered corrupt by
  // `AttributionStorageSql::MaybeReplaceLowerPriorityEventLevelReport()` we
  // retain it here.
  if (!msg.ParseFromArray(blob.data(), blob.size()) ||
      !msg.has_trigger_data() || !msg.has_priority()) {
    return std::nullopt;
  }

  return msg.priority();
}

std::string SerializeAggregatableReportMetadata(
    const std::optional<SuitableOrigin>& aggregation_coordinator_origin,
    const AggregatableTriggerConfig& trigger_config,
    const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
        contributions) {
  proto::AttributionAggregatableMetadata msg;

  SerializeCommonAggregatableData(*msg.mutable_common_data(),
                                  aggregation_coordinator_origin,
                                  trigger_config);

  msg.mutable_contributions()->Reserve(contributions.size());
  for (const auto& contribution : contributions) {
    proto::AttributionAggregatableMetadata_Contribution* contribution_msg =
        msg.add_contributions();
    contribution_msg->mutable_key()->set_high_bits(
        absl::Uint128High64(contribution.bucket));
    contribution_msg->mutable_key()->set_low_bits(
        absl::Uint128Low64(contribution.bucket));
    contribution_msg->set_value(
        base::checked_cast<uint32_t>(contribution.value));
    if (contribution.filtering_id.has_value()) {
      contribution_msg->set_filtering_id(contribution.filtering_id.value());
    }
  }

  return msg.SerializeAsString();
}

std::optional<AttributionReport::AggregatableAttributionData>
DeserializeAggregatableReportMetadata(base::span<const uint8_t> blob,
                                      const StoredSource& source) {
  proto::AttributionAggregatableMetadata msg;
  if (!msg.ParseFromArray(blob.data(), blob.size()) ||
      msg.contributions().empty() || !msg.has_common_data()) {
    return std::nullopt;
  }

  std::optional<AttributionReport::CommonAggregatableData> common_data =
      DeserializeCommonAggregatableData(msg.common_data());
  if (!common_data.has_value()) {
    return std::nullopt;
  }

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions;
  contributions.reserve(msg.contributions_size());

  for (const auto& contribution_msg : msg.contributions()) {
    if (!contribution_msg.has_key() || !contribution_msg.has_value() ||
        !IsValid(contribution_msg.key()) || contribution_msg.value() == 0 ||
        contribution_msg.value() >
            attribution_reporting::kMaxAggregatableValue) {
      return std::nullopt;
    }
    std::optional<uint64_t> filtering_id;
    if (contribution_msg.has_filtering_id()) {
      if (!common_data->aggregatable_trigger_config
               .aggregatable_filtering_id_max_bytes()
               .CanEncompass(contribution_msg.filtering_id())) {
        return std::nullopt;
      }
      filtering_id = contribution_msg.filtering_id();
    }
    contributions.emplace_back(
        absl::MakeUint128(contribution_msg.key().high_bits(),
                          contribution_msg.key().low_bits()),
        base::checked_cast<int32_t>(contribution_msg.value()), filtering_id);
  }

  return AttributionReport::AggregatableAttributionData(
      *std::move(common_data), std::move(contributions), source);
}

std::string SerializeNullAggregatableReportMetadata(
    const std::optional<SuitableOrigin>& aggregation_coordinator_origin,
    const AggregatableTriggerConfig& trigger_config,
    base::Time fake_source_time) {
  proto::AttributionNullAggregatableMetadata msg;

  SerializeCommonAggregatableData(*msg.mutable_common_data(),
                                  aggregation_coordinator_origin,
                                  trigger_config);

  msg.set_fake_source_time(
      fake_source_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  return msg.SerializeAsString();
}

std::optional<AttributionReport::NullAggregatableData>
DeserializeNullAggregatableReportMetadata(base::span<const uint8_t> blob) {
  proto::AttributionNullAggregatableMetadata msg;
  if (!msg.ParseFromArray(blob.data(), blob.size()) ||
      !msg.has_fake_source_time() || !msg.has_common_data()) {
    return std::nullopt;
  }

  std::optional<AttributionReport::CommonAggregatableData> common_data =
      DeserializeCommonAggregatableData(msg.common_data());
  if (!common_data.has_value()) {
    return std::nullopt;
  }

  return AttributionReport::NullAggregatableData(
      *std::move(common_data),
      /*fake_source_time=*/
      base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(msg.fake_source_time())));
}

std::optional<TriggerSpecs> DeserializeTriggerSpecs(
    const proto::AttributionReadOnlySourceData& msg,
    SourceType source_type,
    attribution_reporting::MaxEventLevelReports max_event_level_reports) {
  if (msg.has_trigger_data() && msg.trigger_data().trigger_data().empty()) {
    return TriggerSpecs();
  }

  std::vector<base::TimeDelta> end_times;
  end_times.reserve(msg.event_level_report_window_end_times_size());

  for (int64_t time : msg.event_level_report_window_end_times()) {
    end_times.push_back(base::Microseconds(time));
  }

  auto event_report_windows = EventReportWindows::Create(
      base::Microseconds(msg.event_level_report_window_start_time()),
      std::move(end_times));
  if (!event_report_windows.has_value()) {
    return std::nullopt;
  }

  if (!msg.has_trigger_data()) {
    return TriggerSpecs(source_type, *std::move(event_report_windows),
                        max_event_level_reports);
  }

  std::vector<TriggerSpec> specs;
  specs.emplace_back(*std::move(event_report_windows));

  return TriggerSpecs::Create(
      base::MakeFlatMap<uint32_t, uint8_t>(msg.trigger_data().trigger_data(),
                                           /*comp=*/{},
                                           [](uint32_t trigger_data) {
                                             return std::make_pair(trigger_data,
                                                                   uint8_t{0});
                                           }),
      std::move(specs), max_event_level_reports);
}

std::string SerializeAttributionScopesData(
    const attribution_reporting::AttributionScopesData& scopes_data) {
  proto::AttributionScopesData msg;
  const auto& scopes = scopes_data.attribution_scopes_set().scopes();

  msg.mutable_scopes()->Add(scopes.begin(), scopes.end());
  msg.set_scope_limit(scopes_data.attribution_scope_limit());
  msg.set_max_event_states(scopes_data.max_event_states());

  return msg.SerializeAsString();
}

base::expected<std::optional<attribution_reporting::AttributionScopesData>,
               absl::monostate>
DeserializeAttributionScopesData(sql::Statement& stmt, int col) {
  proto::AttributionScopesData msg;
  if (stmt.GetColumnType(col) == sql::ColumnType::kNull) {
    return std::nullopt;
  }

  if (base::span<const uint8_t> blob = stmt.ColumnBlob(col);
      !msg.ParseFromArray(blob.data(), blob.size())) {
    return base::unexpected(absl::monostate());
  }

  base::flat_set<std::string> scopes(
      std::make_move_iterator(msg.scopes().begin()),
      std::make_move_iterator(msg.scopes().end()));
  auto scopes_data = attribution_reporting::AttributionScopesData::Create(
      attribution_reporting::AttributionScopesSet(std::move(scopes)),
      msg.scope_limit(), msg.max_event_states());
  if (!scopes_data.has_value()) {
    // DB entry is corrupted.
    return base::unexpected(absl::monostate());
  }
  return scopes_data;
}

void DeduplicateSourceIds(std::vector<StoredSource::Id>& ids) {
  ids = base::flat_set<StoredSource::Id>(std::move(ids)).extract();
}

}  // namespace content
