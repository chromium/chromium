// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_

#include <stdint.h>

#include <vector>

#include "components/attribution_reporting/aggregatable_values.h"
#include "components/attribution_reporting/filters.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace attribution_reporting {
class AggregatableTriggerData;
struct EventTriggerData;
}  // namespace attribution_reporting

namespace content {

// Struct which represents a conversion registration event that was observed in
// the renderer and is now being used by the browser process.
class CONTENT_EXPORT AttributionTrigger {
 public:
  // Represents the potential event-level outcomes from attempting to register
  // a trigger.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class EventLevelResult {
    kSuccess = 0,
    // The report was stored successfully, but it replaced an existing report
    // with a lower priority.
    kSuccessDroppedLowerPriority = 1,
    kInternalError = 2,
    kNoCapacityForConversionDestination = 3,
    kNoMatchingImpressions = 4,
    kDeduplicated = 5,
    kExcessiveAttributions = 6,
    kPriorityTooLow = 7,
    kDroppedForNoise = 8,
    kExcessiveReportingOrigins = 9,
    kNoMatchingSourceFilterData = 10,
    kProhibitedByBrowserPolicy = 11,
    kNoMatchingConfigurations = 12,
    kExcessiveReports = 13,
    kFalselyAttributedSource = 14,
    kMaxValue = kFalselyAttributedSource,
  };

  // Represents the potential aggregatable outcomes from attempting to register
  // a trigger.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AggregatableResult {
    kSuccess = 0,
    kInternalError = 1,
    kNoCapacityForConversionDestination = 2,
    kNoMatchingImpressions = 3,
    kExcessiveAttributions = 4,
    kExcessiveReportingOrigins = 5,
    kNoHistograms = 6,
    kInsufficientBudget = 7,
    kNoMatchingSourceFilterData = 8,
    kNotRegistered = 9,
    kProhibitedByBrowserPolicy = 10,
    kDeduplicated = 11,
    kMaxValue = kDeduplicated,
  };

  // Should only be created with values that the browser process has already
  // validated. |conversion_destination| should be filled by a navigation origin
  // known by the browser process.
  AttributionTrigger(
      url::Origin destination_origin,
      url::Origin reporting_origin,
      attribution_reporting::Filters filters,
      attribution_reporting::Filters not_filters,
      absl::optional<uint64_t> debug_key,
      absl::optional<uint64_t> aggregatable_dedup_key,
      std::vector<attribution_reporting::EventTriggerData> event_triggers,
      std::vector<attribution_reporting::AggregatableTriggerData>
          aggregatable_trigger_data,
      attribution_reporting::AggregatableValues aggregatable_values,
      bool is_within_fenced_frame,
      bool debug_reporting);

  AttributionTrigger(const AttributionTrigger&);
  AttributionTrigger& operator=(const AttributionTrigger&);
  AttributionTrigger(AttributionTrigger&&);
  AttributionTrigger& operator=(AttributionTrigger&&);
  ~AttributionTrigger();

  const url::Origin& destination_origin() const { return destination_origin_; }

  const url::Origin& reporting_origin() const { return reporting_origin_; }

  const attribution_reporting::Filters& filters() const { return filters_; }

  const attribution_reporting::Filters& not_filters() const {
    return not_filters_;
  }

  absl::optional<uint64_t> debug_key() const { return debug_key_; }

  absl::optional<uint64_t> aggregatable_dedup_key() const {
    return aggregatable_dedup_key_;
  }

  void ClearDebugKey() { debug_key_ = absl::nullopt; }

  const std::vector<attribution_reporting::EventTriggerData>& event_triggers()
      const {
    return event_triggers_;
  }

  const std::vector<attribution_reporting::AggregatableTriggerData>&
  aggregatable_trigger_data() const {
    return aggregatable_trigger_data_;
  }

  const attribution_reporting::AggregatableValues& aggregatable_values() const {
    return aggregatable_values_;
  }

  bool is_within_fenced_frame() const { return is_within_fenced_frame_; }

  bool debug_reporting() const { return debug_reporting_; }

 private:
  // Origin that this conversion event occurred on.
  url::Origin destination_origin_;

  // Origin of the conversion redirect url, and the origin that will receive any
  // reports.
  url::Origin reporting_origin_;

  attribution_reporting::Filters filters_;

  attribution_reporting::Filters not_filters_;

  absl::optional<uint64_t> debug_key_;

  // Key specified for deduplication against existing aggregatable reports with
  // the same source. If absent, no deduplication is performed.
  absl::optional<uint64_t> aggregatable_dedup_key_;

  std::vector<attribution_reporting::EventTriggerData> event_triggers_;

  std::vector<attribution_reporting::AggregatableTriggerData>
      aggregatable_trigger_data_;
  attribution_reporting::AggregatableValues aggregatable_values_;

  // Whether the trigger is registered within a fenced frame tree.
  bool is_within_fenced_frame_;

  // Whether debug reporting is enabled.
  bool debug_reporting_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_
