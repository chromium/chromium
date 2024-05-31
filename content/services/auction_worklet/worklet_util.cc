// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/worklet_util.h"

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/feature_list.h"
#include "content/services/auction_worklet/public/mojom/real_time_reporting.mojom.h"
#include "third_party/blink/public/common/features.h"

namespace auction_worklet {

RealTimeReportingContributions GetRealTimeReportingContributionsOnError(
    bool trusted_signals_failed,
    bool is_bidding_signal) {
  RealTimeReportingContributions real_time_contributions_on_error;
  MaybeAddRealTimeReportingPlatformContributions(
      trusted_signals_failed, is_bidding_signal,
      real_time_contributions_on_error);
  return real_time_contributions_on_error;
}

CONTENT_EXPORT void MaybeAddRealTimeReportingPlatformContributions(
    bool trusted_signals_failed,
    bool is_bidding_signal,
    RealTimeReportingContributions& contributions) {
  if (base::FeatureList::IsEnabled(blink::features::kFledgeRealTimeReporting) &&
      trusted_signals_failed) {
    contributions.push_back(
        auction_worklet::mojom::RealTimeReportingContribution::New(
            /*bucket=*/is_bidding_signal
                ? kTrustedBiddingSignalsFailureRealTimeBucket
                : kTrustedScoringSignalsFailureRealTimeBucket,
            /*priority_weight=*/kRealTimePlatformContributionPriorityWeight,
            /*latency_threshold=*/std::nullopt));
  }
}

}  // namespace auction_worklet
