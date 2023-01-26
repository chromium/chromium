// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PA_REPORT_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PA_REPORT_UTIL_H_

#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

// Reserved event types for aggregatable report's for-event contribution.
CONTENT_EXPORT extern const char kReservedAlways[];
CONTENT_EXPORT extern const char kReservedWin[];
CONTENT_EXPORT extern const char kReservedLoss[];

// If request's contribution is an AggregatableReportForEventContribution, fills
// the contribution in with post-auction signals such as winning_bid, converts
// it to an AggregatableReportHistogramContribution and returns the resulting
// PrivateAggregationRequest. Returns nullptr in case of an error, or the
// request should not be sent (e.g., it's for a losing bidder but request's
// event type is "reserved.win"). Simply returns request if it's contribution is
// an AggregatableReportHistogramContribution.
CONTENT_EXPORT auction_worklet::mojom::PrivateAggregationRequestPtr
FillInPrivateAggregationRequest(
    auction_worklet::mojom::PrivateAggregationRequestPtr request,
    double winning_bid,
    double highest_scoring_other_bid,
    const absl::optional<auction_worklet::mojom::RejectReason> reject_reason,
    bool is_winner);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PA_REPORT_UTIL_H_
