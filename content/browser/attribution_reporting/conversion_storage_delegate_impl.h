// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_CONVERSION_STORAGE_DELEGATE_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_CONVERSION_STORAGE_DELEGATE_IMPL_H_

#include "base/sequence_checker.h"
#include "content/browser/attribution_reporting/conversion_storage.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/common/content_export.h"

namespace base {
class Time;
}  // namespace base

namespace content {

// Implementation of the storage delegate. This class handles assigning
// report times to newly created conversion reports. It
// also controls constants for ConversionStorage. This is owned by
// ConversionStorageSql, and should only be accessed on the conversions storage
// task runner.
class CONTENT_EXPORT ConversionStorageDelegateImpl
    : public ConversionStorage::Delegate {
 public:
  explicit ConversionStorageDelegateImpl(bool debug_mode = false);
  ConversionStorageDelegateImpl(const ConversionStorageDelegateImpl& other) =
      delete;
  ConversionStorageDelegateImpl& operator=(
      const ConversionStorageDelegateImpl& other) = delete;
  ConversionStorageDelegateImpl(ConversionStorageDelegateImpl&& other) = delete;
  ConversionStorageDelegateImpl& operator=(
      ConversionStorageDelegateImpl&& other) = delete;
  ~ConversionStorageDelegateImpl() override = default;

  // ConversionStorageDelegate:
  base::Time GetReportTime(const StorableSource& impression,
                           base::Time conversion_time) const override;
  int GetMaxConversionsPerImpression(
      StorableSource::SourceType source_type) const override;
  int GetMaxImpressionsPerOrigin() const override;
  int GetMaxConversionsPerOrigin() const override;
  int GetMaxAttributionDestinationsPerEventSource() const override;
  RateLimitConfig GetRateLimits(
      ConversionStorage::AttributionType attribution_type) const override;
  uint64_t GetFakeEventSourceTriggerData() const override;
  base::TimeDelta GetDeleteExpiredImpressionsFrequency() const override;
  base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const override;

 private:
  // Whether the API is running in debug mode, meaning that there should be
  // no delays or noise added to reports.
  const bool debug_mode_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_CONVERSION_STORAGE_DELEGATE_IMPL_H_
