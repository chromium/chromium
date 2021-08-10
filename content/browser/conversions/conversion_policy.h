// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_

#include <stdint.h>
#include <memory>

#include "base/compiler_specific.h"
#include "content/browser/conversions/storable_impression.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class Time;
class TimeDelta;
}  // namespace base

namespace content {

// Class for controlling certain constraints and configurations for handling,
// storing, and sending impressions and conversions.
class CONTENT_EXPORT ConversionPolicy {
 public:
  // Helper class that generates noised conversion data. Can be overridden to
  // make testing deterministic.
  class CONTENT_EXPORT NoiseProvider {
   public:
    NoiseProvider() = default;
    virtual ~NoiseProvider() = default;

    // Returns a noised value of |conversion_data|. By default, this reports
    // completely random data for 5% of conversions, and sends the real data for
    // 95%. Virtual for testing.
    virtual uint64_t GetNoisedConversionData(uint64_t conversion_data) const
        WARN_UNUSED_RESULT;

    // Returns a noised value of |event_source_trigger_data|. By default, this
    // reports completely random data for 5% of conversions, and sends the real
    // data for 95%. Virtual for testing.
    virtual uint64_t GetNoisedEventSourceTriggerData(
        uint64_t event_source_trigger_data) const WARN_UNUSED_RESULT;

   private:
    friend class ConversionStorageDelegateImpl;

    // Returns a noised value of `event_source_trigger_data`. This reports
    // completely random data for 5% of conversions, and sends the real data for
    // 95%. Exposed for reuse in both
    // `ConversionStorageDelegateImpl::GetFakeEventSourceTriggerData()` and
    // `ConversionPolicy::NoiseProvider::GetNoisedEventSourceTriggerData()`.
    static uint64_t GetNoisedEventSourceTriggerDataImpl(
        uint64_t event_source_trigger_data) WARN_UNUSED_RESULT;
  };

  static std::unique_ptr<ConversionPolicy> CreateForTesting(
      std::unique_ptr<NoiseProvider> noise_provider) WARN_UNUSED_RESULT;

  // |debug_mode| indicates whether the API is currently running in a mode where
  // it should not use noise.
  explicit ConversionPolicy(bool debug_mode = false);
  ConversionPolicy(const ConversionPolicy& other) = delete;
  ConversionPolicy& operator=(const ConversionPolicy& other) = delete;
  ConversionPolicy(ConversionPolicy&& other) = delete;
  ConversionPolicy& operator=(ConversionPolicy&& other) = delete;
  virtual ~ConversionPolicy();

  // Gets the sanitized conversion data for a conversion. This strips entropy
  // from the provided to data to at most 3 bits of information.
  virtual uint64_t GetSanitizedConversionData(uint64_t conversion_data) const
      WARN_UNUSED_RESULT;

  // Gets the sanitized event source trigger data for a conversion.
  virtual uint64_t GetSanitizedEventSourceTriggerData(
      uint64_t event_source_trigger_data) const WARN_UNUSED_RESULT;

  // Gets the sanitized impression data for an impression.
  virtual uint64_t GetSanitizedImpressionData(uint64_t impression_data) const
      WARN_UNUSED_RESULT;

  // Returns the expiry time for an impression that is clamped to a maximum
  // value of 30 days from |impression_time|.
  virtual base::Time GetExpiryTimeForImpression(
      const absl::optional<base::TimeDelta>& declared_expiry,
      base::Time impression_time,
      StorableImpression::SourceType source_type) const WARN_UNUSED_RESULT;

  // Delays reports that missed their report time, such as the browser not being
  // open, or internet being disconnected. This given them a noisy report time
  // to help disassociate them from other reports.
  virtual base::Time GetReportTimeForReportPastSendTime(base::Time now) const
      WARN_UNUSED_RESULT;

  // Gets the maximum time a report can be held in storage after its report
  // time.
  virtual base::TimeDelta GetMaxReportAge() const WARN_UNUSED_RESULT;

 private:
  // For testing only.
  explicit ConversionPolicy(std::unique_ptr<NoiseProvider> noise_provider);

  // Whether the API is running in debug mode. No noise or delay should be used.
  const bool debug_mode_;

  std::unique_ptr<NoiseProvider> noise_provider_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_
