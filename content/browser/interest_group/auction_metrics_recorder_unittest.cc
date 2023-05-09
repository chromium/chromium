// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_metrics_recorder.h"

#include <stdint.h>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/interest_group/auction_result.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-shared.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {
namespace {

using UkmEntry = ukm::builders::AdsInterestGroup_AuctionLatency_V2;

class AuctionMetricsRecorderTest : public testing::Test {
 public:
  AuctionMetricsRecorderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        source_id_(ukm::AssignNewSourceId()),
        recorder_(source_id_) {}

  void FastForwardTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  bool HasMetric(std::string metric_name) {
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> entries =
        ukm_recorder_.GetEntries(
            ukm::builders::AdsInterestGroup_AuctionLatency_V2::kEntryName,
            {metric_name});
    EXPECT_THAT(entries, testing::SizeIs(1));
    if (entries.size() != 1) {
      return false;
    }

    EXPECT_EQ(entries.at(0).source_id, source_id_);
    if (entries.at(0).source_id != source_id_) {
      return false;
    }

    return entries.at(0).metrics.contains(metric_name);
  }

  absl::optional<int64_t> GetMetricValue(std::string metric_name) {
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> entries =
        ukm_recorder_.GetEntries(
            ukm::builders::AdsInterestGroup_AuctionLatency_V2::kEntryName,
            {metric_name});
    EXPECT_THAT(entries, testing::SizeIs(1));
    if (entries.size() != 1) {
      return absl::nullopt;
    }

    EXPECT_EQ(entries.at(0).source_id, source_id_);
    if (entries.at(0).source_id != source_id_) {
      return absl::nullopt;
    }

    EXPECT_TRUE(entries.at(0).metrics.contains(metric_name))
        << "Missing expected metric, " << metric_name;
    if (!entries.at(0).metrics.contains(metric_name)) {
      return absl::nullopt;
    }

    return entries.at(0).metrics[metric_name];
  }

  AuctionMetricsRecorder& recorder() { return recorder_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  ukm::SourceId source_id_;
  AuctionMetricsRecorder recorder_;
};

TEST_F(AuctionMetricsRecorderTest, ResultAndEndToEndLatencyInMillis) {
  FastForwardTime(base::Milliseconds(720));
  recorder().OnAuctionEnd(AuctionResult::kNoBids);

  EXPECT_EQ(GetMetricValue(UkmEntry::kResultName),
            static_cast<int64_t>(AuctionResult::kNoBids));
  // e2e latency is reported as 700 and not 720 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kEndToEndLatencyInMillisName), 700);
}

TEST_F(AuctionMetricsRecorderTest, CrashOnRepeatedOnAuctionEnd) {
  recorder().OnAuctionEnd(AuctionResult::kNoBids);

  EXPECT_DEATH_IF_SUPPORTED(recorder().OnAuctionEnd(AuctionResult::kNoBids),
                            "");
}

TEST_F(AuctionMetricsRecorderTest, MostMethodsCrashAfterOnAuctionEnd) {
  recorder().OnAuctionEnd(AuctionResult::kNoBids);

  EXPECT_DEATH_IF_SUPPORTED(recorder().SetNumInterestGroups(4), "");
}

TEST_F(AuctionMetricsRecorderTest, LoadInterestGroupPhaseLatencyInMillis) {
  FastForwardTime(base::Milliseconds(720));
  recorder().OnLoadInterestGroupPhaseComplete();
  FastForwardTime(base::Milliseconds(280));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // e2e latency is reported as 700 and not 720 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kLoadInterestGroupPhaseLatencyInMillisName),
      700);
  EXPECT_EQ(GetMetricValue(UkmEntry::kEndToEndLatencyInMillisName), 1000);
}

TEST_F(AuctionMetricsRecorderTest, NumInterestGroups) {
  recorder().SetNumInterestGroups(42);
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 42 becomes 38 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumInterestGroupsName), 38);
}

TEST_F(AuctionMetricsRecorderTest, NumOwnersWithInterestGroupsName) {
  recorder().SetNumOwnersWithInterestGroups(62);
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 62 becomes 58 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumOwnersWithInterestGroupsName), 58);
}

