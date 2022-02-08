// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_IMPL_H_

#include <vector>

#include "base/sequence_checker.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/common/content_export.h"
#include "content/public/browser/attribution_reporting.h"

namespace content {

class CommonSourceInfo;

// Implementation of the storage delegate. This class handles assigning
// report times to newly created reports. It
// also controls constants for AttributionStorage. This is owned by
// AttributionStorageSql, and should only be accessed on the attribution storage
// task runner.
class CONTENT_EXPORT AttributionStorageDelegateImpl
    : public AttributionStorageDelegate {
 public:
  explicit AttributionStorageDelegateImpl(
      AttributionNoiseMode noise_mode = AttributionNoiseMode::kDefault,
      AttributionDelayMode delay_mode = AttributionDelayMode::kDefault);
  AttributionStorageDelegateImpl(const AttributionStorageDelegateImpl& other) =
      delete;
  AttributionStorageDelegateImpl& operator=(
      const AttributionStorageDelegateImpl& other) = delete;
  AttributionStorageDelegateImpl(AttributionStorageDelegateImpl&& other) =
      delete;
  AttributionStorageDelegateImpl& operator=(
      AttributionStorageDelegateImpl&& other) = delete;
  ~AttributionStorageDelegateImpl() override = default;

  // AttributionStorageDelegate:
  base::Time GetReportTime(const CommonSourceInfo& source,
                           base::Time trigger_time) const override;
  int GetMaxAttributionsPerSource(
      CommonSourceInfo::SourceType source_type) const override;
  int GetMaxSourcesPerOrigin() const override;
  int GetMaxAttributionsPerOrigin() const override;
  int GetMaxDestinationsPerSourceSiteReportingOrigin() const override;
  RateLimitConfig GetRateLimits() const override;
  base::TimeDelta GetDeleteExpiredSourcesFrequency() const override;
  base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const override;
  base::GUID NewReportID() const override;
  absl::optional<OfflineReportDelayConfig> GetOfflineReportDelayConfig()
      const override;
  void ShuffleReports(std::vector<AttributionReport>& reports) const override;
  RandomizedResponse GetRandomizedResponse(
      const CommonSourceInfo& source) const override;

  // Generates fake reports using a random "stars and bars" sequence index of a
  // possible output of the API.
  //
  // Exposed for testing.
  std::vector<FakeReport> GetRandomFakeReports(
      const CommonSourceInfo& source) const;

  // Generates fake reports from the "stars and bars" sequence index of a
  // possible output of the API. This output is determined by the following
  // algorithm:
  // 1. Find all stars before the first bar. These stars represent suppressed
  //    reports.
  // 2. For all other stars, count the number of bars that precede them. Each
  //    star represents a report where the reporting window and trigger data is
  //    uniquely determined by that number.
  //
  // Exposed for testing.
  std::vector<FakeReport> GetFakeReportsForSequenceIndex(
      const CommonSourceInfo& source,
      int random_stars_and_bars_sequence_index) const;

 private:
  const AttributionNoiseMode noise_mode_;
  const AttributionDelayMode delay_mode_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_IMPL_H_
