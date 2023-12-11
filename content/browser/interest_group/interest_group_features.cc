// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

// Please keep features in alphabetical order.

// Enable parsing ad auction response headers for an iframe navigation request.
BASE_FEATURE(kEnableIFrameAdAuctionHeaders,
             "EnableIFrameAdAuctionHeaders",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enable updating the executionMode to "frozen-context" when updating a user's
// interests groups.
BASE_FEATURE(kEnableUpdatingExecutionModeToFrozenContext,
             "EnableUpdatingExecutionModeToFrozenContext",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable sampling forDebuggingOnly reports.
BASE_FEATURE(kFledgeSampleDebugReports,
             "FledgeSampleDebugReports",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Prevent ad techs who accidentally call the API repeatedly for all users,
// from locking themselves out of sending any more debug reports for years.
// This is accomplished by most of the time putting that ad tech in a shorter
// cooldown period, and only some time (e.g., 10% of the time) putting it in a
// restricted cooldown period.
const base::FeatureParam<base::TimeDelta> kFledgeDebugReportShortCooldown{
    &kFledgeSampleDebugReports, "fledge_debug_report_short_cooldown",
    base::Days(14)};
const base::FeatureParam<base::TimeDelta> kFledgeDebugReportRestrictedCooldown{
    &kFledgeSampleDebugReports, "fledge_debug_report_restricted_cooldown",
    base::Days(365)};

// Enable updating userBiddingSignals when updating a user's interests groups.
BASE_FEATURE(kEnableUpdatingUserBiddingSignals,
             "EnableUpdatingUserBiddingSignals",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
