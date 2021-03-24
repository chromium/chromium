// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_storage_delegate_impl.h"

namespace content {

ConversionStorageDelegateImpl::ConversionStorageDelegateImpl(bool debug_mode)
    : debug_mode_(debug_mode) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void ConversionStorageDelegateImpl::ProcessNewConversionReports(
    std::vector<ConversionReport>* reports) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!reports->empty());
  ConversionReport* last_report = &(reports->at(0));

  // Assign attribution credits to each report that will be sent at report time.
  // This performs "last click" attribution which assigns the report
  // for the most recent impression a credit of 100, and the rest a credit of 0.
  for (ConversionReport& report : *reports) {
    report.report_time = GetReportTimeForConversion(report);

    report.attribution_credit = 0;
    if (report.impression.impression_time() >
        last_report->impression.impression_time())
      last_report = &report;
  }

  last_report->attribution_credit = 100;
}

int ConversionStorageDelegateImpl::GetMaxConversionsPerImpression() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return 3;
}

int ConversionStorageDelegateImpl::GetMaxImpressionsPerOrigin() const {
  return 1024;
}

int ConversionStorageDelegateImpl::GetMaxConversionsPerOrigin() const {
  return 1024;
}

ConversionStorage::Delegate::RateLimitConfig
ConversionStorageDelegateImpl::GetRateLimits() const {
  // TODO(csharrison): Finalize max_attributions_per_window value.
  return {
      .time_window = base::TimeDelta::FromDays(30),
      .max_attributions_per_window = 100,
  };
}

base::Time ConversionStorageDelegateImpl::GetReportTimeForConversion(
    const ConversionReport& report) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  //  |report.report_time| is roughly ~now, for newly created conversion
  //  reports. If in debug mode, the report should be sent immediately.
  if (debug_mode_)
    return report.report_time;

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
  constexpr base::TimeDelta kWindowDeadlineOffset =
      base::TimeDelta::FromHours(1);
  base::TimeDelta expiry_deadline =
      report.impression.expiry_time() - report.impression.impression_time();
  const base::TimeDelta kReportingWindowDeadlines[] = {
      base::TimeDelta::FromDays(2) - kWindowDeadlineOffset,
      base::TimeDelta::FromDays(7) - kWindowDeadlineOffset, expiry_deadline};

  base::TimeDelta deadline_to_use;

  // Given a conversion report that was created at |report.report_time|, find
  // the first applicable reporting window this conversion should be reported
  // at.
  for (base::TimeDelta report_window_deadline : kReportingWindowDeadlines) {
    // If this window is valid for the conversion, use it. |report.report_time|
    // is roughly ~now, as the conversion time is used as the default value for
    // newly created reports that have not had a report time set.
    if (report.impression.impression_time() + report_window_deadline >=
        report.report_time) {
      deadline_to_use = report_window_deadline;
      break;
    }
  }

  // Valid conversion reports should always have a valid reporting deadline.
  DCHECK(!deadline_to_use.is_zero());

  // If the expiry deadline falls after the first window, but before another
  // window, use it instead. For example, if expiry is at 3 days, we can send
  // reports at the 2 day deadline and the expiry deadline instead of at the 7
  // day deadline.
  if (expiry_deadline > kReportingWindowDeadlines[0] &&
      expiry_deadline < deadline_to_use) {
    deadline_to_use = expiry_deadline;
  }

  return report.impression.impression_time() + deadline_to_use +
         kWindowDeadlineOffset;
}

}  // namespace content