TEST_F(AuctionMetricsRecorderTest, NumSellersWithBidders) {
  recorder().SetNumSellersWithBidders(72);
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 72 becomes 67 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumSellersWithBiddersName), 67);
}

TEST_F(AuctionMetricsRecorderTest, NumDistinctOwnersWithInterestGroupsName) {
  // We're repeating each of these origins twice.
  for (int repetition = 0; repetition < 2; ++repetition) {
    for (int distinct_buyer = 1; distinct_buyer <= 23; ++distinct_buyer) {
      url::Origin origin = url::Origin::Create(GURL(base::StrCat(
          {"https://sample", base::NumberToString(distinct_buyer), ".com"})));
      recorder().ReportBuyer(origin);
    }
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 23 becomes 22 because of bucketing.
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumDistinctOwnersWithInterestGroupsName),
            22);
}

TEST_F(AuctionMetricsRecorderTest, NumBidderWorklets) {
  // We're repeating each of these BidderWorkletKeys twice.
  for (int repetition = 0; repetition < 2; ++repetition) {
    for (int distinct_worklet = 1; distinct_worklet <= 27; ++distinct_worklet) {
      AuctionWorkletManager::WorkletKey worklet_key(
          AuctionWorkletManager::WorkletType::kBidder,
          GURL(base::StrCat({"https://sample.com/bidding_logic_",
                             base::NumberToString(distinct_worklet)})),
          /*wasm_url=*/absl::nullopt,
          /*signals_url=*/absl::nullopt,
          /*experiment_group_id=*/absl::nullopt);
      recorder().ReportBidderWorkletKey(worklet_key);
    }
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 27 becomes 25 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumBidderWorkletsName), 25);
}

TEST_F(AuctionMetricsRecorderTest, NumBidsAbortedByBuyerCumulativeTimeout) {
  recorder().RecordBidsAbortedByBuyerCumulativeTimeout(42);
  recorder().RecordBidsAbortedByBuyerCumulativeTimeout(30);
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 72 becomes 67 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumBidsAbortedByBuyerCumulativeTimeoutName),
      67);
}

TEST_F(AuctionMetricsRecorderTest, NumBidsAbortedByBidderWorkletFatalError) {
  for (size_t i = 0; i < 21; ++i) {
    recorder().RecordBidAbortedByBidderWorkletFatalError();
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 21 becomes 19 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumBidsAbortedByBidderWorkletFatalErrorName),
      19);
}

TEST_F(AuctionMetricsRecorderTest, NumBidsFilteredDuringInterestGroupLoad) {
  for (size_t i = 0; i < 23; ++i) {
    recorder().RecordBidFilteredDuringInterestGroupLoad();
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 23 becomes 22 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumBidsFilteredDuringInterestGroupLoadName),
      22);
}

TEST_F(AuctionMetricsRecorderTest, NumBidsFilteredDuringReprioritization) {
  for (size_t i = 0; i < 27; ++i) {
    recorder().RecordBidFilteredDuringReprioritization();
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 27 becomes 25 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumBidsFilteredDuringReprioritizationName), 25);
}

TEST_F(AuctionMetricsRecorderTest, NumBidsFilteredByPerBuyerLimits) {
  recorder().RecordBidsFilteredByPerBuyerLimits(23);
  recorder().RecordBidsFilteredByPerBuyerLimits(37);
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 60 becomes 58 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumBidsFilteredByPerBuyerLimitsName), 58);
}

TEST_F(AuctionMetricsRecorderTest, KAnonymityBidMode) {
  recorder().SetKAnonymityBidMode(
      auction_worklet::mojom::KAnonymityBidMode::kEnforce);
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(GetMetricValue(UkmEntry::kKAnonymityBidModeName),
            static_cast<int64_t>(
                auction_worklet::mojom::KAnonymityBidMode::kEnforce));
}

TEST_F(AuctionMetricsRecorderTest, NumInterestGroupsWithNoBids) {
  for (size_t i = 0; i < 14; ++i) {
    recorder().RecordInterestGroupWithNoBids();
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 14 becomes 13 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumInterestGroupsWithNoBidsName), 13);
}

