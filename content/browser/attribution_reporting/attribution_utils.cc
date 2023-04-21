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

base::Time ComputeReportTime(base::Time source_time,
                             base::Time trigger_time,
                             base::span<const base::TimeDelta> deadlines) {
  // Follows the steps detailed in
  // https://wicg.github.io/attribution-reporting-api/#obtain-an-event-level-report-delivery-time
  // Starting from step 2.
  DCHECK(!deadlines.empty());
  base::TimeDelta deadline_to_use = deadlines.back();
  for (base::TimeDelta deadline : deadlines) {
    if (source_time + deadline < trigger_time) {
      continue;
    }
    deadline_to_use = deadline;
    break;
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
