// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"

#include "base/guid.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_policy.h"
#include "content/browser/attribution_reporting/attribution_utils.h"

namespace content {

namespace {

using AttributionType = ::content::AttributionStorage::AttributionType;

}  // namespace

AttributionStorageDelegateImpl::AttributionStorageDelegateImpl(bool debug_mode)
    : debug_mode_(debug_mode) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

int AttributionStorageDelegateImpl::GetMaxAttributionsPerSource(
    StorableSource::SourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (source_type) {
    case StorableSource::SourceType::kNavigation:
      return 3;
    case StorableSource::SourceType::kEvent:
      return 1;
  }
}

int AttributionStorageDelegateImpl::GetMaxSourcesPerOrigin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return 1024;
}

int AttributionStorageDelegateImpl::GetMaxAttributionsPerOrigin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return 1024;
}

int AttributionStorageDelegateImpl::
    GetMaxAttributionDestinationsPerEventSource() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(apaseltiner): Finalize a value for this.
  return INT_MAX;
}

AttributionStorage::Delegate::RateLimitConfig
AttributionStorageDelegateImpl::GetRateLimits(
    AttributionType attribution_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(csharrison): Finalize `max_contributions_per_window` value.
  switch (attribution_type) {
    case AttributionType::kNavigation:
    case AttributionType::kEvent:
      return {
          .time_window = base::Days(30),
          .max_contributions_per_window = 100,
      };
    case AttributionType::kAggregate:
      return {
          .time_window = base::Days(7),
          .max_contributions_per_window = 65536,
      };
  }
}

uint64_t AttributionStorageDelegateImpl::GetFakeEventSourceTriggerData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return AttributionPolicy().SanitizeTriggerData(
      base::RandUint64(), StorableSource::SourceType::kEvent);
}

base::TimeDelta
AttributionStorageDelegateImpl::GetDeleteExpiredSourcesFrequency() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Minutes(5);
}

base::TimeDelta
AttributionStorageDelegateImpl::GetDeleteExpiredRateLimitsFrequency() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Minutes(5);
}

base::Time AttributionStorageDelegateImpl::GetReportTime(
    const StorableSource& source,
    base::Time trigger_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If in debug mode, the report should be sent immediately.
  if (debug_mode_)
    return trigger_time;
  return ComputeReportTime(source, trigger_time);
}

base::GUID AttributionStorageDelegateImpl::NewReportID() const {
  return base::GUID::GenerateRandomV4();
}

}  // namespace content