TEST_F(AuctionMetricsRecorderTest, NumInterestGroupsWithOnlyNonKAnonBid) {
  for (size_t i = 0; i < 16; ++i) {
    recorder().RecordInterestGroupWithOnlyNonKAnonBid();
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 16 becomes 15 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumInterestGroupsWithOnlyNonKAnonBidName),
            15);
}

TEST_F(AuctionMetricsRecorderTest,
       NumInterestGroupsWithSameBidForKAnonAndNonKAnon) {
  for (size_t i = 0; i < 20; ++i) {
    recorder().RecordInterestGroupWithSameBidForKAnonAndNonKAnon();
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 20 becomes 19 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumInterestGroupsWithSameBidForKAnonAndNonKAnonName),
            19);
}

TEST_F(AuctionMetricsRecorderTest,
       NumInterestGroupsWithSeparateBidsForKAnonAndNonKAnon) {
  for (size_t i = 0; i < 18; ++i) {
    recorder().RecordInterestGroupWithSeparateBidsForKAnonAndNonKAnon();
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 18 becomes 17 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kNumInterestGroupsWithSeparateBidsForKAnonAndNonKAnonName),
      17);
}

TEST_F(AuctionMetricsRecorderTest,
       ComponentAuctionLatencyMetricsHaveNoValuesForSingleSellerAuctions) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_FALSE(HasMetric(UkmEntry::kMeanComponentAuctionLatencyInMillisName));
  EXPECT_FALSE(HasMetric(UkmEntry::kMaxComponentAuctionLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest, ComponentAuctionLatencyWithOneRecord) {
  recorder().RecordComponentAuctionLatency(base::Milliseconds(205));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 205 becomes 200 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanComponentAuctionLatencyInMillisName),
            200);
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxComponentAuctionLatencyInMillisName),
            200);
}

TEST_F(AuctionMetricsRecorderTest, ComponentAuctionLatencyWithTwoRecords) {
  recorder().RecordComponentAuctionLatency(base::Milliseconds(305));
  recorder().RecordComponentAuctionLatency(base::Milliseconds(505));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 405 becomes 400 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanComponentAuctionLatencyInMillisName),
            400);
  // 505 becomes 500 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxComponentAuctionLatencyInMillisName),
            500);
}

TEST_F(AuctionMetricsRecorderTest,
       ComponentAuctionLatencyIgnoresNegativeValues) {
  recorder().RecordComponentAuctionLatency(base::Milliseconds(305));
  recorder().RecordComponentAuctionLatency(base::Milliseconds(505));
  recorder().RecordComponentAuctionLatency(base::Milliseconds(-300));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 405 becomes 400 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanComponentAuctionLatencyInMillisName),
            400);
  // 505 becomes 500 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxComponentAuctionLatencyInMillisName),
            500);
}

TEST_F(AuctionMetricsRecorderTest,
       BidForOneInterestGroupLatencyMetricsHaveNoValuesForNoGenerateBids) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_FALSE(
      HasMetric(UkmEntry::kMeanBidForOneInterestGroupLatencyInMillisName));
  EXPECT_FALSE(
      HasMetric(UkmEntry::kMaxBidForOneInterestGroupLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest, BidForOneInterestGroupLatencyWithOneRecord) {
  recorder().RecordBidForOneInterestGroupLatency(base::Milliseconds(305));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 205 becomes 200 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMeanBidForOneInterestGroupLatencyInMillisName),
      300);
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMaxBidForOneInterestGroupLatencyInMillisName),
      300);
}

TEST_F(AuctionMetricsRecorderTest,
       BidForOneInterestGroupLatencyWithTwoRecords) {
  recorder().RecordBidForOneInterestGroupLatency(base::Milliseconds(405));
  recorder().RecordBidForOneInterestGroupLatency(base::Milliseconds(605));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMeanBidForOneInterestGroupLatencyInMillisName),
      500);
  // 505 becomes 500 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMaxBidForOneInterestGroupLatencyInMillisName),
      600);
}

