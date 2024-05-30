// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/storage_interest_group.h"

#include <optional>

#include "base/base64.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"

namespace content {

StorageInterestGroup::StorageInterestGroup() = default;
StorageInterestGroup::StorageInterestGroup(StorageInterestGroup&&) = default;
StorageInterestGroup::~StorageInterestGroup() = default;

DebugReportLockoutAndCooldowns::DebugReportLockoutAndCooldowns() = default;
DebugReportLockoutAndCooldowns::DebugReportLockoutAndCooldowns(
    std::optional<base::Time> last_report_sent_time,
    std::map<url::Origin, DebugReportCooldown> debug_report_cooldown_map)
    : last_report_sent_time(last_report_sent_time),
      debug_report_cooldown_map(std::move(debug_report_cooldown_map)) {}
DebugReportLockoutAndCooldowns::DebugReportLockoutAndCooldowns(
    DebugReportLockoutAndCooldowns&) = default;
DebugReportLockoutAndCooldowns::DebugReportLockoutAndCooldowns(
    DebugReportLockoutAndCooldowns&&) = default;
DebugReportLockoutAndCooldowns::~DebugReportLockoutAndCooldowns() = default;

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

}  // namespace content
