// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_FOR_DEBUGGING_ONLY_REPORT_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_FOR_DEBUGGING_ONLY_REPORT_UTIL_H_

#include <optional>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// Ceil kFledgeEnableFilteringDebugReportStartingFrom to its nearest
// next hour, in the same way as lockout and cooldown start time are ceiled.
CONTENT_EXPORT std::optional<base::Time> GetSampleDebugReportStartingFrom();

// Returns true if the client is under forDebuggingOnly API's lockout period.
CONTENT_EXPORT bool IsInDebugReportLockout(
    const std::optional<base::Time>& last_report_sent_time,
    const base::Time now);

// Ceil `detla` to its nearest next hour.
CONTENT_EXPORT base::Time CeilToNearestNextHour(base::TimeDelta delta);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_FOR_DEBUGGING_ONLY_REPORT_UTIL_H_
