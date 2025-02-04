// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/private_aggregation/private_aggregation_pending_contributions.h"

#include <algorithm>

#include "base/test/scoped_feature_list.h"
#include "content/browser/private_aggregation/private_aggregation_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/aggregation_service/aggregatable_report.mojom.h"

namespace content {

constexpr size_t kExampleMaxNumContributions = 20;

using PendingReportLimitResult =
    PrivateAggregationPendingContributions::PendingReportLimitResult;
using NullReportBehavior =
    PrivateAggregationPendingContributions::NullReportBehavior;
using PAErrorEvent = blink::mojom::PrivateAggregationErrorEvent;

class PrivateAggregationPendingContributionsTest : public testing::Test {
 public:
  std::vector<PrivateAggregationPendingContributions::BudgeterResult>
  AllApprovalVector(size_t n) {
    return std::vector<PrivateAggregationPendingContributions::BudgeterResult>(
        n, PrivateAggregationPendingContributions::BudgeterResult::kApproved);
  }

  std::vector<PrivateAggregationPendingContributions::BudgeterResult>
  AllDenialVector(size_t n) {
    return std::vector<PrivateAggregationPendingContributions::BudgeterResult>(
        n, PrivateAggregationPendingContributions::BudgeterResult::kDenied);
  }

  void MakeConditionalContributionForEachInternalError(
      PrivateAggregationPendingContributions& pending_contributions) {
    for (auto i = static_cast<int>(PAErrorEvent::kMinValue);
         i <= static_cast<int>(PAErrorEvent::kMaxValue); i++) {
      auto error_event = static_cast<PAErrorEvent>(i);

      // Skip this non-internal event as it will always be triggered.
      if (error_event == PAErrorEvent::kAlreadyTriggeredNonInternalError) {
        continue;
      }
      pending_contributions.AddConditionalContributions(
          error_event, {blink::mojom::AggregatableReportHistogramContribution(
                           /*bucket=*/i, /*value=*/1, /*filtering_id=*/0)});
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      kPrivateAggregationApiErrorReporting};
};

TEST_F(PrivateAggregationPendingContributionsTest, Empty) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kTimeout);

  EXPECT_THAT(pending_contributions.CompileFinalUnmergedContributions(
                  {}, PendingReportLimitResult::kNotAtLimit,
                  NullReportBehavior::kDontSendReport),
              testing::IsEmpty());
  EXPECT_THAT(std::move(pending_contributions).TakeFinalContributions({}),
              testing::IsEmpty());
}

TEST_F(PrivateAggregationPendingContributionsTest, AddUnconditional) {
  const std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions_vector = {
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/1, /*value=*/2, /*filtering_id=*/3),
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/4, /*value=*/5, /*filtering_id=*/std::nullopt)};

  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  pending_contributions.AddUnconditionalContributions(contributions_vector);

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kTimeout);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                AllApprovalVector(2), PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            contributions_vector);
  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(AllApprovalVector(2)),
            contributions_vector);
}

TEST_F(PrivateAggregationPendingContributionsTest, AddUnconditionalEmpty) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  pending_contributions.AddUnconditionalContributions({});

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  EXPECT_THAT(pending_contributions.CompileFinalUnmergedContributions(
                  {}, PendingReportLimitResult::kNotAtLimit,
                  NullReportBehavior::kDontSendReport),
              testing::IsEmpty());
  EXPECT_THAT(std::move(pending_contributions).TakeFinalContributions({}),
              testing::IsEmpty());
}

TEST_F(PrivateAggregationPendingContributionsTest, AddConditionalTriggered) {
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions_vector = {
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/1, /*value=*/2, /*filtering_id=*/3),
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/4, /*value=*/5, /*filtering_id=*/std::nullopt)};

  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  pending_contributions.AddConditionalContributions(
      PAErrorEvent::kContributionTimeoutReached, contributions_vector);

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kTimeout);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                {}, PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            contributions_vector);
  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(AllApprovalVector(2)),
            contributions_vector);
}

TEST_F(PrivateAggregationPendingContributionsTest, AddConditionalNotTriggered) {
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions_vector = {
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/1, /*value=*/2, /*filtering_id=*/3),
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/4, /*value=*/5, /*filtering_id=*/std::nullopt)};

  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  pending_contributions.AddConditionalContributions(
      PAErrorEvent::kContributionTimeoutReached, contributions_vector);

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  EXPECT_THAT(pending_contributions.CompileFinalUnmergedContributions(
                  {}, PendingReportLimitResult::kNotAtLimit,
                  NullReportBehavior::kDontSendReport),
              testing::IsEmpty());
  EXPECT_THAT(std::move(pending_contributions).TakeFinalContributions({}),
              testing::IsEmpty());
}

