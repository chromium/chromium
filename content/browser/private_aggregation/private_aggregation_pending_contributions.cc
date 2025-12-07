// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_pending_contributions.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"
#include "third_party/blink/public/mojom/private_aggregation/private_aggregation_host.mojom.h"

namespace content {

using PAErrorEvent = blink::mojom::PrivateAggregationErrorEvent;

PrivateAggregationPendingContributions::PrivateAggregationPendingContributions(
    base::StrictNumeric<size_t> max_contributions,
    std::vector<std::string_view> histogram_suffixes)
    : max_contributions_(max_contributions),
      histogram_suffixes_(histogram_suffixes) {
  CHECK(base::FeatureList::IsEnabled(
      blink::features::kPrivateAggregationApiErrorReporting));
}

PrivateAggregationPendingContributions::PrivateAggregationPendingContributions(
    PrivateAggregationPendingContributions&& other) = default;
PrivateAggregationPendingContributions&
PrivateAggregationPendingContributions::operator=(
    PrivateAggregationPendingContributions&& other) = default;

PrivateAggregationPendingContributions::
    ~PrivateAggregationPendingContributions() = default;

bool PrivateAggregationPendingContributions::IsEmpty() const {
  return unconditional_contributions_.empty() &&
         conditional_contributions_.empty();
}

const std::map<
    blink::mojom::PrivateAggregationErrorEvent,
    std::vector<blink::mojom::AggregatableReportHistogramContribution>>&
PrivateAggregationPendingContributions::GetConditionalContributionsForTesting()
    const {
  return conditional_contributions_;
}

void PrivateAggregationPendingContributions::AddUnconditionalContributions(
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions) {
  CHECK(!are_contributions_finalized_);

  std::erase_if(contributions, [](auto& elem) { return elem.value == 0; });

  unconditional_contributions_.reserve(unconditional_contributions_.size() +
                                       contributions.size());

  std::ranges::transform(
      contributions, std::back_inserter(unconditional_contributions_),
      [](auto& contribution) { return std::move(contribution); });
}

void PrivateAggregationPendingContributions::AddConditionalContributions(
    PAErrorEvent error_event,
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions) {
  CHECK(!are_contributions_finalized_);

  std::erase_if(contributions, [](auto& elem) { return elem.value == 0; });

  std::vector<blink::mojom::AggregatableReportHistogramContribution>&
      destination = conditional_contributions_[error_event];

  destination.reserve(destination.size() + contributions.size());
  std::ranges::transform(
      contributions, std::back_inserter(destination),
      [](auto& contribution) { return std::move(contribution); });
}

void PrivateAggregationPendingContributions::AddToFinalUnmergedContributions(
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions,
    std::set<ContributionMergeKey>& accepted_merge_keys,
    std::set<ContributionMergeKey>& truncated_merge_keys) {
  for (blink::mojom::AggregatableReportHistogramContribution& contribution :
       contributions) {
    ContributionMergeKey merge_key(contribution);

    if (base::Contains(accepted_merge_keys, merge_key)) {
      // Bound worst-case memory usage.
      constexpr size_t kMaxUnmergedContributions = 10'000;
      if (final_unmerged_contributions_.size() < kMaxUnmergedContributions) {
        final_unmerged_contributions_.push_back(std::move(contribution));
      }
    } else if (accepted_merge_keys.size() == max_contributions_) {
      // Drop the contribution, there's no space left.

      // Bound worst-case memory usage.
      constexpr size_t kMaxTruncatedMergeKeysTracked = 10'000;
      if (truncated_merge_keys.size() < kMaxTruncatedMergeKeysTracked) {
        truncated_merge_keys.insert(std::move(merge_key));
      }

      continue;
    } else {
      accepted_merge_keys.insert(std::move(merge_key));
      final_unmerged_contributions_.push_back(std::move(contribution));
    }
  }
}
void PrivateAggregationPendingContributions::MarkContributionsFinalized(
    TimeoutOrDisconnect finalization_cause) {
  CHECK(!are_contributions_finalized_);
  are_contributions_finalized_ = true;

  was_error_triggered_[PAErrorEvent::kContributionTimeoutReached] =
      (finalization_cause == TimeoutOrDisconnect::kTimeout);
}

void PrivateAggregationPendingContributions::ApplyTestBudgeterResults(
    std::vector<BudgeterResult> results,
    PendingReportLimitResult pending_report_limit_result,
    NullReportBehavior null_report_behavior) {
  CHECK(are_contributions_finalized_);
  CHECK_EQ(results.size(), unconditional_contributions_.size());

  bool insufficient_budget = false;
  std::set<ContributionMergeKey> approved_merge_keys;

  for (size_t i = 0; i < results.size(); ++i) {
    if (results[i] == BudgeterResult::kApproved) {
      approved_merge_keys.insert(
          ContributionMergeKey(unconditional_contributions_[i]));
    } else {
      // Signal to delete this later.
      unconditional_contributions_[i].value = 0;
      insufficient_budget = true;
    }
  }
  std::erase_if(unconditional_contributions_,
                [](auto& contribution) { return contribution.value == 0; });

  bool pending_report_limit_reached =
      (pending_report_limit_result == PendingReportLimitResult::kAtLimit);

  bool empty_report_dropped =
      (unconditional_contributions_.empty() &&
       null_report_behavior == NullReportBehavior::kDontSendReport);

  bool too_many_contributions = approved_merge_keys.size() > max_contributions_;

  bool report_success = !empty_report_dropped && !too_many_contributions &&
                        !insufficient_budget && !pending_report_limit_reached;

  // Ensure this function is only called once.
  CHECK_EQ(was_error_triggered_.size(), 1u);
  was_error_triggered_[PAErrorEvent::kInsufficientBudget] = insufficient_budget;
  was_error_triggered_[PAErrorEvent::kPendingReportLimitReached] =
      pending_report_limit_reached;
  was_error_triggered_[PAErrorEvent::kEmptyReportDropped] =
      empty_report_dropped;
  was_error_triggered_[PAErrorEvent::kTooManyContributions] =
      too_many_contributions;
  was_error_triggered_[PAErrorEvent::kReportSuccess] = report_success;
}

const std::vector<blink::mojom::AggregatableReportHistogramContribution>&
PrivateAggregationPendingContributions::CompileFinalUnmergedContributions(
    std::vector<BudgeterResult> test_budgeter_results,
    PendingReportLimitResult pending_report_limit_result,
    NullReportBehavior null_report_behavior) {
  ApplyTestBudgeterResults(std::move(test_budgeter_results),
                           pending_report_limit_result, null_report_behavior);

  was_error_triggered_[PAErrorEvent::kAlreadyTriggeredExternalError] = true;

  std::set<ContributionMergeKey> accepted_merge_keys;
  std::set<ContributionMergeKey> truncated_merge_keys;

  CHECK(final_unmerged_contributions_.empty());

  for (auto i = static_cast<int>(PAErrorEvent::kMinValue);
       i <= static_cast<int>(PAErrorEvent::kMaxValue); i++) {
    auto error_event = static_cast<PAErrorEvent>(i);
    CHECK(base::Contains(was_error_triggered_, error_event));
    if (was_error_triggered_[error_event]) {
      AddToFinalUnmergedContributions(
          std::move(conditional_contributions_[error_event]),
          accepted_merge_keys, truncated_merge_keys);
    }

    // No need to keep in memory any longer
    conditional_contributions_.erase(error_event);
  }

  size_t num_truncations_due_to_unconditional_contributions = 0;
  {
    std::set<ContributionMergeKey> unconditional_contribution_merge_keys;
    for (const blink::mojom::AggregatableReportHistogramContribution&
             contribution : unconditional_contributions_) {
      unconditional_contribution_merge_keys.insert(
          ContributionMergeKey(contribution));
    }
    if (unconditional_contribution_merge_keys.size() > max_contributions_) {
      num_truncations_due_to_unconditional_contributions =
          unconditional_contribution_merge_keys.size() - max_contributions_;
    }
  }

  // Unconditional contributions come last to prioritize successful measurement
  // of errors.
  AddToFinalUnmergedContributions(std::move(unconditional_contributions_),
                                  accepted_merge_keys, truncated_merge_keys);

  RecordNumberOfContributionMergeKeysHistogram(
      /*num_merge_keys_sent_or_truncated=*/accepted_merge_keys.size() +
      truncated_merge_keys.size());
  RecordNumberOfFinalUnmergedContributionsHistogram(
      /*num_final_unmerged_contributions=*/final_unmerged_contributions_
          .size());
  RecordTruncationHistogram(num_truncations_due_to_unconditional_contributions,
                            /*total_truncations=*/truncated_merge_keys.size());

  return final_unmerged_contributions_;
}

void PrivateAggregationPendingContributions::ApplyFinalBudgeterResults(
    std::vector<BudgeterResult> results) {
  CHECK_EQ(results.size(), final_unmerged_contributions_.size());

  for (size_t i = 0; i < results.size(); ++i) {
    if (results[i] != BudgeterResult::kApproved) {
      // Signals this contribution should be deleted below.
      final_unmerged_contributions_[i].value = 0;
    }
  }
  std::erase_if(final_unmerged_contributions_,
                [](auto& contribution) { return contribution.value == 0; });
}

std::vector<blink::mojom::AggregatableReportHistogramContribution>
PrivateAggregationPendingContributions::TakeFinalContributions(
    std::vector<BudgeterResult> final_budgeter_results) && {
  ApplyFinalBudgeterResults(std::move(final_budgeter_results));

  std::map<ContributionMergeKey, size_t> indices_of_accepted_keys;
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      final_contributions;

  for (blink::mojom::AggregatableReportHistogramContribution& contribution :
       final_unmerged_contributions_) {
    ContributionMergeKey merge_key(contribution);

    auto indices_of_accepted_keys_it = indices_of_accepted_keys.find(merge_key);
    if (indices_of_accepted_keys_it != indices_of_accepted_keys.end()) {
      // As this contribution has the same bucket and filtering_id as an
      // earlier contribution, we can just sum the values.
      blink::mojom::AggregatableReportHistogramContribution& vector_entry =
          final_contributions[indices_of_accepted_keys[merge_key]];
      CHECK(ContributionMergeKey(vector_entry) == merge_key);

      vector_entry.value =
          base::ClampedNumeric(vector_entry.value) + contribution.value;
    } else {
      // No need to bound worst-case memory usage, we've already limited the
      // maximum size of this list to `max_contributions_`.
      indices_of_accepted_keys.emplace(std::move(merge_key),
                                       final_contributions.size());
      final_contributions.push_back(std::move(contribution));
    }
  }

  return final_contributions;
}

void PrivateAggregationPendingContributions::
    RecordNumberOfContributionMergeKeysHistogram(
        size_t num_merge_keys_sent_or_truncated) const {
  constexpr std::string_view kMergeKeysHistogramBase =
      "PrivacySandbox.PrivateAggregation.NumContributionMergeKeys";

  base::UmaHistogramCounts10000(kMergeKeysHistogramBase,
                                num_merge_keys_sent_or_truncated);
  for (std::string_view histogram_suffix : histogram_suffixes_) {
    base::UmaHistogramCounts10000(
        base::StrCat({kMergeKeysHistogramBase, histogram_suffix}),
        num_merge_keys_sent_or_truncated);
  }
}

void PrivateAggregationPendingContributions::
    RecordNumberOfFinalUnmergedContributionsHistogram(
        size_t num_final_unmerged_contributions) const {
  constexpr std::string_view kFinalUnmergedContributionsBase =
      "PrivacySandbox.PrivateAggregation.NumFinalUnmergedContributions";

  base::UmaHistogramCounts10000(kFinalUnmergedContributionsBase,
                                num_final_unmerged_contributions);
  for (std::string_view histogram_suffix : histogram_suffixes_) {
    base::UmaHistogramCounts10000(
        base::StrCat({kFinalUnmergedContributionsBase, histogram_suffix}),
        num_final_unmerged_contributions);
  }
}

void PrivateAggregationPendingContributions::RecordTruncationHistogram(
    size_t num_truncations_due_to_unconditional_contributions,
    size_t total_truncations) const {
  TruncationResult truncation_result;
  if (total_truncations == 0) {
    truncation_result = TruncationResult::kNoTruncation;
  } else if (num_truncations_due_to_unconditional_contributions > 0) {
    truncation_result =
        TruncationResult::kTruncationDueToUnconditionalContributions;
  } else {
    truncation_result =
        TruncationResult::kTruncationNotDueToUnconditionalContributions;
  }

  base::UmaHistogramEnumeration(
      "PrivacySandbox.PrivateAggregation.TruncationResult", truncation_result);
}

PrivateAggregationPendingContributions::Wrapper::Wrapper(
    PrivateAggregationPendingContributions pending_contributions)
    : contributions_(std::move(pending_contributions)) {
  CHECK(base::FeatureList::IsEnabled(
      blink::features::kPrivateAggregationApiErrorReporting));
}

PrivateAggregationPendingContributions::Wrapper::Wrapper(
    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        contributions_vector)
    : contributions_(std::move(contributions_vector)) {
  CHECK(!base::FeatureList::IsEnabled(
      blink::features::kPrivateAggregationApiErrorReporting));
}

PrivateAggregationPendingContributions::Wrapper::Wrapper(
    PrivateAggregationPendingContributions::Wrapper&& other) = default;

PrivateAggregationPendingContributions::Wrapper&
PrivateAggregationPendingContributions::Wrapper::operator=(
    PrivateAggregationPendingContributions::Wrapper&& other) = default;

PrivateAggregationPendingContributions::Wrapper::~Wrapper() = default;

PrivateAggregationPendingContributions&
PrivateAggregationPendingContributions::Wrapper::GetPendingContributions() {
  CHECK(base::FeatureList::IsEnabled(
      blink::features::kPrivateAggregationApiErrorReporting));
  return std::get<0>(contributions_);
}

std::vector<blink::mojom::AggregatableReportHistogramContribution>&
PrivateAggregationPendingContributions::Wrapper::GetContributionsVector() {
  CHECK(!base::FeatureList::IsEnabled(
      blink::features::kPrivateAggregationApiErrorReporting));
  return std::get<1>(contributions_);
}

}  // namespace content
