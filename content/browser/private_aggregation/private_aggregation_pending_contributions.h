// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_PENDING_CONTRIBUTIONS_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_PENDING_CONTRIBUTIONS_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"

namespace content {

// Holds the pending histogram contributions for a particular aggregatable
// report through the Private Aggregation layer -- i.e. from the
// PrivateAggregationHost until just before the final budgeting round. This
// class also stores contributions that are conditional on error events,
// triggering or dropping those contributions based on whether the event
// occurred, as well as contribution merging and truncation.
//
// This class is only usable/constructible when the
// `kPrivateAggregationApiErrorReporting` feature is enabled. However, see
// `PrivateAggregationPendingContributions::Wrapper` for a class that holds
// either this type or a bare vector of contributions based on the state of that
// feature.
class CONTENT_EXPORT PrivateAggregationPendingContributions {
 public:
  // Contributions can be merged if they have matching keys.
  struct ContributionMergeKey {
    explicit ContributionMergeKey(
        const blink::mojom::AggregatableReportHistogramContributionPtr&
            contribution)
        : bucket(contribution->bucket),
          filtering_id(contribution->filtering_id.value_or(0)) {}

    auto operator<=>(const ContributionMergeKey& a) const = default;

    absl::uint128 bucket;
    uint64_t filtering_id;
  };

  class Wrapper;

  // TODO(crbug.com/381788013): Implement this class.
};

// This is a simple union class that holds contributions in the appropriate
// type given the state of the `kPrivateAggregationApiErrorReporting` feature.
//
// When the feature is disabled, this class is a wrapper around a vector of
// contributions (accessed via `GetContributionsVector()`), with contribution
// merging and truncation occurring before construction.
//
// When the feature is enabled, this class is a wrapper around
// `PrivateAggregationPendingContributions`, which also stores contributions
// that are conditional on error events, triggering or dropping those
// contributions based on whether the event occurred, as well as contribution
// merging and truncation.
//
// TODO(crbug.com/381788013): Remove this wrapper (replacing with a bare
// `PrivateAggregationPendingContributions`) after the feature is fully
// launched and the flag can be removed.
class CONTENT_EXPORT PrivateAggregationPendingContributions::Wrapper {
 public:
  // Usable iff `PrivateAggregationPendingContributions` is enabled.
  explicit Wrapper(
      PrivateAggregationPendingContributions pending_contributions);

  // Usable iff `PrivateAggregationPendingContributions` is disabled.
  explicit Wrapper(
      std::vector<blink::mojom::AggregatableReportHistogramContribution>
          contributions_vector);

  Wrapper(Wrapper&& other);
  Wrapper& operator=(Wrapper&& other);

  ~Wrapper();

  // Usable iff `PrivateAggregationPendingContributions` is enabled.
  PrivateAggregationPendingContributions& GetPendingContributions();

  // Usable iff `PrivateAggregationPendingContributions` is disabled.
  std::vector<blink::mojom::AggregatableReportHistogramContribution>&
  GetContributionsVector();

 private:
  absl::variant<
      PrivateAggregationPendingContributions,
      std::vector<blink::mojom::AggregatableReportHistogramContribution>>
      contributions_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_PENDING_CONTRIBUTIONS_H_