TEST_F(PrivateAggregationPendingContributionsTest, AddConditionalEmpty) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  pending_contributions.AddConditionalContributions(
      PAErrorEvent::kTooManyContributions, {});
  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  EXPECT_THAT(pending_contributions.CompileFinalUnmergedContributions(
                  {}, PendingReportLimitResult::kNotAtLimit,
                  NullReportBehavior::kDontSendReport),
              testing::IsEmpty());
  EXPECT_THAT(std::move(pending_contributions).TakeFinalContributions({}),
              testing::IsEmpty());
}

TEST_F(PrivateAggregationPendingContributionsTest,
       UntriggeredContributionsSkipped) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  pending_contributions.AddUnconditionalContributions(
      {blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)});

  pending_contributions.AddConditionalContributions(
      PAErrorEvent::kContributionTimeoutReached,
      {blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/4, /*value=*/5, /*filtering_id=*/std::nullopt)});

  pending_contributions.AddConditionalContributions(
      PAErrorEvent::kEmptyReportDropped,
      {blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/6, /*value=*/7, /*filtering_id=*/8)});

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kTimeout);

  // Conditional contributions are placed before unconditional contributions.
  const std::vector<blink::mojom::AggregatableReportHistogramContribution>
      contributions_vector = {
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/4, /*value=*/5, /*filtering_id=*/std::nullopt),
          blink::mojom::AggregatableReportHistogramContribution(
              /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)};

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                AllApprovalVector(1), PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            contributions_vector);

  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(AllApprovalVector(2)),
            contributions_vector);
}

TEST_F(PrivateAggregationPendingContributionsTest, ContributionsMerged) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  const std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unmerged_vector = {blink::mojom::AggregatableReportHistogramContribution(
                             /*bucket=*/1, /*value=*/2, /*filtering_id=*/3),
                         blink::mojom::AggregatableReportHistogramContribution(
                             /*bucket=*/4, /*value=*/5, /*filtering_id=*/6),
                         blink::mojom::AggregatableReportHistogramContribution(
                             /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)};

  pending_contributions.AddUnconditionalContributions(unmerged_vector);

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kTimeout);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                AllApprovalVector(3), PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            unmerged_vector);

  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(AllApprovalVector(3)),
            std::vector<blink::mojom::AggregatableReportHistogramContribution>(
                {blink::mojom::AggregatableReportHistogramContribution(
                     /*bucket=*/1, /*value=*/4, /*filtering_id=*/3),
                 blink::mojom::AggregatableReportHistogramContribution(
                     /*bucket=*/4, /*value=*/5, /*filtering_id=*/6)}));
}

TEST_F(PrivateAggregationPendingContributionsTest,
       ContributionsMergedEvenIfConditionalAndUnconditional) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  pending_contributions.AddUnconditionalContributions(
      {blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)});

  pending_contributions.AddConditionalContributions(
      PAErrorEvent::kContributionTimeoutReached,
      {blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)});

  pending_contributions.AddConditionalContributions(
      PAErrorEvent::kReportSuccess,
      {blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)});

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kTimeout);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                AllApprovalVector(1), PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            std::vector<blink::mojom::AggregatableReportHistogramContribution>(
                {blink::mojom::AggregatableReportHistogramContribution(
                     /*bucket=*/1, /*value=*/2, /*filtering_id=*/3),
                 blink::mojom::AggregatableReportHistogramContribution(
                     /*bucket=*/1, /*value=*/2, /*filtering_id=*/3),
                 blink::mojom::AggregatableReportHistogramContribution(
                     /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)}));

  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(AllApprovalVector(3)),
            std::vector<blink::mojom::AggregatableReportHistogramContribution>(
                {blink::mojom::AggregatableReportHistogramContribution(
                    /*bucket=*/1, /*value=*/6, /*filtering_id=*/3)}));
}

