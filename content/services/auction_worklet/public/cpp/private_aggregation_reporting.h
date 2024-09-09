// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_PRIVATE_AGGREGATION_REPORTING_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_PRIVATE_AGGREGATION_REPORTING_H_

#include <optional>
#include <string>

#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"

namespace auction_worklet {

std::optional<auction_worklet::mojom::ReservedEventType> ParseReservedEventType(
    const std::string& type,
    bool additional_extensions_allowed);

// Returns nullptr on unrecognized reserved name.
auction_worklet::mojom::EventTypePtr ParsePrivateAggregationEventType(
    const std::string& event_type_str,
    bool additional_extensions_allowed);

// Returns true if `value` requires the feature
// kPrivateAggregationApiProtectedAudienceAdditionalExtensions to be used.
CONTENT_EXPORT inline bool RequiresAdditionalExtensions(
    mojom::BaseValue value) {
  return value > mojom::BaseValue::kBidRejectReason;
}

// Returns whether the request is valid or not, checking whether it uses
// features enabled based on `additional_extensions_allowed`.
CONTENT_EXPORT bool IsValidPrivateAggregationRequestForAdditionalExtensions(
    const auction_worklet::mojom::PrivateAggregationRequest& request,
    bool additional_extensions_allowed);

// Returns true if `request` is asking to record reject-reason, and therefore
// can be used to report kBelowKAnonThreshold for the bid that would have
// won if not for k-anonymity.
CONTENT_EXPORT bool HasKAnonFailureComponent(
    const mojom::PrivateAggregationRequest& request);

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_PRIVATE_AGGREGATION_REPORTING_H_
