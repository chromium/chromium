// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_FOR_DEBUGGING_ONLY_REPORT_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_FOR_DEBUGGING_ONLY_REPORT_UTIL_H_

#include <map>
#include <optional>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

struct CONTENT_EXPORT DebugReportLockout {
  base::Time starting_time;
  base::TimeDelta duration;

  bool operator==(const DebugReportLockout& other) const = default;
};

enum class DebugReportCooldownType {
  kShortCooldown = 0,
  kRestrictedCooldown = 1,

  kMaxValue = kRestrictedCooldown,
};

struct CONTENT_EXPORT DebugReportCooldown {
  base::Time starting_time;
  DebugReportCooldownType type;

  bool operator==(const DebugReportCooldown& other) const = default;
};

// True if only send sampled debug reports, false if send all debug reports.
CONTENT_EXPORT bool ShouldSampleDebugReport();

// Ceil kFledgeEnableFilteringDebugReportStartingFrom to its nearest
// next hour, in the same way as lockout and cooldown start time are ceiled.
CONTENT_EXPORT std::optional<base::Time> GetSampleDebugReportStartingFrom();

// Ceil `detla` to its nearest next hour.
CONTENT_EXPORT base::Time CeilToNearestNextHour(base::TimeDelta delta);

// Converts forDebuggingOnly API's cooldown type to its actual cooldown
// duration.
CONTENT_EXPORT std::optional<base::TimeDelta>
ConvertDebugReportCooldownTypeToDuration(DebugReportCooldownType type);

// Returns true if the client is under forDebuggingOnly API's lockout period.
CONTENT_EXPORT bool IsInDebugReportLockout(
    const std::optional<DebugReportLockout>& lockout,
    const base::Time now);

// Returns true if the `origin` is under forDebuggingOnly API's cooldown period.
CONTENT_EXPORT bool IsInDebugReportCooldown(
    const url::Origin& origin,
    const std::map<url::Origin, DebugReportCooldown>& cooldowns_map,
    const base::Time now);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_FOR_DEBUGGING_ONLY_REPORT_UTIL_H_
