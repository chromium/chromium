// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/for_debugging_only_report_util.h"

#include <optional>

#include "base/time/time.h"
#include "third_party/blink/public/common/features.h"

namespace content {

std::optional<base::Time> GetSampleDebugReportStartingFrom() {
  if (blink::features::kFledgeEnableFilteringDebugReportStartingFrom.Get() !=
      base::Milliseconds(0)) {
    // Also ceil kFledgeEnableFilteringDebugReportStartingFrom to its nearest
    // next hour, in the same way as lockout and cooldown start time are ceiled.
    // Otherwise, it's possible that the ceiled lockout/cooldowns collected
    // before this flag being greater than the flag, which caused them not being
    // ignored when they should be.
    return CeilToNearestNextHour(
        blink::features::kFledgeEnableFilteringDebugReportStartingFrom.Get());
  }
  return std::nullopt;
}

base::Time CeilToNearestNextHour(base::TimeDelta delta) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      delta.CeilToMultiple(base::Hours(1)));
}

bool IsInDebugReportLockout(
    const std::optional<base::Time>& last_report_sent_time,
    const base::Time now) {
  if (!last_report_sent_time.has_value()) {
    return false;
  }
  base::Time filtering_starting_from = base::Time::FromDeltaSinceWindowsEpoch(
      blink::features::kFledgeEnableFilteringDebugReportStartingFrom.Get()
          .CeilToMultiple(base::Hours(1)));

  bool is_lockout_before_filtering_starting =
      *last_report_sent_time < filtering_starting_from;
  bool is_in_lockout = *last_report_sent_time +
                           blink::features::kFledgeDebugReportLockout.Get() >=
                       now;
  return !is_lockout_before_filtering_starting && is_in_lockout;
}

}  // namespace content