TEST_F(PrivateAggregationPendingContributionsTest,
       ContributionsNotMergedIfConditonalOnUntriggeredEvent) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  pending_contributions.AddConditionalContributions(
      PAErrorEvent::kTooManyContributions,
      {blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)});

  pending_contributions.AddConditionalContributions(
      PAErrorEvent::kEmptyReportDropped,
      {blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)});

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kTimeout);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                {}, PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            std::vector<blink::mojom::AggregatableReportHistogramContribution>(
                {blink::mojom::AggregatableReportHistogramContribution(
                    /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)}));

  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(AllApprovalVector(1)),
            std::vector<blink::mojom::AggregatableReportHistogramContribution>(
                {blink::mojom::AggregatableReportHistogramContribution(
                    /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)}));
}

TEST_F(PrivateAggregationPendingContributionsTest,
       ContributionsTruncatedAppropriately) {
  for (size_t max_num_contributions : {1, 20, 100, 1000}) {
    PrivateAggregationPendingContributions pending_contributions(
        max_num_contributions);

    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        supplied_contributions_vector;

    for (int i = 0; i < 1000; ++i) {
      supplied_contributions_vector.emplace_back(
          /*bucket=*/0, /*value=*/1, /*filtering_id=*/2);
      supplied_contributions_vector.emplace_back(
          /*bucket=*/1 + i, /*value=*/2, /*filtering_id=*/3);
    }

    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        expected_unmerged_contribution_vector;

    for (size_t i = 0; i < 1000; ++i) {
      expected_unmerged_contribution_vector.emplace_back(
          /*bucket=*/0, /*value=*/1, /*filtering_id=*/2);
      if (i < max_num_contributions - 1) {
        expected_unmerged_contribution_vector.emplace_back(
            /*bucket=*/1 + i, /*value=*/2, /*filtering_id=*/3);
      }
    }

    std::vector<blink::mojom::AggregatableReportHistogramContribution>
        expected_contribution_vector;
    expected_contribution_vector.emplace_back(
        /*bucket=*/0, /*value=*/1000, /*filtering_id=*/2);
    for (size_t i = 0; i < max_num_contributions - 1; ++i) {
      expected_contribution_vector.emplace_back(
          /*bucket=*/1 + i, /*value=*/2, /*filtering_id=*/3);
    }

    pending_contributions.AddUnconditionalContributions(
        std::move(supplied_contributions_vector));

    pending_contributions.MarkContributionsFinalized(
        PrivateAggregationPendingContributions::TimeoutOrDisconnect::kTimeout);

    EXPECT_EQ(
        pending_contributions.CompileFinalUnmergedContributions(
            AllApprovalVector(2000), PendingReportLimitResult::kNotAtLimit,
            NullReportBehavior::kDontSendReport),
        expected_unmerged_contribution_vector);

    EXPECT_EQ(std::move(pending_contributions)
                  .TakeFinalContributions(
                      AllApprovalVector(999 + max_num_contributions)),
              expected_contribution_vector);
  }
}

TEST_F(PrivateAggregationPendingContributionsTest,
       UnconditionalContributionsPreferentiallyTruncated) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unconditional_contributions_vector;

  for (int i = 0; i < 1000; ++i) {
    unconditional_contributions_vector.emplace_back(
        /*bucket=*/1 + i, /*value=*/2, /*filtering_id=*/3);
  }

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_contribution_vector;
  expected_contribution_vector.emplace_back(
      /*bucket=*/0, /*value=*/1, /*filtering_id=*/2);
  for (size_t i = 0; i < kExampleMaxNumContributions - 1; ++i) {
    expected_contribution_vector.emplace_back(
        /*bucket=*/1 + i, /*value=*/2, /*filtering_id=*/3);
  }

  pending_contributions.AddUnconditionalContributions(
      std::move(unconditional_contributions_vector));

  pending_contributions.AddConditionalContributions(
      PAErrorEvent::kTooManyContributions,
      {blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/0, /*value=*/1, /*filtering_id=*/2)});

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kTimeout);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                AllApprovalVector(1000), PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            expected_contribution_vector);

  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(
                    AllApprovalVector(kExampleMaxNumContributions)),
            expected_contribution_vector);
}

