// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_

#include <stdint.h>

#include <vector>

#include "content/browser/attribution_reporting/attribution_aggregatable_trigger.h"
#include "content/browser/attribution_reporting/attribution_filter_data.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

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
    kMaxValue = kProhibitedByBrowserPolicy,
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
    kMaxValue = kProhibitedByBrowserPolicy,
  };

  struct CONTENT_EXPORT EventTriggerData {
    // Data associated with trigger.
    // Will be sanitized to a lower entropy by the `AttributionStorageDelegate`
    // before storage.
    uint64_t data;

    // Priority specified in conversion redirect. Used to prioritize which
    // reports to send among multiple different reports for the same attribution
    // source. Defaults to 0 if not provided.
    int64_t priority;

    // Key specified in conversion redirect for deduplication against existing
    // conversions with the same source. If absent, no deduplication is
    // performed.
    absl::optional<uint64_t> dedup_key;

    // The filters used to determine whether this `EventTriggerData'`s fields
    // are used.
    AttributionFilterData filters;

    // The negated filters used to determine whether this `EventTriggerData'`s
    // fields are used.
    AttributionFilterData not_filters;

    EventTriggerData(uint64_t data,
                     int64_t priority,
                     absl::optional<uint64_t> dedup_key,
                     AttributionFilterData filters,
                     AttributionFilterData not_filters);
  };

  // Should only be created with values that the browser process has already
  // validated. |conversion_destination| should be filled by a navigation origin
  // known by the browser process.
  AttributionTrigger(url::Origin destination_origin,
                     url::Origin reporting_origin,
                     AttributionFilterData filters,
                     absl::optional<uint64_t> debug_key,
                     std::vector<EventTriggerData> event_triggers,
                     AttributionAggregatableTrigger aggregatable_trigger);

  AttributionTrigger(const AttributionTrigger& other);
  AttributionTrigger& operator=(const AttributionTrigger& other);
  AttributionTrigger(AttributionTrigger&& other);
  AttributionTrigger& operator=(AttributionTrigger&& other);
  ~AttributionTrigger();

  const url::Origin& destination_origin() const { return destination_origin_; }

  const url::Origin& reporting_origin() const { return reporting_origin_; }

  const AttributionFilterData& filters() const { return filters_; }

  absl::optional<uint64_t> debug_key() const { return debug_key_; }

  const AttributionAggregatableTrigger& aggregatable_trigger() const {
    return aggregatable_trigger_;
  }

  void ClearDebugKey() { debug_key_ = absl::nullopt; }

  const std::vector<EventTriggerData>& event_triggers() const {
    return event_triggers_;
  }

 private:
  // Origin that this conversion event occurred on.
  url::Origin destination_origin_;

  // Origin of the conversion redirect url, and the origin that will receive any
  // reports.
  url::Origin reporting_origin_;

  AttributionFilterData filters_;

  absl::optional<uint64_t> debug_key_;

  std::vector<EventTriggerData> event_triggers_;

  AttributionAggregatableTrigger aggregatable_trigger_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_
