// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/store_source_result.h"

#include "base/functional/overloaded.h"
#include "content/browser/attribution_reporting/store_source_result.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

namespace {
using Status = ::attribution_reporting::mojom::StoreSourceResult;
}  // namespace

Status StoreSourceResult::status() const {
  return absl::visit(
      base::Overloaded{
          [](Success) { return Status::kSuccess; },
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
          [](SuccessNoised) { return Status::kSuccessNoised; },
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
      },
      result_);
}

}  // namespace content
