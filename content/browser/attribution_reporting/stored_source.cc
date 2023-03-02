// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/stored_source.h"

#include <stdint.h>

#include <utility>

#include "base/check_op.h"
#include "base/time/time.h"
#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/filters.h"
#include "content/browser/attribution_reporting/attribution_constants.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

StoredSource::StoredSource(
    CommonSourceInfo common_info,
    uint64_t source_event_id,
    attribution_reporting::DestinationSet destination_sites,
    base::Time expiry_time,
    base::Time event_report_window_time,
    base::Time aggregatable_report_window_time,
    int64_t priority,
    attribution_reporting::FilterData filter_data,
    absl::optional<uint64_t> debug_key,
    attribution_reporting::AggregationKeys aggregation_keys,
    AttributionLogic attribution_logic,
    ActiveState active_state,
    Id source_id,
    int64_t aggregatable_budget_consumed)
    : common_info_(std::move(common_info)),
      source_event_id_(source_event_id),
      destination_sites_(std::move(destination_sites)),
      expiry_time_(expiry_time),
      event_report_window_time_(event_report_window_time),
      aggregatable_report_window_time_(aggregatable_report_window_time),
      priority_(priority),
      filter_data_(std::move(filter_data)),
      debug_key_(debug_key),
      aggregation_keys_(std::move(aggregation_keys)),
      attribution_logic_(attribution_logic),
      active_state_(active_state),
      source_id_(source_id),
      aggregatable_budget_consumed_(aggregatable_budget_consumed) {
  DCHECK_GE(aggregatable_budget_consumed_, 0);

  base::Time source_time = common_info_.source_time();
  DCHECK_GE(kDefaultAttributionSourceExpiry, expiry_time_ - source_time);
  DCHECK_GE(kDefaultAttributionSourceExpiry,
            event_report_window_time_ - source_time);
  DCHECK_GE(kDefaultAttributionSourceExpiry,
            aggregatable_report_window_time_ - source_time);

  // The impression must expire strictly after it occurred.
  DCHECK_GT(expiry_time_, source_time);
  DCHECK_GT(event_report_window_time_, source_time);
  DCHECK_GT(aggregatable_report_window_time_, source_time);
}

StoredSource::~StoredSource() = default;

StoredSource::StoredSource(const StoredSource&) = default;

StoredSource::StoredSource(StoredSource&&) = default;

StoredSource& StoredSource::operator=(const StoredSource&) = default;

StoredSource& StoredSource::operator=(StoredSource&&) = default;

}  // namespace content
