// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_PENDING_CONTRIBUTIONS_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_PENDING_CONTRIBUTIONS_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <variant>
#include <vector>

#include "base/numerics/safe_conversions.h"
#include "content/browser/private_aggregation/private_aggregation_budgeter.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"

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
        const blink::mojom::AggregatableReportHistogramContribution&
            contribution)
        : bucket(contribution.bucket),
          filtering_id(contribution.filtering_id.value_or(0)) {}

    explicit ContributionMergeKey(
        const blink::mojom::AggregatableReportHistogramContributionPtr&
            contribution)
        : ContributionMergeKey(*contribution) {}

    auto operator<=>(const ContributionMergeKey& a) const = default;

    absl::uint128 bucket;
    uint64_t filtering_id;
  };

  class Wrapper;

  using BudgeterResult = PrivateAggregationBudgeter::ResultForContribution;
  using PendingReportLimitResult =
      PrivateAggregationBudgeter::PendingReportLimitResult;

  // Indicates the reason for the PrivateAggregationHost mojo pipe closing.
  // TODO(crbug.com/381788013): Consider moving this enum to
  // `PrivateAggregationHost` once the feature is fully launched and the
  // circular dependency is avoided.
  enum TimeoutOrDisconnect {
    // The timeout was reached.
    kTimeout,

    // The pipe was disconnected by the caller API (e.g. due to the script
    // completing).
    kDisconnect
  };

  // Mirrors `PrivateAggregationHost::NullReportBehavior` to avoid a circular
  // dependency.
  // TODO(crbug.com/381788013): Merge these enums once the feature is fully
  // launched and the circular dependency is avoided.
  enum class NullReportBehavior {
    kSendNullReport,
    kDontSendReport,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(TruncationResult)
  enum class TruncationResult {
    kNoTruncation = 0,
    kTruncationDueToUnconditionalContributions = 1,
    kTruncationNotDueToUnconditionalContributions = 2,
    kMaxValue = kTruncationNotDueToUnconditionalContributions,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/privacy/enums.xml:PrivacySandboxPrivateAggregationTruncationResult)

  // The elements of `histogram_suffixes` must outlive this object.
  PrivateAggregationPendingContributions(
      base::StrictNumeric<size_t> max_num_contributions,
      std::vector<std::string_view> histogram_suffixes);

  PrivateAggregationPendingContributions(
      PrivateAggregationPendingContributions&& other);
  PrivateAggregationPendingContributions& operator=(
      PrivateAggregationPendingContributions&& other);

  ~PrivateAggregationPendingContributions();

  const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
  unconditional_contributions() const {
    return unconditional_contributions_;
  }

  bool are_contributions_finalized() const {
    return are_contributions_finalized_;
  }

  // Returns false if this object has any unconditional or conditional
  // contributions.
  bool IsEmpty() const;

  const std::map<
      blink::mojom::PrivateAggregationErrorEvent,
      std::vector<blink::mojom::AggregatableReportHistogramContribution>>&
  GetConditionalContributionsForTesting() const;

  // Tracks contributions not conditional on any error event (i.e. passed via
  // `ContributeToHistogram()`). Drops contributions with value 0.
  void AddUnconditionalContributions(
      std::vector<blink::mojom::AggregatableReportHistogramContribution>
          contributions);

  // Tracks contributions conditional on an error event (i.e. passed via
  // `ContributeToHistogramOnEvent()`). Drops contributions with value 0.
  void AddConditionalContributions(
      blink::mojom::PrivateAggregationErrorEvent error_event,
      std::vector<blink::mojom::AggregatableReportHistogramContribution>
          contributions);

  // Should be called exactly once per object when no further contributions can
  // be made. `finalization_cause` should indicate whether the mojo pipe
  // disconnected or timed out.
  void MarkContributionsFinalized(TimeoutOrDisconnect finalization_cause);

  // Applies the results of the test budget call, i.e. filtering out
  // unconditional contributions that were denied in that call and possibly
  // triggering conditional contributions, and then compiles (and returns) a
  // final list of unmerged contributions. This consists of any conditional
  // contributions for error events that were triggered followed by any
  // unconditional contributions. 'Truncates' the resulting list by assuming all
  // contributions would be approved by the budgeter and removing any
  // contributions that would not fit into the report *after merging is
  // performed*. In other words, this 'truncation' determines the first n
  // `ContributionMergeKey`s in this list, where n is `max_contributions_` and
  // removes any contributions with other `ContributionMergeKey`s. Note that
  // `test_budgeter_results` must a length equal to the number of unconditional
  // contributions before the call. Can only be called after
  // `MarkContributionsFinalized()`.
  const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
  CompileFinalUnmergedContributions(
      std::vector<BudgeterResult> test_budgeter_results,
      PendingReportLimitResult pending_report_limit_result,
      NullReportBehavior null_report_behavior);

  // Applies the results of the final budget call to the result of
  // `CompileFinalUnmergedContributions()`, i.e. filtering out any contributions
  // denied by the budgeter, and then merges (approved) contributions where
  // possible (i.e. have the same `ContributionMergeKey`). Given the
  // 'truncation' performed earlier, no further truncation is needed to ensure
  // the overall length fits in limits. Consumes this object and returns the
  // final vector of merged (and truncated) contributions. Can only be called
  // after `CompileFinalUnmergedContributions()`.
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
  TakeFinalContributions(std::vector<BudgeterResult> final_budgeter_results) &&;

 private:
  // Adds `contributions` to the end of the `final_unmerged_contributions_`
  // vector, adding each associated `ContributionMergeKey` to
  // `accepted_merge_keys`. However, if a contribution would cause
  // `accepted_merge_keys.size()` to grow beyond `max_contributions_`, the
  // contribution is instead dropped and its `ContributionMergeKey` is added to
  // `truncated_merge_keys`. The contributions are processed in the order given.
  void AddToFinalUnmergedContributions(
      std::vector<blink::mojom::AggregatableReportHistogramContribution>
          contributions,
      std::set<ContributionMergeKey>& accepted_merge_keys,
      std::set<ContributionMergeKey>& truncated_merge_keys);

  void ApplyTestBudgeterResults(
      std::vector<BudgeterResult> results,
      PendingReportLimitResult pending_report_limit_result,
      NullReportBehavior null_report_behavior);
  void ApplyFinalBudgeterResults(std::vector<BudgeterResult> results);

  void RecordNumberOfContributionMergeKeysHistogram(
      size_t num_merge_keys_sent_or_truncated) const;
  void RecordNumberOfFinalUnmergedContributionsHistogram(
      size_t num_final_unmerged_contributions) const;
  void RecordTruncationHistogram(
      size_t num_truncations_due_to_unconditional_contributions,
      size_t total_truncations) const;

  bool are_contributions_finalized_ = false;

  // Contributions passed to `ContributeToHistogram()` for the associated mojo
  // receiver. Only very loose truncation (to limit worst-case memory usage) and
  // dropping zero-value contributions has occurred. No contribution merging has
  // been occurred. This is consumed in `CompileFinalUnmergedContributions()`.
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unconditional_contributions_;

  // Same considerations as `unconditional_contributions_`, but for
  // contributions passed to `ContributeToHistogramOnEvent()`.
  std::map<blink::mojom::PrivateAggregationErrorEvent,
           std::vector<blink::mojom::AggregatableReportHistogramContribution>>
      conditional_contributions_;

  // For each error event, whether the error has been triggered or not. No entry
  // for an error event if it hasn't yet been determined.
  std::map<blink::mojom::PrivateAggregationErrorEvent, bool>
      was_error_triggered_;

  // Only populated when `CompileFinalUnmergedContributions()` is called. The
  // return value of that call is a const reference to this object.
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      final_unmerged_contributions_;

  size_t max_contributions_;

  std::vector<std::string_view> histogram_suffixes_;
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
  std::variant<
      PrivateAggregationPendingContributions,
      std::vector<blink::mojom::AggregatableReportHistogramContribution>>
      contributions_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_PENDING_CONTRIBUTIONS_H_