TEST_F(AuctionMetricsRecorderTest,
       BidForOneInterestGroupLatencyIgnoresNegativeValues) {
  recorder().RecordBidForOneInterestGroupLatency(base::Milliseconds(405));
  recorder().RecordBidForOneInterestGroupLatency(base::Milliseconds(605));
  recorder().RecordBidForOneInterestGroupLatency(base::Milliseconds(-400));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMeanBidForOneInterestGroupLatencyInMillisName),
      500);
  // 505 becomes 500 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMaxBidForOneInterestGroupLatencyInMillisName),
      600);
}

TEST_F(AuctionMetricsRecorderTest,
       GenerateSingleBidLatencyMetricsHaveNoValuesForNoGenerateBids) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_FALSE(HasMetric(UkmEntry::kMeanGenerateSingleBidLatencyInMillisName));
  EXPECT_FALSE(HasMetric(UkmEntry::kMaxGenerateSingleBidLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest, GenerateSingleBidLatencyWithOneRecord) {
  recorder().RecordGenerateSingleBidLatency(base::Milliseconds(405));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 405 becomes 400 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanGenerateSingleBidLatencyInMillisName),
            400);
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxGenerateSingleBidLatencyInMillisName),
            400);
}

TEST_F(AuctionMetricsRecorderTest, GenerateSingleBidLatencyWithTwoRecords) {
  recorder().RecordGenerateSingleBidLatency(base::Milliseconds(505));
  recorder().RecordGenerateSingleBidLatency(base::Milliseconds(705));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 605 becomes 600 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanGenerateSingleBidLatencyInMillisName),
            600);
  // 505 becomes 500 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxGenerateSingleBidLatencyInMillisName),
            700);
}

TEST_F(AuctionMetricsRecorderTest,
       GenerateSingleBidLatencyIgnoresNegativeValues) {
  recorder().RecordGenerateSingleBidLatency(base::Milliseconds(505));
  recorder().RecordGenerateSingleBidLatency(base::Milliseconds(705));
  recorder().RecordGenerateSingleBidLatency(base::Milliseconds(-500));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 605 becomes 600 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanGenerateSingleBidLatencyInMillisName),
            600);
  // 505 becomes 500 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxGenerateSingleBidLatencyInMillisName),
            700);
}

TEST_F(AuctionMetricsRecorderTest,
       GenerateBidDependencyLatencyMetricsHaveNoValuesForNoGenerateBids) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_FALSE(
      HasMetric(UkmEntry::kMeanGenerateBidCodeReadyLatencyInMillisName));
  EXPECT_FALSE(
      HasMetric(UkmEntry::kMaxGenerateBidCodeReadyLatencyInMillisName));
  EXPECT_FALSE(
      HasMetric(UkmEntry::kMeanGenerateBidConfigPromisesLatencyInMillisName));
  EXPECT_FALSE(
      HasMetric(UkmEntry::kMaxGenerateBidConfigPromisesLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanGenerateBidDirectFromSellerSignalsLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMaxGenerateBidDirectFromSellerSignalsLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanGenerateBidTrustedBiddingSignalsLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMaxGenerateBidTrustedBiddingSignalsLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest,
       GenerateBidDependencyLatencyMetricsHaveNoValuesForNullOptLatencies) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/absl::nullopt,
       /*config_promises_latency=*/absl::nullopt,
       /*direct_from_seller_signals_latency=*/absl::nullopt,
       /*trusted_bidding_signals_latency=*/absl::nullopt});

  EXPECT_FALSE(
      HasMetric(UkmEntry::kMeanGenerateBidCodeReadyLatencyInMillisName));
  EXPECT_FALSE(
      HasMetric(UkmEntry::kMaxGenerateBidCodeReadyLatencyInMillisName));
  EXPECT_FALSE(
      HasMetric(UkmEntry::kMeanGenerateBidConfigPromisesLatencyInMillisName));
  EXPECT_FALSE(
      HasMetric(UkmEntry::kMaxGenerateBidConfigPromisesLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanGenerateBidDirectFromSellerSignalsLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMaxGenerateBidDirectFromSellerSignalsLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanGenerateBidTrustedBiddingSignalsLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMaxGenerateBidTrustedBiddingSignalsLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest,
       GenerateBidDependencyLatencyMetricsWithOneRecord) {
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(105),
       /*config_promises_latency=*/base::Milliseconds(205),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(305),
       /*trusted_bidding_signals_latency=*/base::Milliseconds(405)});
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 105 becomes 100 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMeanGenerateBidCodeReadyLatencyInMillisName),
      100);
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMaxGenerateBidCodeReadyLatencyInMillisName),
      100);

  // 205 becomes 200 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMeanGenerateBidConfigPromisesLatencyInMillisName),
            200);
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMaxGenerateBidConfigPromisesLatencyInMillisName),
            200);

  // 305 becomes 300 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanGenerateBidDirectFromSellerSignalsLatencyInMillisName),
      300);
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMaxGenerateBidDirectFromSellerSignalsLatencyInMillisName),
      300);

  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanGenerateBidTrustedBiddingSignalsLatencyInMillisName),
      400);
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMaxGenerateBidTrustedBiddingSignalsLatencyInMillisName),
      400);
}

