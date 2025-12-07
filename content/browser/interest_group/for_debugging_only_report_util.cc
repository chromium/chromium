// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/for_debugging_only_report_util.h"

#include <map>
#include <optional>

#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace content {

bool ShouldSampleDebugReport() {
  // TODO(crbug.com/391877228): Set its value based on cookie settings.
  bool for_debugging_only_sampling = false;

  // Overwrite `for_debugging_only_sampling` to true, for testing purpose.
  if (base::FeatureList::IsEnabled(
          features::kFledgeDoSampleDebugReportForTesting)) {
    for_debugging_only_sampling = true;
  }
  return for_debugging_only_sampling;
}

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

std::optional<base::TimeDelta> ConvertDebugReportCooldownTypeToDuration(
    DebugReportCooldownType type) {
  switch (type) {
    case DebugReportCooldownType::kShortCooldown:
      return blink::features::kFledgeDebugReportShortCooldown.Get();
    case DebugReportCooldownType::kRestrictedCooldown:
      return blink::features::kFledgeDebugReportRestrictedCooldown.Get();
  }
  return std::nullopt;
}

bool IsInDebugReportLockout(const std::optional<DebugReportLockout>& lockout,
                            const base::Time now) {
  if (!lockout.has_value()) {
    return false;
  }
  base::Time filtering_starting_from = base::Time::FromDeltaSinceWindowsEpoch(
      blink::features::kFledgeEnableFilteringDebugReportStartingFrom.Get()
          .CeilToMultiple(base::Hours(1)));

  bool is_lockout_before_filtering_starting =
      lockout->starting_time < filtering_starting_from;
  bool is_in_lockout = lockout->starting_time + lockout->duration >= now;
  return !is_lockout_before_filtering_starting && is_in_lockout;
}

bool IsInDebugReportCooldown(
    const url::Origin& origin,
    const std::map<url::Origin, DebugReportCooldown>& cooldowns_map,
    const base::Time now) {
  const auto cooldown_it = cooldowns_map.find(origin);
  if (cooldown_it != cooldowns_map.end()) {
    std::optional<base::TimeDelta> duration =
        ConvertDebugReportCooldownTypeToDuration(cooldown_it->second.type);
    if (duration.has_value()) {
      bool is_cooldown_before_filtering_starting =
          cooldown_it->second.starting_time <
          CeilToNearestNextHour(
              blink::features::kFledgeEnableFilteringDebugReportStartingFrom
                  .Get());
      bool is_in_cooldown =
          cooldown_it->second.starting_time + *duration >= now;
      if (!is_cooldown_before_filtering_starting && is_in_cooldown) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace content
