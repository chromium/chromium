// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PA_REPORT_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PA_REPORT_UTIL_H_

#include <vector>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/private_aggregation/aggregatable_report.mojom-forward.h"

namespace url {
class Origin;
}

namespace content {

class PrivateAggregationManager;

// Reserved event types for aggregatable report's for-event contribution.
CONTENT_EXPORT extern const char kReservedAlways[];
CONTENT_EXPORT extern const char kReservedWin[];
CONTENT_EXPORT extern const char kReservedLoss[];

struct CONTENT_EXPORT PrivateAggregationRequestWithEventType {
  PrivateAggregationRequestWithEventType(
      auction_worklet::mojom::PrivateAggregationRequestPtr request,
      absl::optional<std::string> event_type);

  PrivateAggregationRequestWithEventType(
      PrivateAggregationRequestWithEventType&&);

  bool operator==(const PrivateAggregationRequestWithEventType& rhs) const;

  ~PrivateAggregationRequestWithEventType();

  auction_worklet::mojom::PrivateAggregationRequestPtr request;

  // Event type of the private aggregation request. Set to absl::nullopt if it's
  // a reserved event type.
  absl::optional<std::string> event_type;
};

// Various timings that can be used as base values in private aggregation.
struct CONTENT_EXPORT PrivateAggregationTimings {
  base::TimeDelta script_run_time;
  base::TimeDelta signals_fetch_time;
};

// If request's contribution is an AggregatableReportForEventContribution, fills
// the contribution in with post-auction signals such as winning_bid, converts
// it to an AggregatableReportHistogramContribution and returns the resulting
// PrivateAggregationRequest. Returns absl::nullopt in case of an error, or the
// request should not be sent (e.g., it's for a losing bidder but request's
// event type is "reserved.win"). Simply returns request with event type
// "reserved." if it's contribution is an
// AggregatableReportHistogramContribution.
CONTENT_EXPORT absl::optional<PrivateAggregationRequestWithEventType>
FillInPrivateAggregationRequest(
    auction_worklet::mojom::PrivateAggregationRequestPtr request,
    double winning_bid,
    double highest_scoring_other_bid,
    const absl::optional<auction_worklet::mojom::RejectReason> reject_reason,
    const PrivateAggregationTimings& timings,
    bool is_winner);

// Splits a vector of requests into those with matching debug mode details and
// then forwards to a new mojo pipe.
CONTENT_EXPORT void SplitContributionsIntoBatchesThenSendToHost(
    std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr> requests,
    PrivateAggregationManager& pa_manager,
    const url::Origin& reporting_origin,
    const url::Origin& main_frame_origin);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PA_REPORT_UTIL_H_