TEST_F(AuctionMetricsRecorderTest,
       GenerateBidDependencyLatencyMetricsWithTwoRecords) {
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(105),
       /*config_promises_latency=*/base::Milliseconds(205),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(305),
       /*trusted_bidding_signals_latency=*/base::Milliseconds(405)});
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(305),
       /*config_promises_latency=*/base::Milliseconds(405),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(505),
       /*trusted_bidding_signals_latency=*/base::Milliseconds(605)});
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 205 becomes 200 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMeanGenerateBidCodeReadyLatencyInMillisName),
      200);
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMaxGenerateBidCodeReadyLatencyInMillisName),
      300);

  // 305 becomes 300 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMeanGenerateBidConfigPromisesLatencyInMillisName),
            300);
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMaxGenerateBidConfigPromisesLatencyInMillisName),
            400);

  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanGenerateBidDirectFromSellerSignalsLatencyInMillisName),
      400);
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMaxGenerateBidDirectFromSellerSignalsLatencyInMillisName),
      500);

  // 505 becomes 500 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanGenerateBidTrustedBiddingSignalsLatencyInMillisName),
      500);
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMaxGenerateBidTrustedBiddingSignalsLatencyInMillisName),
      600);
}

TEST_F(AuctionMetricsRecorderTest,
       GenerateBidDependencyLatencyMetricsIgnoresNegativeValues) {
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(105),
       /*config_promises_latency=*/base::Milliseconds(205),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(305),
       /*trusted_bidding_signals_latency=*/base::Milliseconds(405)});
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(305),
       /*config_promises_latency=*/base::Milliseconds(405),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(505),
       /*trusted_bidding_signals_latency=*/base::Milliseconds(605)});
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(-200),
       /*config_promises_latency=*/base::Milliseconds(-300),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(-400),
       /*trusted_bidding_signals_latency=*/base::Milliseconds(-500)});
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 205 becomes 200 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMeanGenerateBidCodeReadyLatencyInMillisName),
      200);
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMaxGenerateBidCodeReadyLatencyInMillisName),
      300);

  // 305 becomes 300 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMeanGenerateBidConfigPromisesLatencyInMillisName),
            300);
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMaxGenerateBidConfigPromisesLatencyInMillisName),
            400);

  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanGenerateBidDirectFromSellerSignalsLatencyInMillisName),
      400);
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMaxGenerateBidDirectFromSellerSignalsLatencyInMillisName),
      500);

  // 505 becomes 500 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanGenerateBidTrustedBiddingSignalsLatencyInMillisName),
      500);
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMaxGenerateBidTrustedBiddingSignalsLatencyInMillisName),
      600);
}

