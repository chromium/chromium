// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/stored_source.h"

#include <stdint.h>

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/time/time.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/constants.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_level_epsilon.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/max_event_level_reports.h"
#include "components/attribution_reporting/trigger_config.h"
#include "components/attribution_reporting/trigger_data_matching.mojom-forward.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

bool IsExpiryOrReportWindowTimeValid(base::Time expiry_or_report_window_time,
                                     base::Time source_time) {
  // The source must expire strictly after it occurred.
  return expiry_or_report_window_time > source_time &&
         expiry_or_report_window_time - source_time <=
             attribution_reporting::kMaxSourceExpiry;
}

bool AreFieldsValid(int64_t aggregatable_budget_consumed,
                    double randomized_response_rate,
                    base::Time source_time,
                    base::Time expiry_time,
                    base::Time aggregatable_report_window_time,
                    absl::optional<uint64_t> debug_key,
                    bool debug_cookie_set) {
  return aggregatable_budget_consumed >= 0 && randomized_response_rate >= 0 &&
         randomized_response_rate <= 1 &&
         IsExpiryOrReportWindowTimeValid(expiry_time, source_time) &&
         IsExpiryOrReportWindowTimeValid(aggregatable_report_window_time,
                                         source_time) &&
         (!debug_key.has_value() || debug_cookie_set);
}

}  // namespace

// static
absl::optional<StoredSource> StoredSource::Create(
    CommonSourceInfo common_info,
    uint64_t source_event_id,
    attribution_reporting::DestinationSet destination_sites,
    base::Time source_time,
    base::Time expiry_time,
    attribution_reporting::TriggerSpecs trigger_specs,
    base::Time aggregatable_report_window_time,
    attribution_reporting::MaxEventLevelReports max_event_level_reports,
    int64_t priority,
    attribution_reporting::FilterData filter_data,
    absl::optional<uint64_t> debug_key,
    attribution_reporting::AggregationKeys aggregation_keys,
    AttributionLogic attribution_logic,
    ActiveState active_state,
    Id source_id,
    int64_t aggregatable_budget_consumed,
    double randomized_response_rate,
    attribution_reporting::mojom::TriggerDataMatching trigger_data_matching,
    attribution_reporting::EventLevelEpsilon event_level_epsilon,
    bool debug_cookie_set) {
  if (!AreFieldsValid(aggregatable_budget_consumed, randomized_response_rate,
                      source_time, expiry_time, aggregatable_report_window_time,
                      debug_key, debug_cookie_set)) {
    return absl::nullopt;
  }

  return StoredSource(std::move(common_info), source_event_id,
                      std::move(destination_sites), source_time, expiry_time,
                      std::move(trigger_specs), aggregatable_report_window_time,
                      max_event_level_reports, priority, std::move(filter_data),
                      debug_key, std::move(aggregation_keys), attribution_logic,
                      active_state, source_id, aggregatable_budget_consumed,
                      randomized_response_rate, trigger_data_matching,
                      event_level_epsilon, debug_cookie_set);
}

StoredSource::StoredSource(
    CommonSourceInfo common_info,
    uint64_t source_event_id,
    attribution_reporting::DestinationSet destination_sites,
    base::Time source_time,
    base::Time expiry_time,
    attribution_reporting::TriggerSpecs trigger_specs,
    base::Time aggregatable_report_window_time,
    attribution_reporting::MaxEventLevelReports max_event_level_reports,
    int64_t priority,
    attribution_reporting::FilterData filter_data,
    absl::optional<uint64_t> debug_key,
    attribution_reporting::AggregationKeys aggregation_keys,
    AttributionLogic attribution_logic,
    ActiveState active_state,
    Id source_id,
    int64_t aggregatable_budget_consumed,
    double randomized_response_rate,
    attribution_reporting::mojom::TriggerDataMatching trigger_data_matching,
    attribution_reporting::EventLevelEpsilon event_level_epsilon,
    bool debug_cookie_set)
    : common_info_(std::move(common_info)),
      source_event_id_(source_event_id),
      destination_sites_(std::move(destination_sites)),
      source_time_(source_time),
      expiry_time_(expiry_time),
      trigger_specs_(std::move(trigger_specs)),
      aggregatable_report_window_time_(aggregatable_report_window_time),
      max_event_level_reports_(max_event_level_reports),
      priority_(priority),
      filter_data_(std::move(filter_data)),
      debug_key_(debug_key),
      aggregation_keys_(std::move(aggregation_keys)),
      attribution_logic_(attribution_logic),
      active_state_(active_state),
      source_id_(source_id),
      aggregatable_budget_consumed_(aggregatable_budget_consumed),
      randomized_response_rate_(randomized_response_rate),
      trigger_data_matching_(std::move(trigger_data_matching)),
      event_level_epsilon_(event_level_epsilon),
      debug_cookie_set_(debug_cookie_set) {
  DCHECK(AreFieldsValid(aggregatable_budget_consumed_,
                        randomized_response_rate_, source_time_, expiry_time_,
                        aggregatable_report_window_time_, debug_key_,
                        debug_cookie_set_));
}

StoredSource::~StoredSource() = default;

StoredSource::StoredSource(const StoredSource&) = default;

StoredSource::StoredSource(StoredSource&&) = default;

StoredSource& StoredSource::operator=(const StoredSource&) = default;

StoredSource& StoredSource::operator=(StoredSource&&) = default;

}  // namespace content
