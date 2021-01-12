// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_
#define CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

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

    // Returns a noise value of |conversion_data|. By default, this reports
    // completely random data for 5% of conversions, and sends the real data for
    // 95%. Virtual for testing.
    virtual uint64_t GetNoisedConversionData(uint64_t conversion_data) const;
  };

  static std::unique_ptr<ConversionPolicy> CreateForTesting(
      std::unique_ptr<NoiseProvider> noise_provider);

  // |debug_mode| indicates whether the API is currently running in a mode where
  // it should not use noise.
  explicit ConversionPolicy(bool debug_mode = false);
  ConversionPolicy(const ConversionPolicy& other) = delete;
  ConversionPolicy& operator=(const ConversionPolicy& other) = delete;
  virtual ~ConversionPolicy();

  // Gets the sanitized conversion data for a conversion. This strips entropy
  // from the provided to data to at most 3 bits of information.
  virtual std::string GetSanitizedConversionData(
      uint64_t conversion_data) const;

  // Gets the sanitized impression data for an impression. Returns the decoded
  // number as a hexadecimal string.
  virtual std::string GetSanitizedImpressionData(
      uint64_t impression_data) const;

  // Returns the expiry time for an impression that is clamped to a maximum
  // value of 30 days from |impression_time|.
  virtual base::Time GetExpiryTimeForImpression(
      const base::Optional<base::TimeDelta>& declared_expiry,
      base::Time impression_time) const;

  // Delays reports that should have been sent while the browser was not open by
  // given them a noisy report time to help disassociate them from other
  // reports.
  virtual base::Time GetReportTimeForExpiredReportAtStartup(
      base::Time now) const;

 private:
  // For testing only.
  explicit ConversionPolicy(std::unique_ptr<NoiseProvider> noise_provider);

  // Whether the API is running in debug mode. No noise or delay should be used.
  const bool debug_mode_;

  std::unique_ptr<NoiseProvider> noise_provider_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONVERSIONS_CONVERSION_POLICY_H_
