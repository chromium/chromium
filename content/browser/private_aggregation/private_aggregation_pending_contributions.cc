// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_pending_contributions.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "content/browser/private_aggregation/private_aggregation_features.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"

namespace content {

PrivateAggregationPendingContributions::Wrapper::Wrapper(
    PrivateAggregationPendingContributions pending_contributions)
    : contributions_(std::move(pending_contributions)) {
  CHECK(base::FeatureList::IsEnabled(kPrivateAggregationApiErrorReporting));
}

PrivateAggregationPendingContributions::Wrapper::Wrapper(
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions_vector)
    : contributions_(std::move(contributions_vector)) {
  CHECK(!base::FeatureList::IsEnabled(kPrivateAggregationApiErrorReporting));
}

PrivateAggregationPendingContributions::Wrapper::Wrapper(
    PrivateAggregationPendingContributions::Wrapper&& other) = default;

PrivateAggregationPendingContributions::Wrapper&
PrivateAggregationPendingContributions::Wrapper::operator=(
    PrivateAggregationPendingContributions::Wrapper&& other) = default;

PrivateAggregationPendingContributions::Wrapper::~Wrapper() = default;

PrivateAggregationPendingContributions&
PrivateAggregationPendingContributions::Wrapper::GetPendingContributions() {
  CHECK(base::FeatureList::IsEnabled(kPrivateAggregationApiErrorReporting));
  return absl::get<0>(contributions_);
}

std::vector<blink::mojom::AggregatableReportHistogramContribution>&
PrivateAggregationPendingContributions::Wrapper::GetContributionsVector() {
  CHECK(!base::FeatureList::IsEnabled(kPrivateAggregationApiErrorReporting));
  return absl::get<1>(contributions_);
}

}  // namespace content
