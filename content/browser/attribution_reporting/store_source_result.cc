// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/store_source_result.h"

#include <optional>
#include <utility>

#include "base/functional/overloaded.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/store_source_result.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

namespace {
using Status = ::attribution_reporting::mojom::StoreSourceResult;
}  // namespace

StoreSourceResult::StoreSourceResult(StorableSource source,
                                     bool is_noised,
                                     base::Time source_time,
                                     std::optional<int> destination_limit,
                                     Result result)
    : source_(std::move(source)),
      is_noised_(is_noised),
      source_time_(source_time),
      destination_limit_(destination_limit),
      result_(std::move(result)) {
  if (const auto* success = absl::get_if<Success>(&result_)) {
    CHECK(!success->min_fake_report_time.has_value() || is_noised_);
  }
}

StoreSourceResult::~StoreSourceResult() = default;

StoreSourceResult::StoreSourceResult(const StoreSourceResult&) = default;

StoreSourceResult& StoreSourceResult::operator=(const StoreSourceResult&) =
    default;

StoreSourceResult::StoreSourceResult(StoreSourceResult&&) = default;

StoreSourceResult& StoreSourceResult::operator=(StoreSourceResult&&) = default;

Status StoreSourceResult::status() const {
  return absl::visit(
      base::Overloaded{
          [&](Success) {
            return is_noised_ ? Status::kSuccessNoised : Status::kSuccess;
          },
          [](InternalError) { return Status::kInternalError; },
          [](InsufficientSourceCapacity) {
            return Status::kInsufficientSourceCapacity;
          },
          [](InsufficientUniqueDestinationCapacity) {
            return Status::kInsufficientUniqueDestinationCapacity;
          },
          [](ExcessiveReportingOrigins) {
            return Status::kExcessiveReportingOrigins;
          },
          [](ProhibitedByBrowserPolicy) {
            return Status::kProhibitedByBrowserPolicy;
          },
          [](DestinationReportingLimitReached) {
            return Status::kDestinationReportingLimitReached;
          },
          [](DestinationGlobalLimitReached) {
            return Status::kDestinationGlobalLimitReached;
          },
          [](DestinationBothLimitsReached) {
            return Status::kDestinationBothLimitsReached;
          },
          [](ReportingOriginsPerSiteLimitReached) {
            return Status::kReportingOriginsPerSiteLimitReached;
          },
          [](ExceedsMaxChannelCapacity) {
            return Status::kExceedsMaxChannelCapacity;
          },
          [](ExceedsMaxScopesChannelCapacity) {
            return Status::kExceedsMaxScopesChannelCapacity;
          },
          [](ExceedsMaxTriggerStateCardinality) {
            return Status::kExceedsMaxTriggerStateCardinality;
          },
          [](ExceedsMaxEventStatesLimit) {
            return Status::kExceedsMaxEventStatesLimit;
          },
          [](DestinationPerDayReportingLimitReached) {
            return Status::kDestinationPerDayReportingLimitReached;
          },
      },
      result_);
}

}  // namespace content
