// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/public/cpp/private_aggregation_reporting.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_util.h"
#include "content/services/auction_worklet/public/mojom/private_aggregation_request.mojom.h"

namespace auction_worklet {

namespace {

using ReservedErrorEventType = auction_worklet::mojom::ReservedErrorEventType;
using ReservedNonErrorEventType =
    auction_worklet::mojom::ReservedNonErrorEventType;

constexpr auto kReservedErrorEventTypes =
    base::MakeFixedFlatMap<std::string_view, ReservedErrorEventType>(
        {{"reserved.report-success", ReservedErrorEventType::kReportSuccess},
         {"reserved.too-many-contributions",
          ReservedErrorEventType::kTooManyContributions},
         {"reserved.empty-report-dropped",
          ReservedErrorEventType::kEmptyReportDropped},
         {"reserved.pending-report-limit-reached",
          ReservedErrorEventType::kPendingReportLimitReached},
         {"reserved.insufficient-budget",
          ReservedErrorEventType::kInsufficientBudget},
         {"reserved.uncaught-error", ReservedErrorEventType::kUncaughtError}});

constexpr auto kReservedNonErrorEventTypes =
    base::MakeFixedFlatMap<std::string_view, ReservedNonErrorEventType>(
        {{"reserved.always", ReservedNonErrorEventType::kReservedAlways},
         {"reserved.win", ReservedNonErrorEventType::kReservedWin},
         {"reserved.loss", ReservedNonErrorEventType::kReservedLoss},
         {"reserved.once", ReservedNonErrorEventType::kReservedOnce}});

bool RequiresAdditionalExtensionsForReservedNonErrorEventType(
    ReservedNonErrorEventType type) {
  return type == ReservedNonErrorEventType::kReservedOnce;
}

std::optional<ReservedErrorEventType> ParseReservedErrorEventType(
    const std::string& name,
    bool error_reporting_allowed) {
  if (!error_reporting_allowed) {
    return std::nullopt;
  }

  auto it = kReservedErrorEventTypes.find(name);
  if (it == kReservedErrorEventTypes.end()) {
    return std::nullopt;
  }
  ReservedErrorEventType keyword = it->second;

  return keyword;
}

std::optional<ReservedNonErrorEventType> ParseReservedNonErrorEventType(
    const std::string& name,
    bool additional_extensions_allowed) {
  auto it = kReservedNonErrorEventTypes.find(name);
  if (it == kReservedNonErrorEventTypes.end()) {
    return std::nullopt;
  }
  ReservedNonErrorEventType keyword = it->second;
  if (!additional_extensions_allowed &&
      RequiresAdditionalExtensionsForReservedNonErrorEventType(keyword)) {
    return std::nullopt;
  }
  return keyword;
}

}  // namespace

auction_worklet::mojom::EventTypePtr ParsePrivateAggregationEventType(
    const std::string& event_type_str,
    bool additional_extensions_allowed,
    bool error_reporting_allowed) {
  if (!base::StartsWith(event_type_str, "reserved.")) {
    return auction_worklet::mojom::EventType::NewNonReserved(event_type_str);
  }

  std::optional<ReservedNonErrorEventType> maybe_reserved_non_error =
      ParseReservedNonErrorEventType(event_type_str,
                                     additional_extensions_allowed);
  // Don't throw an error if an invalid reserved event type is provided, to
  // provide forward compatibility with new reserved event types added
  // later.
  if (maybe_reserved_non_error.has_value()) {
    return auction_worklet::mojom::EventType::NewReservedNonError(
        maybe_reserved_non_error.value());
  }

  std::optional<ReservedErrorEventType> maybe_reserved_error =
      ParseReservedErrorEventType(event_type_str, error_reporting_allowed);
  if (maybe_reserved_error.has_value()) {
    return auction_worklet::mojom::EventType::NewReservedError(
        maybe_reserved_error.value());
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

  if (for_event_contrib.event_type->is_reserved_non_error() &&
      RequiresAdditionalExtensionsForReservedNonErrorEventType(
          for_event_contrib.event_type->get_reserved_non_error())) {
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