TEST_F(AuctionMetricsRecorderTest,
       GenerateBidCriticalPathMetricsHasNoValuesWhenNoneAreRecorded) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumGenerateBidCodeReadyOnCriticalPathName), 0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanGenerateBidCodeReadyCriticalPathLatencyInMillisName));

  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumGenerateBidConfigPromisesOnCriticalPathName),
      0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanGenerateBidConfigPromisesCriticalPathLatencyInMillisName));

  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kNumGenerateBidDirectFromSellerSignalsOnCriticalPathName),
      0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::
          kMeanGenerateBidDirectFromSellerSignalsCriticalPathLatencyInMillisName));

  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kNumGenerateBidTrustedBiddingSignalsOnCriticalPathName),
      0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::
          kMeanGenerateBidTrustedBiddingSignalsCriticalPathLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest,
       GenerateBidCriticalPathMetricsRecordedManyTimes) {
  using auction_worklet::mojom::GenerateBidDependencyLatencies;
  for (int i = 0; i < 12; ++i) {
    recorder().RecordGenerateBidDependencyLatencies(
        {/*code_ready_latency=*/base::Milliseconds(305),
         /*config_promises_latency=*/base::Milliseconds(100),
         /*direct_from_seller_signals_latency=*/base::Milliseconds(50),
         /*trusted_bidding_signals_latency=*/absl::nullopt});
  }
  for (int i = 0; i < 14; ++i) {
    recorder().RecordGenerateBidDependencyLatencies(
        {/*code_ready_latency=*/base::Milliseconds(100),
         /*config_promises_latency=*/base::Milliseconds(405),
         /*direct_from_seller_signals_latency=*/absl::nullopt,
         /*trusted_bidding_signals_latency=*/base::Milliseconds(50)});
  }
  for (int i = 0; i < 18; ++i) {
    recorder().RecordGenerateBidDependencyLatencies(
        {/*code_ready_latency=*/base::Milliseconds(50),
         /*config_promises_latency=*/absl::nullopt,
         /*direct_from_seller_signals_latency=*/base::Milliseconds(505),
         /*trusted_bidding_signals_latency=*/base::Milliseconds(100)});
  }
  for (int i = 0; i < 21; ++i) {
    recorder().RecordGenerateBidDependencyLatencies(
        {/*code_ready_latency=*/absl::nullopt,
         /*config_promises_latency=*/base::Milliseconds(100),
         /*direct_from_seller_signals_latency=*/base::Milliseconds(50),
         /*trusted_bidding_signals_latency=*/base::Milliseconds(605)});
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 12 becomes 11 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumGenerateBidCodeReadyOnCriticalPathName), 11);
  // 205 becomes 200 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanGenerateBidCodeReadyCriticalPathLatencyInMillisName),
      200);

  // 14 becomes 13 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumGenerateBidConfigPromisesOnCriticalPathName),
      13);
  // 305 becomes 300 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::
              kMeanGenerateBidConfigPromisesCriticalPathLatencyInMillisName),
      300);

  // 18 becomes 17 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kNumGenerateBidDirectFromSellerSignalsOnCriticalPathName),
      17);
  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::
              kMeanGenerateBidDirectFromSellerSignalsCriticalPathLatencyInMillisName),
      400);

  // 21 becomes 19 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kNumGenerateBidTrustedBiddingSignalsOnCriticalPathName),
      19);
  // 505 becomes 500 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::
              kMeanGenerateBidTrustedBiddingSignalsCriticalPathLatencyInMillisName),
      500);
}