TEST_F(
    PrivateAggregationPendingContributionsTest,
    UnconditionalContributionsStillMergedIntoTruncatedConditionalContributions) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      conditional_contributions_vector;

  conditional_contributions_vector.emplace_back(
      /*bucket=*/0, /*value=*/1, /*filtering_id=*/2);
  for (int i = 0; i < 1000; ++i) {
    conditional_contributions_vector.emplace_back(
        /*bucket=*/1 + i, /*value=*/2, /*filtering_id=*/3);
  }

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_final_contribution_vector;
  expected_final_contribution_vector.emplace_back(
      /*bucket=*/0, /*value=*/2, /*filtering_id=*/2);
  for (size_t i = 0; i < kExampleMaxNumContributions - 1; ++i) {
    expected_final_contribution_vector.emplace_back(
        /*bucket=*/1 + i, /*value=*/2, /*filtering_id=*/3);
  }

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_unmerged_contribution_vector = conditional_contributions_vector;
  expected_unmerged_contribution_vector.resize(kExampleMaxNumContributions);
  expected_unmerged_contribution_vector.emplace_back(
      /*bucket=*/0, /*value=*/1, /*filtering_id=*/2);

  pending_contributions.AddUnconditionalContributions(
      {blink::mojom::AggregatableReportHistogramContribution(
          /*bucket=*/0, /*value=*/1, /*filtering_id=*/2)});

  pending_contributions.AddConditionalContributions(
      PAErrorEvent::kContributionTimeoutReached,
      conditional_contributions_vector);

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kTimeout);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                AllApprovalVector(1), PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            expected_unmerged_contribution_vector);

  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(
                    AllApprovalVector(kExampleMaxNumContributions + 1)),
            expected_final_contribution_vector);
}

TEST_F(PrivateAggregationPendingContributionsTest,
       ZeroValueContributionsDropped) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  pending_contributions.AddUnconditionalContributions(
      {blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/1, /*value=*/2, /*filtering_id=*/3),
       blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/4, /*value=*/0, /*filtering_id=*/5)});

  pending_contributions.AddConditionalContributions(
      PAErrorEvent::kContributionTimeoutReached,
      {blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/6, /*value=*/0, /*filtering_id=*/7),
       blink::mojom::AggregatableReportHistogramContribution(
           /*bucket=*/8, /*value=*/9, /*filtering_id=*/10)});

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kTimeout);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                AllApprovalVector(1), PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            std::vector<blink::mojom::AggregatableReportHistogramContribution>(
                {blink::mojom::AggregatableReportHistogramContribution(
                     /*bucket=*/8, /*value=*/9, /*filtering_id=*/10),
                 blink::mojom::AggregatableReportHistogramContribution(
                     /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)}));

  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(AllApprovalVector(2)),
            std::vector<blink::mojom::AggregatableReportHistogramContribution>(
                {blink::mojom::AggregatableReportHistogramContribution(
                     /*bucket=*/8, /*value=*/9, /*filtering_id=*/10),
                 blink::mojom::AggregatableReportHistogramContribution(
                     /*bucket=*/1, /*value=*/2, /*filtering_id=*/3)}));
}

TEST_F(PrivateAggregationPendingContributionsTest,
       ReportSuccessTriggeredAppropriately) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  // We only expect kReportSuccess to be triggered.
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_contributions{
          {/*bucket=*/static_cast<int>(PAErrorEvent::kReportSuccess),
           /*value=*/1, /*filtering_id=*/0},
          {/*bucket=*/0, /*value=*/1, /*filtering_id=*/1}};

  MakeConditionalContributionForEachInternalError(pending_contributions);

  // Avoids triggering empty report.
  pending_contributions.AddUnconditionalContributions(
      {{/*bucket=*/0, /*value=*/1, /*filtering_id=*/1}});

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                AllApprovalVector(1), PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            expected_contributions);

  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(AllApprovalVector(2)),
            expected_contributions);
}

TEST_F(PrivateAggregationPendingContributionsTest,
       EmptyReportDroppedTriggeredAppropriately) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  // We only expect kEmptyReportDropped to be triggered. Note that this is
  // triggered based on the unconditional contributions only.
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_contributions{
          {/*bucket=*/static_cast<int>(PAErrorEvent::kEmptyReportDropped),
           /*value=*/1, /*filtering_id=*/0}};

  MakeConditionalContributionForEachInternalError(pending_contributions);

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                {}, PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            expected_contributions);

  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(AllApprovalVector(1)),
            expected_contributions);
}

TEST_F(PrivateAggregationPendingContributionsTest,
       EmptyReportDroppedNotTriggeredForDeterministicReport) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  // We only expect kReportSuccess to be triggered. Note that this is
  // triggered based on the unconditional contributions only.
  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_contributions{
          {/*bucket=*/static_cast<int>(PAErrorEvent::kReportSuccess),
           /*value=*/1, /*filtering_id=*/0}};

  MakeConditionalContributionForEachInternalError(pending_contributions);

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                {}, PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kSendNullReport),
            expected_contributions);

  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(AllApprovalVector(1)),
            expected_contributions);
}

