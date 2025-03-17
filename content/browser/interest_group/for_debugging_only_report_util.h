// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_FOR_DEBUGGING_ONLY_REPORT_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_FOR_DEBUGGING_ONLY_REPORT_UTIL_H_

#include <optional>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT DebugReportLockout {
  base::Time starting_time;
  base::TimeDelta duration;

  bool operator==(const DebugReportLockout& other) const = default;
};

// Should forDebuggingOnly reports be sampled or not.
CONTENT_EXPORT bool ShouldSampleDebugReport();

// Ceil kFledgeEnableFilteringDebugReportStartingFrom to its nearest
// next hour, in the same way as lockout and cooldown start time are ceiled.
CONTENT_EXPORT std::optional<base::Time> GetSampleDebugReportStartingFrom();

// Ceil `detla` to its nearest next hour.
CONTENT_EXPORT base::Time CeilToNearestNextHour(base::TimeDelta delta);

// Returns true if the client is under forDebuggingOnly API's lockout period.
CONTENT_EXPORT bool IsInDebugReportLockout(
    const std::optional<DebugReportLockout>& lockout,
    const base::Time now);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_FOR_DEBUGGING_ONLY_REPORT_UTIL_H_