TEST_F(AuctionMetricsRecorderTest, GenerateBidCriticalPathMetricsComputeMean) {
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(205),
       /*config_promises_latency=*/absl::nullopt,
       /*direct_from_seller_signals_latency=*/absl::nullopt,
       /*trusted_bidding_signals_latency=*/absl::nullopt});
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(405),
       /*config_promises_latency=*/absl::nullopt,
       /*direct_from_seller_signals_latency=*/absl::nullopt,
       /*trusted_bidding_signals_latency=*/absl::nullopt});
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumGenerateBidCodeReadyOnCriticalPathName), 2);
  // 305 becomes 300 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanGenerateBidCodeReadyCriticalPathLatencyInMillisName),
      300);
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumGenerateBidConfigPromisesOnCriticalPathName),
      0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanGenerateBidConfigPromisesCriticalPathLatencyInMillisName));

  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kNumGenerateBidDirectFromSellerSignalsOnCriticalPathName),
      0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::
          kMeanGenerateBidDirectFromSellerSignalsCriticalPathLatencyInMillisName));

  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kNumGenerateBidTrustedBiddingSignalsOnCriticalPathName),
      0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::
          kMeanGenerateBidTrustedBiddingSignalsCriticalPathLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest,
       GenerateBidCriticalPathMetricsIgnoreNegativeValues) {
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(205),
       /*config_promises_latency=*/absl::nullopt,
       /*direct_from_seller_signals_latency=*/absl::nullopt,
       /*trusted_bidding_signals_latency=*/absl::nullopt});
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(405),
       /*config_promises_latency=*/absl::nullopt,
       /*direct_from_seller_signals_latency=*/absl::nullopt,
       /*trusted_bidding_signals_latency=*/absl::nullopt});
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(-100),
       /*config_promises_latency=*/absl::nullopt,
       /*direct_from_seller_signals_latency=*/absl::nullopt,
       /*trusted_bidding_signals_latency=*/absl::nullopt});
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumGenerateBidCodeReadyOnCriticalPathName), 2);
  // 305 becomes 300 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanGenerateBidCodeReadyCriticalPathLatencyInMillisName),
      300);
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumGenerateBidConfigPromisesOnCriticalPathName),
      0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanGenerateBidConfigPromisesCriticalPathLatencyInMillisName));

  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kNumGenerateBidDirectFromSellerSignalsOnCriticalPathName),
      0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::
          kMeanGenerateBidDirectFromSellerSignalsCriticalPathLatencyInMillisName));

  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kNumGenerateBidTrustedBiddingSignalsOnCriticalPathName),
      0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::
          kMeanGenerateBidTrustedBiddingSignalsCriticalPathLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest,
       TopLevelBidsQueuedWaitingForConfigPromisesMetricsWhenNeverRecorded) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumTopLevelBidsQueuedWaitingForConfigPromisesName),
            0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::
          kMeanTimeTopLevelBidsQueuedWaitingForConfigPromisesInMillisName));
}

TEST_F(AuctionMetricsRecorderTest,
       TopLevelBidsQueuedWaitingForConfigPromisesMetricWhenRecordedManyTimes) {
  for (int i = 0; i < 12; ++i) {
    recorder().RecordTopLevelBidQueuedWaitingForConfigPromises(
        base::Milliseconds(305));
    recorder().RecordTopLevelBidQueuedWaitingForConfigPromises(
        base::Milliseconds(505));
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 24 becomes 22 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumTopLevelBidsQueuedWaitingForConfigPromisesName),
            22);
  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::
              kMeanTimeTopLevelBidsQueuedWaitingForConfigPromisesInMillisName),
      400);
}

TEST_F(AuctionMetricsRecorderTest,
       TopLevelBidsQueuedWaitingForConfigPromisesMetricIgnoresNegativeValues) {
  for (int i = 0; i < 12; ++i) {
    recorder().RecordBidQueuedWaitingForConfigPromises(
        base::Milliseconds(-200));
    recorder().RecordTopLevelBidQueuedWaitingForConfigPromises(
        base::Milliseconds(305));
    recorder().RecordTopLevelBidQueuedWaitingForConfigPromises(
        base::Milliseconds(505));
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 24 becomes 22 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumTopLevelBidsQueuedWaitingForConfigPromisesName),
            22);
  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::
              kMeanTimeTopLevelBidsQueuedWaitingForConfigPromisesInMillisName),
      400);
}

TEST_F(AuctionMetricsRecorderTest,
       TopLevelBidsQueuedWaitingForSellerWorkletMetricsWhenNeverRecorded) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumTopLevelBidsQueuedWaitingForSellerWorkletName),
            0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::
          kMeanTimeTopLevelBidsQueuedWaitingForSellerWorkletInMillisName));
}

TEST_F(AuctionMetricsRecorderTest,
       TopLevelBidsQueuedWaitingForSellerWorkletMetricWhenRecordedManyTimes) {
  for (int i = 0; i < 12; ++i) {
    recorder().RecordTopLevelBidQueuedWaitingForSellerWorklet(
        base::Milliseconds(305));
    recorder().RecordTopLevelBidQueuedWaitingForSellerWorklet(
        base::Milliseconds(505));
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 24 becomes 22 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumTopLevelBidsQueuedWaitingForSellerWorkletName),
            22);
  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::
              kMeanTimeTopLevelBidsQueuedWaitingForSellerWorkletInMillisName),
      400);
}

