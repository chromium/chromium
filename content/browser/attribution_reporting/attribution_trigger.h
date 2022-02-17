// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_

#include <stdint.h>

#include "content/common/content_export.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace content {

// Struct which represents a conversion registration event that was observed in
// the renderer and is now being used by the browser process.
class CONTENT_EXPORT AttributionTrigger {
 public:
  // Represents the potential outcomes from attempting to register a trigger.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Result {
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
    kMaxValue = kExcessiveReportingOrigins,
  };

  // Should only be created with values that the browser process has already
  // validated. At creation time, |trigger_data_| should already be stripped
  // to a lower entropy. |conversion_destination| should be filled by a
  // navigation origin known by the browser process.
  AttributionTrigger(uint64_t trigger_data,
                     net::SchemefulSite conversion_destination,
                     url::Origin reporting_origin,
                     uint64_t event_source_trigger_data,
                     int64_t priority,
                     absl::optional<uint64_t> dedup_key,
                     absl::optional<uint64_t> debug_key);
  AttributionTrigger(const AttributionTrigger& other);
  AttributionTrigger& operator=(const AttributionTrigger& other);
  AttributionTrigger(AttributionTrigger&& other);
  AttributionTrigger& operator=(AttributionTrigger&& other);
  ~AttributionTrigger();

  uint64_t trigger_data() const { return trigger_data_; }

  const net::SchemefulSite& conversion_destination() const {
    return conversion_destination_;
  }

  const url::Origin& reporting_origin() const { return reporting_origin_; }

  uint64_t event_source_trigger_data() const {
    return event_source_trigger_data_;
  }

  int64_t priority() const { return priority_; }

  const absl::optional<uint64_t>& dedup_key() const { return dedup_key_; }

  const absl::optional<uint64_t>& debug_key() const { return debug_key_; }

  void ClearDebugKey() { debug_key_ = absl::nullopt; }

 private:
  // Data associated with trigger.
  uint64_t trigger_data_;

  // Schemeful site that this conversion event occurred on.
  net::SchemefulSite conversion_destination_;

  // Origin of the conversion redirect url, and the origin that will receive any
  // reports.
  url::Origin reporting_origin_;

  // Event source trigger data specified in conversion redirect. Defaults to 0
  // if not provided.
  uint64_t event_source_trigger_data_;

  // Priority specified in conversion redirect. Used to prioritize which reports
  // to send among multiple different reports for the same attribution source.
  // Defaults to 0 if not provided.
  int64_t priority_;

  // Key specified in conversion redirect for deduplication against existing
  // conversions with the same source. If absent, no deduplication is performed.
  absl::optional<uint64_t> dedup_key_;

  absl::optional<uint64_t> debug_key_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_
