// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_POLICY_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_POLICY_H_

#include <stdint.h>

#include "base/time/time.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Class for controlling certain constraints and configurations for handling,
// storing, and sending impressions and conversions.
class CONTENT_EXPORT AttributionPolicy {
 public:
  AttributionPolicy();
  AttributionPolicy(const AttributionPolicy& other) = delete;
  AttributionPolicy& operator=(const AttributionPolicy& other) = delete;
  AttributionPolicy(AttributionPolicy&& other) = delete;
  AttributionPolicy& operator=(AttributionPolicy&& other) = delete;
  ~AttributionPolicy();

  uint64_t SanitizeTriggerData(uint64_t trigger_data,
                               CommonSourceInfo::SourceType source_type) const;

  // Returns the expiry time for an impression that is clamped to a maximum
  // value of 30 days from |impression_time|.
  base::Time GetExpiryTimeForImpression(
      const absl::optional<base::TimeDelta>& declared_expiry,
      base::Time impression_time,
      CommonSourceInfo::SourceType source_type) const;

  // Gets the delay for a report that has failed to be sent
  // `failed_send_attempts` times.
  // Returns `absl::nullopt` to indicate that no more attempts should be made.
  // Otherwise, the return value must be positive. `failed_send_attempts` is
  // guaranteed to be positive.
  absl::optional<base::TimeDelta> GetFailedReportDelay(
      int failed_send_attempts) const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_POLICY_H_
