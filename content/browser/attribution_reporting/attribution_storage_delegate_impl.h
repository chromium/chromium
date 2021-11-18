// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_IMPL_H_

#include "base/sequence_checker.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/common/content_export.h"

namespace base {
class GUID;
class Time;
}  // namespace base

namespace content {

// Implementation of the storage delegate. This class handles assigning
// report times to newly created reports. It
// also controls constants for AttributionStorage. This is owned by
// AttributionStorageSql, and should only be accessed on the attribution storage
// task runner.
class CONTENT_EXPORT AttributionStorageDelegateImpl
    : public AttributionStorage::Delegate {
 public:
  explicit AttributionStorageDelegateImpl(bool debug_mode = false);
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
  base::Time GetReportTime(const StorableSource& source,
                           base::Time trigger_time) const override;
  int GetMaxAttributionsPerSource(
      StorableSource::SourceType source_type) const override;
  int GetMaxSourcesPerOrigin() const override;
  int GetMaxAttributionsPerOrigin() const override;
  int GetMaxAttributionDestinationsPerEventSource() const override;
  RateLimitConfig GetRateLimits(
      AttributionStorage::AttributionType attribution_type) const override;
  uint64_t GetFakeEventSourceTriggerData() const override;
  base::TimeDelta GetDeleteExpiredSourcesFrequency() const override;
  base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const override;
  base::GUID NewReportID() const override;

 private:
  // Whether the API is running in debug mode, meaning that there should be
  // no delays or noise added to reports.
  const bool debug_mode_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_IMPL_H_