TEST_F(AuctionMetricsRecorderTest,
       TopLevelBidsQueuedWaitingForSellerWorkletMetricIgnoresNegativeValues) {
  for (int i = 0; i < 12; ++i) {
    recorder().RecordTopLevelBidQueuedWaitingForSellerWorklet(
        base::Milliseconds(-200));
    recorder().RecordTopLevelBidQueuedWaitingForSellerWorklet(
        base::Milliseconds(305));
    recorder().RecordTopLevelBidQueuedWaitingForSellerWorklet(
        base::Milliseconds(505));
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 24 becomes 22 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumTopLevelBidsQueuedWaitingForSellerWorkletName),
            22);
  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::
              kMeanTimeTopLevelBidsQueuedWaitingForSellerWorkletInMillisName),
      400);
}

TEST_F(AuctionMetricsRecorderTest,
       BidsQueuedWaitingForConfigPromisesMetricsWhenNeverRecorded) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumBidsQueuedWaitingForConfigPromisesName), 0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanTimeBidsQueuedWaitingForConfigPromisesInMillisName));
}

TEST_F(AuctionMetricsRecorderTest,
       BidsQueuedWaitingForConfigPromisesMetricWhenRecordedManyTimes) {
  for (int i = 0; i < 12; ++i) {
    recorder().RecordBidQueuedWaitingForConfigPromises(base::Milliseconds(305));
    recorder().RecordBidQueuedWaitingForConfigPromises(base::Milliseconds(505));
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 24 becomes 22 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumBidsQueuedWaitingForConfigPromisesName), 22);
  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanTimeBidsQueuedWaitingForConfigPromisesInMillisName),
      400);
}

TEST_F(AuctionMetricsRecorderTest,
       BidsQueuedWaitingForConfigPromisesMetricIgnoresNegativeValues) {
  for (int i = 0; i < 12; ++i) {
    recorder().RecordBidQueuedWaitingForConfigPromises(
        base::Milliseconds(-200));
    recorder().RecordBidQueuedWaitingForConfigPromises(base::Milliseconds(305));
    recorder().RecordBidQueuedWaitingForConfigPromises(base::Milliseconds(505));
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 24 becomes 22 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumBidsQueuedWaitingForConfigPromisesName), 22);
  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanTimeBidsQueuedWaitingForConfigPromisesInMillisName),
      400);
}

TEST_F(AuctionMetricsRecorderTest,
       BidsQueuedWaitingForSellerWorkletMetricsWhenNeverRecorded) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(GetMetricValue(UkmEntry::kNumBidsQueuedWaitingForSellerWorkletName),
            0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanTimeBidsQueuedWaitingForSellerWorkletInMillisName));
}

TEST_F(AuctionMetricsRecorderTest,
       BidsQueuedWaitingForSellerWorkletMetricWhenRecordedManyTimes) {
  for (int i = 0; i < 12; ++i) {
    recorder().RecordBidQueuedWaitingForSellerWorklet(base::Milliseconds(305));
    recorder().RecordBidQueuedWaitingForSellerWorklet(base::Milliseconds(505));
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 24 becomes 22 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumBidsQueuedWaitingForSellerWorkletName),
            22);
  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanTimeBidsQueuedWaitingForSellerWorkletInMillisName),
      400);
}

TEST_F(AuctionMetricsRecorderTest,
       BidsQueuedWaitingForSellerWorkletMetricIgnoresNegativeValues) {
  for (int i = 0; i < 12; ++i) {
    recorder().RecordBidQueuedWaitingForSellerWorklet(base::Milliseconds(-200));
    recorder().RecordBidQueuedWaitingForSellerWorklet(base::Milliseconds(305));
    recorder().RecordBidQueuedWaitingForSellerWorklet(base::Milliseconds(505));
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 24 becomes 22 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumBidsQueuedWaitingForSellerWorkletName),
            22);
  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanTimeBidsQueuedWaitingForSellerWorkletInMillisName),
      400);
}

}  // namespace
}  // namespace content
