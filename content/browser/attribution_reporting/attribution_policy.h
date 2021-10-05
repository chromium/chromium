// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_POLICY_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_POLICY_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Time;
class TimeDelta;
}  // namespace base

namespace content {

// Class for controlling certain constraints and configurations for handling,
// storing, and sending impressions and conversions.
class CONTENT_EXPORT AttributionPolicy {
 public:
  // |debug_mode| indicates whether the API is currently running in a mode where
  // it should not use noise.
  explicit AttributionPolicy(bool debug_mode = false);
  AttributionPolicy(const AttributionPolicy& other) = delete;
  AttributionPolicy& operator=(const AttributionPolicy& other) = delete;
  AttributionPolicy(AttributionPolicy&& other) = delete;
  AttributionPolicy& operator=(AttributionPolicy&& other) = delete;
  virtual ~AttributionPolicy();

  // Gets the sanitized conversion data for a conversion.
  uint64_t GetSanitizedConversionData(
      uint64_t conversion_data,
      StorableSource::SourceType source_type) const WARN_UNUSED_RESULT;

  bool IsConversionDataInRange(uint64_t conversion_data,
                               StorableSource::SourceType source_type) const
      WARN_UNUSED_RESULT;

  // Gets the sanitized impression data for an impression.
  virtual uint64_t GetSanitizedImpressionData(uint64_t impression_data) const
      WARN_UNUSED_RESULT;

  // Returns the expiry time for an impression that is clamped to a maximum
  // value of 30 days from |impression_time|.
  virtual base::Time GetExpiryTimeForImpression(
      const absl::optional<base::TimeDelta>& declared_expiry,
      base::Time impression_time,
      StorableSource::SourceType source_type) const WARN_UNUSED_RESULT;

  // Delays reports that missed their report time, such as the browser not being
  // open, or internet being disconnected. This given them a noisy report time
  // to help disassociate them from other reports.
  virtual base::Time GetReportTimeForReportPastSendTime(base::Time now) const
      WARN_UNUSED_RESULT;

  // Gets the delay for a report that has failed to be sent
  // `failed_send_attempts` times.
  // Returns `absl::nullopt` to indicate that no more attempts should be made.
  // Otherwise, the return value must be positive. `failed_send_attempts` is
  // guaranteed to be positive.
  virtual absl::optional<base::TimeDelta> GetFailedReportDelay(
      int failed_send_attempts) const WARN_UNUSED_RESULT;

  // Selects how to handle the given impression; may involve RNG or other
  // dynamic criteria.
  virtual StorableSource::AttributionLogic GetAttributionLogicForImpression(
      StorableSource::SourceType source_type) const WARN_UNUSED_RESULT;

 protected:
  virtual bool ShouldNoiseConversionData() const WARN_UNUSED_RESULT;

  virtual uint64_t MakeNoisedConversionData(uint64_t max) const
      WARN_UNUSED_RESULT;

 private:
  // Whether the API is running in debug mode. No noise or delay should be used.
  const bool debug_mode_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_POLICY_H_
