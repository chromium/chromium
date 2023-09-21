// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_

#include <vector>

#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/trigger_verification.h"

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
    // Equivalent to the "noised" trigger result described in the spec.
    kNeverAttributedSource = 8,
    kExcessiveReportingOrigins = 9,
    kNoMatchingSourceFilterData = 10,
    kProhibitedByBrowserPolicy = 11,
    kNoMatchingConfigurations = 12,
    kExcessiveReports = 13,
    kFalselyAttributedSource = 14,
    kReportWindowPassed = 15,
    kNotRegistered = 16,
    kReportWindowNotStarted = 17,
    kMaxValue = kReportWindowNotStarted,
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
    kReportWindowPassed = 12,
    kExcessiveReports = 13,
    kMaxValue = kExcessiveReports,
  };

  AttributionTrigger(attribution_reporting::SuitableOrigin reporting_origin,
                     attribution_reporting::TriggerRegistration registration,
                     attribution_reporting::SuitableOrigin destination_origin,
                     std::vector<network::TriggerVerification> verifications,
                     bool is_within_fenced_frame);

  AttributionTrigger(const AttributionTrigger&);
  AttributionTrigger& operator=(const AttributionTrigger&);
  AttributionTrigger(AttributionTrigger&&);
  AttributionTrigger& operator=(AttributionTrigger&&);
  ~AttributionTrigger();

  const attribution_reporting::SuitableOrigin& reporting_origin() const {
    return reporting_origin_;
  }

  const attribution_reporting::TriggerRegistration& registration() const {
    return registration_;
  }

  attribution_reporting::TriggerRegistration& registration() {
    return registration_;
  }

  const attribution_reporting::SuitableOrigin& destination_origin() const {
    return destination_origin_;
  }

  bool is_within_fenced_frame() const { return is_within_fenced_frame_; }

  const std::vector<network::TriggerVerification>& verifications() const {
    return verifications_;
  }

 private:
  attribution_reporting::SuitableOrigin reporting_origin_;

  attribution_reporting::TriggerRegistration registration_;

  // Origin on which this trigger was registered.
  attribution_reporting::SuitableOrigin destination_origin_;

  // Optional tokens attesting to the veracity of the trigger.
  std::vector<network::TriggerVerification> verifications_;

  // Whether the trigger is registered within a fenced frame tree.
  bool is_within_fenced_frame_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_TRIGGER_H_
