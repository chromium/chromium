// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage_delegate_impl.h"

#include "base/rand_util.h"
#include "base/time/time.h"
#include "content/browser/conversions/conversion_policy.h"
#include "content/browser/conversions/conversion_report.h"

namespace content {

ConversionStorageDelegateImpl::ConversionStorageDelegateImpl(bool debug_mode)
    : debug_mode_(debug_mode) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

int ConversionStorageDelegateImpl::GetMaxConversionsPerImpression(
    StorableImpression::SourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (source_type) {
    case StorableImpression::SourceType::kNavigation:
      return 3;
    case StorableImpression::SourceType::kEvent:
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
ConversionStorageDelegateImpl::GetRateLimits() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(csharrison): Finalize max_attributions_per_window value.
  return {
      .time_window = base::TimeDelta::FromDays(30),
      .max_attributions_per_window = 100,
  };
}

StorableImpression::AttributionLogic
ConversionStorageDelegateImpl::SelectAttributionLogic(
    const StorableImpression& impression) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (debug_mode_)
    return StorableImpression::AttributionLogic::kTruthfully;

  switch (impression.source_type()) {
    case StorableImpression::SourceType::kNavigation:
      return StorableImpression::AttributionLogic::kTruthfully;
    case StorableImpression::SourceType::kEvent: {
      // TODO(apaseltiner): Finalize a value for this so that noise is actually
      // triggered.
      const double kNoise = 0;
      if (base::RandDouble() < (1 - kNoise))
        return StorableImpression::AttributionLogic::kTruthfully;
      if (base::RandInt(0, 1) == 0)
        return StorableImpression::AttributionLogic::kNever;
      return StorableImpression::AttributionLogic::kFalsely;
    }
  }
}

uint64_t ConversionStorageDelegateImpl::GetFakeEventSourceTriggerData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ConversionPolicy::NoiseProvider::GetNoisedEventSourceTriggerDataImpl(
      base::RandUint64());
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
    const ConversionReport& report) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  //  |report.report_time| is roughly ~now, for newly created conversion
  //  reports. If in debug mode, the report should be sent immediately.
  if (debug_mode_)
    return report.report_time;

  base::TimeDelta expiry_deadline =
      report.impression.expiry_time() - report.impression.impression_time();

  constexpr base::TimeDelta kMinExpiryDeadline = base::TimeDelta::FromDays(2);
  if (expiry_deadline < kMinExpiryDeadline)
    expiry_deadline = kMinExpiryDeadline;

  // After the initial impression, a schedule of reporting windows and deadlines
  // associated with that impression begins. The time between impression time
  // and impression expiry is split into multiple reporting windows. At the end
  // of each window, the browser will send all scheduled reports for that
  // impression.
  //
  // Each reporting window has a deadline and only conversions registered before
  // that deadline are sent in that window. Each deadline is one hour prior to
  // the window report time. The deadlines relative to impression time are <2
  // days minus 1 hour, 7 days minus 1 hour, impression expiry>. The impression
  // expiry window is only used for conversions that occur after the 7 day
  // deadline. For example, a conversion which happens one hour after an
  // impression with an expiry of two hours, is still reported in the 2 day
  // window.
  //
  // Note that only navigation (not event) sources have early reporting
  // deadlines.
  constexpr base::TimeDelta kWindowDeadlineOffset =
      base::TimeDelta::FromHours(1);

  std::vector<base::TimeDelta> early_deadlines;
  switch (report.impression.source_type()) {
    case StorableImpression::SourceType::kNavigation:
      early_deadlines = {base::TimeDelta::FromDays(2) - kWindowDeadlineOffset,
                         base::TimeDelta::FromDays(7) - kWindowDeadlineOffset};
      break;
    case StorableImpression::SourceType::kEvent:
      early_deadlines = {};
      break;
  }

  base::TimeDelta deadline_to_use = expiry_deadline;

  // Given a conversion report that was created at |report.report_time|, find
  // the first applicable reporting window this conversion should be reported
  // at.
  for (base::TimeDelta early_deadline : early_deadlines) {
    // If this window is valid for the conversion, use it. |report.report_time|
    // is roughly ~now, as the conversion time is used as the default value for
    // newly created reports that have not had a report time set.
    if (report.impression.impression_time() + early_deadline >=
            report.report_time &&
        early_deadline < deadline_to_use) {
      deadline_to_use = early_deadline;
      break;
    }
  }

  // Valid conversion reports should always have a valid reporting deadline.
  DCHECK(!deadline_to_use.is_zero());

  return report.impression.impression_time() + deadline_to_use +
         kWindowDeadlineOffset;
}

}  // namespace content
