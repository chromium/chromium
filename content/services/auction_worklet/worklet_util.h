// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_UTIL_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_UTIL_H_

#include <stdint.h>

#include <vector>

#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"

namespace auction_worklet {

using RealTimeReportingContributions =
    std::vector<auction_worklet::mojom::RealTimeReportingContributionPtr>;

// Real time contributions to send on scoreAd error. May contain platform
// contribution (e.g., trusted signal fetch error contribution) that still may
// be reported even when scoreAd() has error.
CONTENT_EXPORT RealTimeReportingContributions
GetRealTimeReportingContributionsOnError(bool trusted_signals_fetch_failed,
                                         bool is_bidding_signal);

// Add platform contributions to `contributions` if there are any.
CONTENT_EXPORT void MaybeAddRealTimeReportingPlatformContributions(
    bool trusted_signals_fetch_failed,
    bool is_bidding_signal,
    RealTimeReportingContributions& contributions);

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_UTIL_H_
