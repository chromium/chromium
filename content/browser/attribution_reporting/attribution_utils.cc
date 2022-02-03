// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_utils.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/time/time.h"

namespace content {

namespace {

constexpr base::TimeDelta kWindowDeadlineOffset = base::Hours(1);

base::span<const base::TimeDelta> EarlyDeadlines(
    CommonSourceInfo::SourceType source_type) {
  static constexpr base::TimeDelta kEarlyDeadlinesNavigation[] = {
      base::Days(2) - kWindowDeadlineOffset,
      base::Days(7) - kWindowDeadlineOffset,
  };

  switch (source_type) {
    case CommonSourceInfo::SourceType::kNavigation:
      return kEarlyDeadlinesNavigation;
    case CommonSourceInfo::SourceType::kEvent:
      return base::span<const base::TimeDelta>();
  }
}

base::TimeDelta ExpiryDeadline(const CommonSourceInfo& source) {
  base::TimeDelta expiry_deadline =
      source.expiry_time() - source.impression_time();

  constexpr base::TimeDelta kMinExpiryDeadline = base::Days(2);
  if (expiry_deadline < kMinExpiryDeadline)
    expiry_deadline = kMinExpiryDeadline;

  return expiry_deadline;
}

base::Time ReportTimeFromDeadline(base::Time impression_time,
                                  base::TimeDelta deadline) {
  // Valid conversion reports should always have a valid reporting deadline.
  DCHECK(!deadline.is_zero());
  return impression_time + deadline + kWindowDeadlineOffset;
}

}  // namespace

base::Time ComputeReportTime(const CommonSourceInfo& source,
                             base::Time trigger_time) {
  base::TimeDelta expiry_deadline = ExpiryDeadline(source);

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
  base::TimeDelta deadline_to_use = expiry_deadline;

  // Given a conversion that happened at `trigger_time`, find the first
  // applicable reporting window this conversion should be reported at.
  for (base::TimeDelta early_deadline : EarlyDeadlines(source.source_type())) {
    // If this window is valid for the conversion, use it.
    // |trigger_time| is roughly ~now.
    if (source.impression_time() + early_deadline >= trigger_time &&
        early_deadline < deadline_to_use) {
      deadline_to_use = early_deadline;
      break;
    }
  }

  return ReportTimeFromDeadline(source.impression_time(), deadline_to_use);
}

int NumReportWindows(CommonSourceInfo::SourceType source_type) {
  // Add 1 for the expiry deadline.
  return 1 + EarlyDeadlines(source_type).size();
}

base::Time ReportTimeAtWindow(const CommonSourceInfo& source,
                              int window_index) {
  DCHECK_GE(window_index, 0);
  DCHECK_LT(window_index, NumReportWindows(source.source_type()));

  base::span<const base::TimeDelta> early_deadlines =
      EarlyDeadlines(source.source_type());

  base::TimeDelta deadline =
      static_cast<size_t>(window_index) < early_deadlines.size()
          ? early_deadlines[window_index]
          : ExpiryDeadline(source);

  return ReportTimeFromDeadline(source.impression_time(), deadline);
}

uint64_t TriggerDataCardinality(CommonSourceInfo::SourceType source_type) {
  switch (source_type) {
    case CommonSourceInfo::SourceType::kNavigation:
      return 8;
    case CommonSourceInfo::SourceType::kEvent:
      return 2;
  }
}

double RandomizedTriggerRate(CommonSourceInfo::SourceType source_type) {
  // Note: When these values are changed from .0024 and/or .0000025, update
  // `AttributionReport::ReportBody()` per the comment there.
  switch (source_type) {
    case CommonSourceInfo::SourceType::kNavigation:
      return .0024;
    case CommonSourceInfo::SourceType::kEvent:
      return .0000025;
  }
}

std::string SerializeAttributionJson(const base::Value& body,
                                     bool pretty_print) {
  int options = pretty_print ? base::JSONWriter::OPTIONS_PRETTY_PRINT : 0;

  std::string output_json;
  bool success =
      base::JSONWriter::WriteWithOptions(body, options, &output_json);
  DCHECK(success);
  return output_json;
}

}  // namespace content
