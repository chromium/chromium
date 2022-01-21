// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_utils.h"

#include <vector>

#include "base/check.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/common_source_info.h"

namespace content {

base::Time ComputeReportTime(const CommonSourceInfo& source,
                             base::Time trigger_time) {
  base::TimeDelta expiry_deadline =
      source.expiry_time() - source.impression_time();

  constexpr base::TimeDelta kMinExpiryDeadline = base::Days(2);
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
  constexpr base::TimeDelta kWindowDeadlineOffset = base::Hours(1);

  std::vector<base::TimeDelta> early_deadlines;
  switch (source.source_type()) {
    case CommonSourceInfo::SourceType::kNavigation:
      early_deadlines = {base::Days(2) - kWindowDeadlineOffset,
                         base::Days(7) - kWindowDeadlineOffset};
      break;
    case CommonSourceInfo::SourceType::kEvent:
      early_deadlines = {};
      break;
  }

  base::TimeDelta deadline_to_use = expiry_deadline;

  // Given a conversion that happened at `trigger_time`, find the first
  // applicable reporting window this conversion should be reported at.
  for (base::TimeDelta early_deadline : early_deadlines) {
    // If this window is valid for the conversion, use it.
    // |trigger_time| is roughly ~now.
    if (source.impression_time() + early_deadline >= trigger_time &&
        early_deadline < deadline_to_use) {
      deadline_to_use = early_deadline;
      break;
    }
  }

  // Valid conversion reports should always have a valid reporting deadline.
  DCHECK(!deadline_to_use.is_zero());

  return source.impression_time() + deadline_to_use + kWindowDeadlineOffset;
}

}  // namespace content
