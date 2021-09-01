// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_

#include <stdint.h>

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
  // |debug_mode| indicates whether the API is currently running in a mode where
  // it should not use noise.
  explicit ConversionPolicy(bool debug_mode = false);
  ConversionPolicy(const ConversionPolicy& other) = delete;
  ConversionPolicy& operator=(const ConversionPolicy& other) = delete;
  ConversionPolicy(ConversionPolicy&& other) = delete;
  ConversionPolicy& operator=(ConversionPolicy&& other) = delete;
  virtual ~ConversionPolicy();

  // Gets the sanitized conversion data for a conversion.
  uint64_t GetSanitizedConversionData(
      uint64_t conversion_data,
      StorableImpression::SourceType source_type) const WARN_UNUSED_RESULT;

  bool IsConversionDataInRange(uint64_t conversion_data,
                               StorableImpression::SourceType source_type) const
      WARN_UNUSED_RESULT;

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

 protected:
  virtual bool ShouldNoiseConversionData() const WARN_UNUSED_RESULT;

  virtual uint64_t MakeNoisedConversionData(uint64_t max) const
      WARN_UNUSED_RESULT;

 private:
  // Whether the API is running in debug mode. No noise or delay should be used.
  const bool debug_mode_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_
