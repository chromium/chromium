// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/stored_source.h"

#include <stdint.h>

#include <limits>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/time/time.h"
#include "components/attribution_reporting/aggregatable_utils.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/attribution_scopes_data.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_level_epsilon.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_data_matching.mojom-forward.h"
#include "content/browser/attribution_reporting/common_source_info.h"

namespace content {

namespace {

bool IsExpiryOrReportWindowTimeValid(base::Time expiry_or_report_window_time,
                                     base::Time source_time) {
  // The source must expire strictly after it occurred.
  return expiry_or_report_window_time > source_time &&
         expiry_or_report_window_time - source_time <=
             attribution_reporting::kMaxSourceExpiry;
}

bool AreFieldsValid(int remaining_aggregatable_attribution_budget,
                    int remaining_aggregatable_debug_budget,
                    double randomized_response_rate,
                    base::Time source_time,
                    base::Time expiry_time,
                    base::Time aggregatable_report_window_time,
                    std::optional<uint64_t> debug_key,
                    bool debug_cookie_set) {
  static_assert(attribution_reporting::kMaxAggregatableValue <=
                std::numeric_limits<int>::max() / 2);

  return attribution_reporting::IsRemainingAggregatableBudgetInRange(
             remaining_aggregatable_attribution_budget) &&
         attribution_reporting::IsRemainingAggregatableBudgetInRange(
             remaining_aggregatable_debug_budget) &&
         attribution_reporting::IsRemainingAggregatableBudgetInRange(
             remaining_aggregatable_attribution_budget +
             remaining_aggregatable_debug_budget) &&
         randomized_response_rate >= 0 && randomized_response_rate <= 1 &&
         IsExpiryOrReportWindowTimeValid(expiry_time, source_time) &&
         IsExpiryOrReportWindowTimeValid(aggregatable_report_window_time,
                                         source_time) &&
         (!debug_key.has_value() || debug_cookie_set);
}

}  // namespace

// static
std::optional<StoredSource> StoredSource::Create(
    CommonSourceInfo common_info,
    uint64_t source_event_id,
    attribution_reporting::DestinationSet destination_sites,
    base::Time source_time,
    base::Time expiry_time,
    attribution_reporting::TriggerSpecs trigger_specs,
    base::Time aggregatable_report_window_time,
    int64_t priority,
    attribution_reporting::FilterData filter_data,
    std::optional<uint64_t> debug_key,
    attribution_reporting::AggregationKeys aggregation_keys,
    AttributionLogic attribution_logic,
    ActiveState active_state,
    Id source_id,
    int remaining_aggregatable_attribution_budget,
    double randomized_response_rate,
    attribution_reporting::mojom::TriggerDataMatching trigger_data_matching,
    attribution_reporting::EventLevelEpsilon event_level_epsilon,
    absl::uint128 aggregatable_debug_key_piece,
    int remaining_aggregatable_debug_budget,
    std::optional<attribution_reporting::AttributionScopesData>
        attribution_scopes_data) {
  if (!AreFieldsValid(remaining_aggregatable_attribution_budget,
                      remaining_aggregatable_debug_budget,
                      randomized_response_rate, source_time, expiry_time,
                      aggregatable_report_window_time, debug_key,
                      common_info.debug_cookie_set())) {
    return std::nullopt;
  }

  return StoredSource(
      std::move(common_info), source_event_id, std::move(destination_sites),
      source_time, expiry_time, std::move(trigger_specs),
      aggregatable_report_window_time, priority, std::move(filter_data),
      debug_key, std::move(aggregation_keys), attribution_logic, active_state,
      source_id, remaining_aggregatable_attribution_budget,
      randomized_response_rate, trigger_data_matching, event_level_epsilon,
      aggregatable_debug_key_piece, remaining_aggregatable_debug_budget,
      std::move(attribution_scopes_data));
}

StoredSource::StoredSource(
    CommonSourceInfo common_info,
    uint64_t source_event_id,
    attribution_reporting::DestinationSet destination_sites,
    base::Time source_time,
    base::Time expiry_time,
    attribution_reporting::TriggerSpecs trigger_specs,
    base::Time aggregatable_report_window_time,
    int64_t priority,
    attribution_reporting::FilterData filter_data,
    std::optional<uint64_t> debug_key,
    attribution_reporting::AggregationKeys aggregation_keys,
    AttributionLogic attribution_logic,
    ActiveState active_state,
    Id source_id,
    int remaining_aggregatable_attribution_budget,
    double randomized_response_rate,
    attribution_reporting::mojom::TriggerDataMatching trigger_data_matching,
    attribution_reporting::EventLevelEpsilon event_level_epsilon,
    absl::uint128 aggregatable_debug_key_piece,
    int remaining_aggregatable_debug_budget,
    std::optional<attribution_reporting::AttributionScopesData>
        attribution_scopes_data)
    : common_info_(std::move(common_info)),
      source_event_id_(source_event_id),
      destination_sites_(std::move(destination_sites)),
      source_time_(source_time),
      expiry_time_(expiry_time),
      trigger_specs_(std::move(trigger_specs)),
      aggregatable_report_window_time_(aggregatable_report_window_time),
      priority_(priority),
      filter_data_(std::move(filter_data)),
      debug_key_(debug_key),
      aggregation_keys_(std::move(aggregation_keys)),
      attribution_logic_(attribution_logic),
      active_state_(active_state),
      source_id_(source_id),
      remaining_aggregatable_attribution_budget_(
          remaining_aggregatable_attribution_budget),
      randomized_response_rate_(randomized_response_rate),
      trigger_data_matching_(std::move(trigger_data_matching)),
      event_level_epsilon_(event_level_epsilon),
      aggregatable_debug_key_piece_(aggregatable_debug_key_piece),
      remaining_aggregatable_debug_budget_(remaining_aggregatable_debug_budget),
      attribution_scopes_data_(std::move(attribution_scopes_data)) {
  DCHECK(AreFieldsValid(remaining_aggregatable_attribution_budget_,
                        remaining_aggregatable_debug_budget_,
                        randomized_response_rate_, source_time_, expiry_time_,
                        aggregatable_report_window_time_, debug_key_,
                        common_info_.debug_cookie_set()));
}

StoredSource::~StoredSource() = default;

StoredSource::StoredSource(const StoredSource&) = default;

StoredSource::StoredSource(StoredSource&&) = default;

StoredSource& StoredSource::operator=(const StoredSource&) = default;

StoredSource& StoredSource::operator=(StoredSource&&) = default;

}  // namespace content