TEST_F(PrivateAggregationPendingContributionsTest,
       PendingReportLimitReachedTriggeredAppropriately) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_contributions{
          {/*bucket=*/static_cast<int>(PAErrorEvent::kEmptyReportDropped),
           /*value=*/1, /*filtering_id=*/0},
          {/*bucket=*/static_cast<int>(
               PAErrorEvent::kPendingReportLimitReached),
           /*value=*/1, /*filtering_id=*/0}};

  MakeConditionalContributionForEachInternalError(pending_contributions);

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                {}, PendingReportLimitResult::kAtLimit,
                NullReportBehavior::kDontSendReport),
            expected_contributions);

  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(AllApprovalVector(2)),
            expected_contributions);
}

TEST_F(PrivateAggregationPendingContributionsTest,
       AlreadyTriggeredNonInternalError) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_contributions{
          {/*bucket=*/static_cast<int>(PAErrorEvent::kEmptyReportDropped),
           /*value=*/1, /*filtering_id=*/0},
          {/*bucket=*/static_cast<int>(
               PAErrorEvent::kAlreadyTriggeredNonInternalError),
           /*value=*/1, /*filtering_id=*/0}};

  MakeConditionalContributionForEachInternalError(pending_contributions);

  pending_contributions.AddConditionalContributions(
      PAErrorEvent::kAlreadyTriggeredNonInternalError,
      {{/*bucket=*/static_cast<int>(
            PAErrorEvent::kAlreadyTriggeredNonInternalError),
        /*value=*/1, /*filtering_id=*/0}});

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                {}, PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            expected_contributions);

  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(AllApprovalVector(2)),
            expected_contributions);
}

TEST_F(PrivateAggregationPendingContributionsTest,
       AllContributionsDeniedInProvisionalBudgetTest) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_contributions{
          {/*bucket=*/static_cast<int>(PAErrorEvent::kEmptyReportDropped),
           /*value=*/1, /*filtering_id=*/0},
          {/*bucket=*/static_cast<int>(PAErrorEvent::kInsufficientBudget),
           /*value=*/1, /*filtering_id=*/0}};

  MakeConditionalContributionForEachInternalError(pending_contributions);

  pending_contributions.AddUnconditionalContributions(
      {{/*bucket=*/123, /*value=*/45, /*filtering_id=*/6},
       {/*bucket=*/456, /*value=*/78, /*filtering_id=*/9},
       {/*bucket=*/789, /*value=*/12, /*filtering_id=*/3}});

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                AllDenialVector(3), PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            expected_contributions);

  EXPECT_EQ(std::move(pending_contributions)
                .TakeFinalContributions(AllApprovalVector(2)),
            expected_contributions);
}

TEST_F(PrivateAggregationPendingContributionsTest,
       AllContributionsDeniedInFinalBudgetTest) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      unconditional_contributions{
          {/*bucket=*/123, /*value=*/45, /*filtering_id=*/6},
          {/*bucket=*/456, /*value=*/78, /*filtering_id=*/9},
          {/*bucket=*/789, /*value=*/12, /*filtering_id=*/3}};

  MakeConditionalContributionForEachInternalError(pending_contributions);

  pending_contributions.AddUnconditionalContributions(
      unconditional_contributions);

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_contributions = unconditional_contributions;
  expected_contributions.insert(
      expected_contributions.begin(),
      {/*bucket=*/static_cast<int>(PAErrorEvent::kReportSuccess),
       /*value=*/1, /*filtering_id=*/0});

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                AllApprovalVector(3), PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            expected_contributions);

  EXPECT_THAT(std::move(pending_contributions)
                  .TakeFinalContributions(AllDenialVector(4)),
              testing::IsEmpty());
}

TEST_F(PrivateAggregationPendingContributionsTest,
       AllContributionsDeniedInBothQueries) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_contributions{
          {/*bucket=*/static_cast<int>(PAErrorEvent::kEmptyReportDropped),
           /*value=*/1, /*filtering_id=*/0},
          {/*bucket=*/static_cast<int>(PAErrorEvent::kInsufficientBudget),
           /*value=*/1, /*filtering_id=*/0}};

  MakeConditionalContributionForEachInternalError(pending_contributions);

  pending_contributions.AddUnconditionalContributions(
      {{/*bucket=*/123, /*value=*/45, /*filtering_id=*/6},
       {/*bucket=*/456, /*value=*/78, /*filtering_id=*/9},
       {/*bucket=*/789, /*value=*/12, /*filtering_id=*/3}});

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  EXPECT_EQ(pending_contributions.CompileFinalUnmergedContributions(
                AllDenialVector(3), PendingReportLimitResult::kNotAtLimit,
                NullReportBehavior::kDontSendReport),
            expected_contributions);

  EXPECT_THAT(std::move(pending_contributions)
                  .TakeFinalContributions(AllDenialVector(2)),
              testing::IsEmpty());
}

