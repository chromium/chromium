// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/sql_utils.h"

#include <stdint.h>

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/aggregation_service/features.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/event_report_windows.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration_time_config.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_data_matching.mojom.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_reporting.pb.h"
#include "sql/statement.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using ::attribution_reporting::mojom::SourceRegistrationTimeConfig;
using ::attribution_reporting::mojom::SourceType;
using ::attribution_reporting::mojom::TriggerDataMatching;

bool IsValid(const proto::AttributionAggregationKey& key) {
  return key.has_high_bits() && key.has_low_bits();
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
    auto aggregation_coordinator_origin =
        attribution_reporting::SuitableOrigin::Deserialize(
            msg.coordinator_origin());
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

}  // namespace

url::Origin DeserializeOrigin(const std::string& origin) {
  return url::Origin::Create(GURL(origin));
}

absl::optional<SourceType> DeserializeSourceType(int val) {
  switch (val) {
    case static_cast<int>(SourceType::kNavigation):
      return SourceType::kNavigation;
    case static_cast<int>(SourceType::kEvent):
      return SourceType::kEvent;
    default:
      return absl::nullopt;
  }
}

std::string SerializeReadOnlySourceData(
    const attribution_reporting::EventReportWindows& event_report_windows,
    int max_event_level_reports,
    double randomized_response_rate,
    const attribution_reporting::TriggerConfig* trigger_config,
    const bool* debug_cookie_set) {
  DCHECK_GE(max_event_level_reports, 0);
  proto::AttributionReadOnlySourceData msg;

  msg.set_max_event_level_reports(max_event_level_reports);
  msg.set_event_level_report_window_start_time(
      event_report_windows.start_time().InMicroseconds());

  for (base::TimeDelta time : event_report_windows.end_times()) {
    msg.add_event_level_report_window_end_times(time.InMicroseconds());
  }

  if (randomized_response_rate >= 0 && randomized_response_rate <= 1) {
    msg.set_randomized_response_rate(randomized_response_rate);
  }

  if (trigger_config) {
    switch (trigger_config->trigger_data_matching()) {
      case TriggerDataMatching::kExact:
        msg.set_trigger_data_matching(
            proto::AttributionReadOnlySourceData::EXACT);
        break;
      case TriggerDataMatching::kModulus:
        msg.set_trigger_data_matching(
            proto::AttributionReadOnlySourceData::MODULUS);
        break;
    }
  }

  if (debug_cookie_set) {
    msg.set_debug_cookie_set(*debug_cookie_set);
  }

  return msg.SerializeAsString();
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
  filter_values.reserve(msg.filter_values_size());

  for (auto& entry : *msg.mutable_filter_values()) {
    // Serialized source filter data can only contain these keys due to DB
    // corruption or deliberate modification.
    if (entry.first ==
            attribution_reporting::FilterData::kSourceTypeFilterKey ||
        entry.first ==
            attribution_reporting::FilterConfig::kLookbackWindowKey) {
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
  keys.reserve(msg.keys_size());

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
  return msg.SerializeAsString();
}

bool DeserializeReportMetadata(const std::string& str,
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

  return msg.SerializeAsString();
}

bool DeserializeReportMetadata(
    const std::string& str,
    AttributionReport::AggregatableAttributionData& data) {
  proto::AttributionAggregatableMetadata msg;
  if (!msg.ParseFromString(str) || msg.contributions().empty() ||
      !msg.has_common_data() ||
      !DeserializeCommonAggregatableData(msg.common_data(), data.common_data)) {
    return false;
  }

  data.contributions.reserve(msg.contributions_size());
  for (const auto& contribution_msg : msg.contributions()) {
    if (!contribution_msg.has_key() || !contribution_msg.has_value() ||
        !IsValid(contribution_msg.key()) || contribution_msg.value() == 0 ||
        contribution_msg.value() >
            attribution_reporting::kMaxAggregatableValue) {
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

  return msg.SerializeAsString();
}

bool DeserializeReportMetadata(const std::string& str,
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

absl::optional<attribution_reporting::EventReportWindows>
DeserializeEventReportWindows(const proto::AttributionReadOnlySourceData& msg) {
  std::vector<base::TimeDelta> end_times;
  end_times.reserve(msg.event_level_report_window_end_times_size());

  for (int64_t time : msg.event_level_report_window_end_times()) {
    end_times.push_back(base::Microseconds(time));
  }

  return attribution_reporting::EventReportWindows::Create(
      base::Microseconds(msg.event_level_report_window_start_time()),
      std::move(end_times));
}

}  // namespace content
