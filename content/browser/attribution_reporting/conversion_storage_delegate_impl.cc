// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/conversion_storage_delegate_impl.h"

#include "base/rand_util.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/conversion_policy.h"
#include "content/browser/attribution_reporting/conversion_utils.h"

namespace content {

namespace {

using AttributionType = ::content::ConversionStorage::AttributionType;

}  // namespace

ConversionStorageDelegateImpl::ConversionStorageDelegateImpl(bool debug_mode)
    : debug_mode_(debug_mode) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

int ConversionStorageDelegateImpl::GetMaxConversionsPerImpression(
    StorableSource::SourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (source_type) {
    case StorableSource::SourceType::kNavigation:
      return 3;
    case StorableSource::SourceType::kEvent:
      return 1;
  }
}

int ConversionStorageDelegateImpl::GetMaxImpressionsPerOrigin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return 1024;
}

int ConversionStorageDelegateImpl::GetMaxConversionsPerOrigin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return 1024;
}

int ConversionStorageDelegateImpl::GetMaxAttributionDestinationsPerEventSource()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(apaseltiner): Finalize a value for this.
  return INT_MAX;
}

ConversionStorage::Delegate::RateLimitConfig
ConversionStorageDelegateImpl::GetRateLimits(
    AttributionType attribution_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(csharrison): Finalize `max_contributions_per_window` value.
  switch (attribution_type) {
    case AttributionType::kNavigation:
    case AttributionType::kEvent:
      return {
          .time_window = base::TimeDelta::FromDays(30),
          .max_contributions_per_window = 100,
      };
    case AttributionType::kAggregate:
      return {
          .time_window = base::TimeDelta::FromDays(7),
          .max_contributions_per_window = 65536,
      };
  }
}

uint64_t ConversionStorageDelegateImpl::GetFakeEventSourceTriggerData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ConversionPolicy().GetSanitizedConversionData(
      base::RandUint64(), StorableSource::SourceType::kEvent);
}

base::TimeDelta
ConversionStorageDelegateImpl::GetDeleteExpiredImpressionsFrequency() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::TimeDelta::FromMinutes(5);
}

base::TimeDelta
ConversionStorageDelegateImpl::GetDeleteExpiredRateLimitsFrequency() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::TimeDelta::FromMinutes(5);
}

base::Time ConversionStorageDelegateImpl::GetReportTime(
    const StorableSource& impression,
    base::Time conversion_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If in debug mode, the report should be sent immediately.
  if (debug_mode_)
    return conversion_time;
  return ComputeReportTime(impression, conversion_time);
}

}  // namespace content
