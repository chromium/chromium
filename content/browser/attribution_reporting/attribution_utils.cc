// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_utils.h"

#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

constexpr base::TimeDelta kWindowDeadlineOffset = base::Hours(1);

}  // namespace

base::TimeDelta ExpiryDeadline(base::Time source_time,
                               base::Time event_report_window_time) {
  DCHECK_GT(event_report_window_time, source_time);
  return event_report_window_time - source_time;
}

base::Time ReportTimeFromDeadline(base::Time source_time,
                                  base::TimeDelta deadline) {
  // Valid conversion reports should always have a valid reporting deadline.
  DCHECK(!deadline.is_zero());
  return source_time + deadline + kWindowDeadlineOffset;
}

base::Time ComputeReportTime(
    base::Time source_time,
    base::Time event_report_window_time,
    base::Time trigger_time,
    base::span<const base::TimeDelta> early_deadlines) {
  base::TimeDelta expiry_deadline =
      ExpiryDeadline(source_time, event_report_window_time);

  // After the initial impression, a schedule of reporting windows and deadlines
  // associated with that impression begins. The time between impression time
  // and impression expiry is split into multiple reporting windows whose values
  // are defined in `kEarlyDeadlinesNavigation`. At the end of each window, the
  // browser will send all scheduled reports for that impression.
  //
  // Each reporting window has a deadline and only conversions registered before
  // that deadline are sent in that window. Each deadline is at the window
  // report time. The deadlines relative to impression time are <first report
  // window, second report window, impression expiry>. The impression expiry
  // window is only used for conversions that occur after the second report
  // window. For example, a conversion which happens one hour after an
  // impression with an expiry of two hours, is still reported in the first
  // report window.
  //
  // Note that only navigation (not event) sources have early reporting
  // deadlines.
  base::TimeDelta deadline_to_use = expiry_deadline;

  // Given a conversion that happened at `trigger_time`, find the first
  // applicable reporting window this conversion should be reported at.
  for (base::TimeDelta early_deadline : early_deadlines) {
    // If this window is valid for the conversion, use it.
    // |trigger_time| is roughly ~now.
    if (source_time + early_deadline >= trigger_time &&
        early_deadline < deadline_to_use) {
      deadline_to_use = early_deadline;
      break;
    }
  }

  return ReportTimeFromDeadline(source_time, deadline_to_use);
}

base::Time LastTriggerTimeForReportTime(base::Time report_time) {
  return report_time - kWindowDeadlineOffset;
}

std::string SerializeAttributionJson(base::ValueView body, bool pretty_print) {
  int options = pretty_print ? base::JSONWriter::OPTIONS_PRETTY_PRINT : 0;

  std::string output_json;
  bool success =
      base::JSONWriter::WriteWithOptions(body, options, &output_json);
  DCHECK(success);
  return output_json;
}

base::Time ComputeReportWindowTime(
    absl::optional<base::Time> report_window_time,
    base::Time expiry_time) {
  return report_window_time.has_value() &&
                 report_window_time.value() <= expiry_time
             ? report_window_time.value()
             : expiry_time;
}

}  // namespace content
