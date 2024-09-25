// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PA_REPORT_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PA_REPORT_UTIL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom-forward.h"

namespace url {
class Origin;
}

namespace content {

class PrivateAggregationManager;

struct CONTENT_EXPORT PrivateAggregationRequestWithEventType {
  PrivateAggregationRequestWithEventType(
      auction_worklet::mojom::PrivateAggregationRequestPtr request,
      std::optional<std::string> event_type);

  PrivateAggregationRequestWithEventType(
      PrivateAggregationRequestWithEventType&&);

  bool operator==(const PrivateAggregationRequestWithEventType& rhs) const;

  ~PrivateAggregationRequestWithEventType();

  auction_worklet::mojom::PrivateAggregationRequestPtr request;

  // Event type of the private aggregation request. Set to std::nullopt if it's
  // a reserved event type.
  std::optional<std::string> event_type;
};

// Various timings that can be used as base values in private aggregation.
struct CONTENT_EXPORT PrivateAggregationTimings {
  base::TimeDelta script_run_time;
  base::TimeDelta signals_fetch_time;
};

struct CONTENT_EXPORT PrivateAggregationParticipantData {
  PrivateAggregationParticipantData();
  PrivateAggregationParticipantData(const PrivateAggregationParticipantData&);
  PrivateAggregationParticipantData& operator=(
      const PrivateAggregationParticipantData&);
  PrivateAggregationParticipantData(PrivateAggregationParticipantData&&);
  PrivateAggregationParticipantData& operator=(
      PrivateAggregationParticipantData&&);

  // These metrics are set on bidders only; on sellers they are always 0.

  // Number of interest groups that got selected to make bids (after filtering,
  // capabilities checks, discarding those w/o ads, etc).
  int participating_interest_group_count = 0;
  double percent_igs_cumulative_timeout = 0;
  base::TimeDelta cumulative_buyer_time;
  int regular_igs = 0;
  double percent_regular_igs_quota_used = 0;
  int negative_igs = 0;
  double percent_negative_igs_quota_used = 0;
  size_t igs_storage_used = 0;
  double percent_igs_storage_quota_used = 0;

  // These metrics are set for both bidders and sellers.
  base::TimeDelta average_code_fetch_time;
  double percent_scripts_timeout = 0;
};

// Key used to group Private aggregation signals.
struct CONTENT_EXPORT PrivateAggregationKey {
  PrivateAggregationKey(
      url::Origin reporting_origin,
      std::optional<url::Origin> aggregation_coordinator_origin);
  PrivateAggregationKey(const PrivateAggregationKey&);
  PrivateAggregationKey& operator=(const PrivateAggregationKey&);
  PrivateAggregationKey(PrivateAggregationKey&&);
  PrivateAggregationKey& operator=(PrivateAggregationKey&&);
  ~PrivateAggregationKey();

  bool operator<(const PrivateAggregationKey& other) const {
    return std::tie(reporting_origin, aggregation_coordinator_origin) <
           std::tie(other.reporting_origin,
                    other.aggregation_coordinator_origin);
  }

  url::Origin reporting_origin;
  std::optional<url::Origin> aggregation_coordinator_origin;
};

// Helps determine which level of worklet a particular PA request came from.
enum class PrivateAggregationPhase {
  kBidder,
  kNonTopLevelSeller,  // Seller for a component auction.
  kTopLevelSeller,     // Top-level seller, either with components or
                       // as the sole seller in a single-seller auction.
  kNumPhases
};

// Used as a key to group Private Aggregation API requests from worklets in
// a map. The `reporting_origin` and `aggregation_coordinator_origin` are
// passed into the Private Aggregation API.
struct CONTENT_EXPORT PrivateAggregationPhaseKey {
  PrivateAggregationPhaseKey(
      url::Origin reporting_origin,
      PrivateAggregationPhase phase,
      std::optional<url::Origin> aggregation_coordinator_origin);
  PrivateAggregationPhaseKey(const PrivateAggregationPhaseKey& other);
  PrivateAggregationPhaseKey& operator=(
      const PrivateAggregationPhaseKey& other);
  PrivateAggregationPhaseKey(PrivateAggregationPhaseKey&& other);
  PrivateAggregationPhaseKey& operator=(PrivateAggregationPhaseKey&& other);
  ~PrivateAggregationPhaseKey();

  bool operator<(const PrivateAggregationPhaseKey& other) const {
    return std::tie(reporting_origin, phase, aggregation_coordinator_origin) <
           std::tie(other.reporting_origin, other.phase,
                    other.aggregation_coordinator_origin);
  }

  url::Origin reporting_origin;
  PrivateAggregationPhase phase;
  std::optional<url::Origin> aggregation_coordinator_origin;
};

// If request's contribution is an AggregatableReportForEventContribution, fills
// the contribution in with post-auction signals such as winning_bid, converts
// it to an AggregatableReportHistogramContribution and returns the resulting
// PrivateAggregationRequest. Returns std::nullopt in case of an error, or the
// request should not be sent (e.g., it's for a losing bidder but request's
// event type is "reserved.win"). Simply returns request with event type
// "reserved." if it's contribution is an
// AggregatableReportHistogramContribution.
CONTENT_EXPORT std::optional<PrivateAggregationRequestWithEventType>
FillInPrivateAggregationRequest(
    auction_worklet::mojom::PrivateAggregationRequestPtr request,
    double winning_bid,
    double highest_scoring_other_bid,
    const std::optional<auction_worklet::mojom::RejectReason> reject_reason,
    const PrivateAggregationParticipantData& participant_data,
    const PrivateAggregationTimings& timings,
    bool is_winner);

// Returns true if `request` is a for-event contribution with "reserved.once"
// event type.
CONTENT_EXPORT bool IsPrivateAggregationRequestReservedOnce(
    const auction_worklet::mojom::PrivateAggregationRequest& request);

// Splits a vector of requests into those with matching debug mode details and
// then forwards to a new mojo pipe.
CONTENT_EXPORT void SplitContributionsIntoBatchesThenSendToHost(
    std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr> requests,
    PrivateAggregationManager& pa_manager,
    const url::Origin& reporting_origin,
    std::optional<url::Origin> aggregation_coordinator_origin,
    const url::Origin& main_frame_origin);

// Returns true if request has a valid filtering ID.
CONTENT_EXPORT bool HasValidFilteringId(
    const auction_worklet::mojom::PrivateAggregationRequestPtr& request);

// Returns true if filtering ID is valid.
CONTENT_EXPORT bool IsValidFilteringId(std::optional<uint64_t> filtering_id);

// If `pa_requests` contains malformed requests, returns an error message.
// Otherwise returns nullopt.
CONTENT_EXPORT std::optional<std::string> ValidatePrivateAggregationRequests(
    const std::vector<auction_worklet::mojom::PrivateAggregationRequestPtr>&
        pa_requests);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PA_REPORT_UTIL_H_
