// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_STORAGE_INTEREST_GROUP_H_
#define CONTENT_BROWSER_INTEREST_GROUP_STORAGE_INTEREST_GROUP_H_

#include <map>
#include <optional>
#include <vector>

#include "base/time/time.h"
#include "content/browser/interest_group/for_debugging_only_report_util.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/origin.h"

namespace content {

// StorageInterestGroup contains both the auction worklet's Bidding interest
// group as well as several fields that are used by the browser process during
// an auction but are not needed by or should not be sent to the worklet
// process.
struct CONTENT_EXPORT StorageInterestGroup {
  StorageInterestGroup();
  StorageInterestGroup(StorageInterestGroup&&);
  StorageInterestGroup& operator=(StorageInterestGroup&&) = default;
  ~StorageInterestGroup();

  blink::InterestGroup interest_group;
  blink::mojom::BiddingBrowserSignalsPtr bidding_browser_signals;

  // Hashed k-anonymous keys.
  base::flat_set<std::string> hashed_kanon_keys;

  // Top level page origin from when the interest group was joined.
  url::Origin joining_origin;
  // Most recent time the interest group was joined. Stored in database as
  // `exact_join_time`.
  base::Time join_time;
  // The last time this interest group was updated.
  base::Time last_updated;
  // The time when the browser will permit updating this interest group.
  base::Time next_update_after;
  // The last time the k-anon values were updated.
  base::Time last_k_anon_updated;
};

struct CONTENT_EXPORT DebugReportLockoutAndCooldowns {
  DebugReportLockoutAndCooldowns();
  DebugReportLockoutAndCooldowns(
      std::optional<DebugReportLockout> lockout,
      std::map<url::Origin, DebugReportCooldown> debug_report_cooldown_map);
  DebugReportLockoutAndCooldowns(DebugReportLockoutAndCooldowns&);
  DebugReportLockoutAndCooldowns& operator=(DebugReportLockoutAndCooldowns&&) =
      default;
  DebugReportLockoutAndCooldowns(DebugReportLockoutAndCooldowns&&);
  ~DebugReportLockoutAndCooldowns();

  // The lockout of sending forDebuggingOnly reports.
  std::optional<DebugReportLockout> lockout;
  // The key is an ad tech origin, and value is its cooldown of sending
  // forDebuggingOnly reports.
  std::map<url::Origin, DebugReportCooldown> debug_report_cooldown_map;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_STORAGE_INTEREST_GROUP_H_
