// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_POLICY_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_POLICY_H_

#include <stdint.h>

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

  uint64_t SanitizeTriggerData(uint64_t trigger_data,
                               StorableSource::SourceType source_type) const;

  bool IsTriggerDataInRange(uint64_t trigger_data,
                            StorableSource::SourceType source_type) const;

  // Returns the expiry time for an impression that is clamped to a maximum
  // value of 30 days from |impression_time|.
  base::Time GetExpiryTimeForImpression(
      const absl::optional<base::TimeDelta>& declared_expiry,
      base::Time impression_time,
      StorableSource::SourceType source_type) const;

  // Both bounds are inclusive.
  struct OfflineReportDelayConfig {
    base::TimeDelta min;
    base::TimeDelta max;
  };

  // Delays reports that missed their report time, such as the browser not being
  // open, or internet being disconnected. This given them a noisy report time
  // to help disassociate them from other reports. Returns null if no delay
  // should be applied, e.g. because the policy is in debug mode.
  virtual absl::optional<OfflineReportDelayConfig> GetOfflineReportDelayConfig()
      const;

  // Gets the delay for a report that has failed to be sent
  // `failed_send_attempts` times.
  // Returns `absl::nullopt` to indicate that no more attempts should be made.
  // Otherwise, the return value must be positive. `failed_send_attempts` is
  // guaranteed to be positive.
  absl::optional<base::TimeDelta> GetFailedReportDelay(
      int failed_send_attempts) const;

  class CONTENT_EXPORT AttributionMode {
   public:
    explicit AttributionMode(
        StorableSource::AttributionLogic logic,
        absl::optional<uint64_t> fake_trigger_data = absl::nullopt);

    ~AttributionMode();

    AttributionMode(const AttributionMode&);
    AttributionMode(AttributionMode&&);

    AttributionMode& operator=(const AttributionMode&);
    AttributionMode& operator=(AttributionMode&&);

    StorableSource::AttributionLogic logic() const { return logic_; }

    // `absl::nullopt` when `logic()` is not `AttributionLogic::kFalsely`.
    absl::optional<uint64_t> fake_trigger_data() const {
      return fake_trigger_data_;
    }

   private:
    StorableSource::AttributionLogic logic_;
    absl::optional<uint64_t> fake_trigger_data_;
  };

  // Selects how to handle the given source type; may involve RNG or other
  // dynamic criteria.
  AttributionMode GetAttributionMode(
      StorableSource::SourceType source_type) const;

 protected:
  virtual bool ShouldNoiseTriggerData() const;

  virtual uint64_t MakeNoisedTriggerData(uint64_t max) const;

 private:
  // Whether the API is running in debug mode. No noise or delay should be used.
  const bool debug_mode_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_POLICY_H_
