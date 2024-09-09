// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/public/cpp/private_aggregation_reporting.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"

namespace auction_worklet {

namespace {

constexpr auto kReservedEventTypes =
    base::MakeFixedFlatMap<std::string_view,
                           auction_worklet::mojom::ReservedEventType>(
        {{"reserved.always",
          auction_worklet::mojom::ReservedEventType::kReservedAlways},
         {"reserved.win",
          auction_worklet::mojom::ReservedEventType::kReservedWin},
         {"reserved.loss",
          auction_worklet::mojom::ReservedEventType::kReservedLoss},
         {"reserved.once",
          auction_worklet::mojom::ReservedEventType::kReservedOnce}});

bool RequiresAdditionalExtensionsForReservedEventType(
    auction_worklet::mojom::ReservedEventType type) {
  return type == auction_worklet::mojom::ReservedEventType::kReservedOnce;
}

}  // namespace

std::optional<auction_worklet::mojom::ReservedEventType> ParseReservedEventType(
    const std::string& name,
    bool additional_extensions_allowed) {
  auto it = kReservedEventTypes.find(name);
  if (it == kReservedEventTypes.end()) {
    return std::nullopt;
  }
  auction_worklet::mojom::ReservedEventType keyword = it->second;
  if (!additional_extensions_allowed &&
      RequiresAdditionalExtensionsForReservedEventType(keyword)) {
    return std::nullopt;
  }
  return keyword;
}

auction_worklet::mojom::EventTypePtr ParsePrivateAggregationEventType(
    const std::string& event_type_str,
    bool additional_extensions_allowed) {
  if (base::StartsWith(event_type_str, "reserved.")) {
    std::optional<auction_worklet::mojom::ReservedEventType> maybe_reserved =
        ParseReservedEventType(event_type_str, additional_extensions_allowed);
    // Don't throw an error if an invalid reserved event type is provided, to
    // provide forward compatibility with new reserved event types added
    // later.
    if (maybe_reserved.has_value()) {
      return auction_worklet::mojom::EventType::NewReserved(
          maybe_reserved.value());
    }
  } else {
    return auction_worklet::mojom::EventType::NewNonReserved(event_type_str);
  }
  return auction_worklet::mojom::EventTypePtr();
}

bool IsValidPrivateAggregationRequestForAdditionalExtensions(
    const mojom::PrivateAggregationRequest& request,
    bool additional_extensions_allowed) {
  if (additional_extensions_allowed) {
    return true;
  }

  if (request.contribution->is_histogram_contribution()) {
    return true;
  }

  const mojom::AggregatableReportForEventContribution& for_event_contrib =
      *request.contribution->get_for_event_contribution();

  if (for_event_contrib.event_type->is_reserved() &&
      RequiresAdditionalExtensionsForReservedEventType(
          for_event_contrib.event_type->get_reserved())) {
    return false;
  }

  if (for_event_contrib.bucket->is_signal_bucket() &&
      RequiresAdditionalExtensions(
          for_event_contrib.bucket->get_signal_bucket()->base_value)) {
    return false;
  }
  if (for_event_contrib.value->is_signal_value() &&
      RequiresAdditionalExtensions(
          for_event_contrib.value->get_signal_value()->base_value)) {
    return false;
  }

  return true;
}

bool HasKAnonFailureComponent(const mojom::PrivateAggregationRequest& request) {
  if (request.contribution->is_histogram_contribution()) {
    return false;
  }
  const mojom::AggregatableReportForEventContributionPtr& event_contribution =
      request.contribution->get_for_event_contribution();
  if (event_contribution->bucket->is_signal_bucket() &&
      event_contribution->bucket->get_signal_bucket()->base_value ==
          mojom::BaseValue::kBidRejectReason) {
    return true;
  }
  if (event_contribution->value->is_signal_value() &&
      event_contribution->value->get_signal_value()->base_value ==
          mojom::BaseValue::kBidRejectReason) {
    return true;
  }
  return false;
}

}  // namespace auction_worklet