TEST_F(PrivateAggregationPendingContributionsTest, SomeContributionsDenied) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_first_round_contributions{
          {/*bucket=*/static_cast<int>(PAErrorEvent::kInsufficientBudget),
           /*value=*/1, /*filtering_id=*/0},
          {/*bucket=*/789, /*value=*/12, /*filtering_id=*/3}};

  MakeConditionalContributionForEachInternalError(pending_contributions);

  pending_contributions.AddUnconditionalContributions(
      {{/*bucket=*/123, /*value=*/45, /*filtering_id=*/6},
       {/*bucket=*/456, /*value=*/78, /*filtering_id=*/9},
       {/*bucket=*/789, /*value=*/12, /*filtering_id=*/3}});

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  EXPECT_EQ(
      pending_contributions.CompileFinalUnmergedContributions(
          {PrivateAggregationPendingContributions::BudgeterResult::kDenied,
           PrivateAggregationPendingContributions::BudgeterResult::kDenied,
           PrivateAggregationPendingContributions::BudgeterResult::kApproved},
          PendingReportLimitResult::kNotAtLimit,
          NullReportBehavior::kDontSendReport),
      expected_first_round_contributions);

  EXPECT_EQ(
      std::move(pending_contributions)
          .TakeFinalContributions({PrivateAggregationPendingContributions::
                                       BudgeterResult::kApproved,
                                   PrivateAggregationPendingContributions::
                                       BudgeterResult::kDenied}),
      std::vector<blink::mojom::AggregatableReportHistogramContribution>(
          {{/*bucket=*/static_cast<int>(PAErrorEvent::kInsufficientBudget),
            /*value=*/1, /*filtering_id=*/0}}));
}

TEST_F(PrivateAggregationPendingContributionsTest,
       MergeableContributionsDifferentBudgetingResults) {
  PrivateAggregationPendingContributions pending_contributions(
      kExampleMaxNumContributions);

  pending_contributions.AddUnconditionalContributions(
      {{/*bucket=*/123, /*value=*/45, /*filtering_id=*/6},
       {/*bucket=*/456, /*value=*/78, /*filtering_id=*/9},
       {/*bucket=*/123, /*value=*/45, /*filtering_id=*/6},
       {/*bucket=*/123, /*value=*/4, /*filtering_id=*/6}});

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_first_round_contributions{
          {/*bucket=*/123, /*value=*/45, /*filtering_id=*/6},
          {/*bucket=*/456, /*value=*/78, /*filtering_id=*/9},
          {/*bucket=*/123, /*value=*/4, /*filtering_id=*/6}};

  std::vector<blink::mojom::AggregatableReportHistogramContribution>
      expected_second_round_contributions{
          {/*bucket=*/456, /*value=*/78, /*filtering_id=*/9},
          {/*bucket=*/123, /*value=*/4, /*filtering_id=*/6}};

  pending_contributions.MarkContributionsFinalized(
      PrivateAggregationPendingContributions::TimeoutOrDisconnect::kDisconnect);

  EXPECT_EQ(
      pending_contributions.CompileFinalUnmergedContributions(
          {PrivateAggregationPendingContributions::BudgeterResult::kApproved,
           PrivateAggregationPendingContributions::BudgeterResult::kApproved,
           PrivateAggregationPendingContributions::BudgeterResult::kDenied,
           PrivateAggregationPendingContributions::BudgeterResult::kApproved},
          PendingReportLimitResult::kNotAtLimit,
          NullReportBehavior::kDontSendReport),
      expected_first_round_contributions);

  EXPECT_EQ(
      std::move(pending_contributions)
          .TakeFinalContributions(
              {PrivateAggregationPendingContributions::BudgeterResult::kDenied,
               PrivateAggregationPendingContributions::BudgeterResult::
                   kApproved,
               PrivateAggregationPendingContributions::BudgeterResult::
                   kApproved}),
      std::vector<blink::mojom::AggregatableReportHistogramContribution>(
          expected_second_round_contributions));
}

}  // namespace content
