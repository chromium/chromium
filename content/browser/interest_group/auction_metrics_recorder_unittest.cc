// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_metrics_recorder.h"

#include <stdint.h>

#include <optional>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/interest_group/additional_bid_result.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/public/browser/auction_result.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-shared.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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

  // Advances the clock and returns the new current time.
  base::TimeTicks FastForwardTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
    return base::TimeTicks::Now();
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

  std::optional<int64_t> GetMetricValue(std::string metric_name) {
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> entries =
        ukm_recorder_.GetEntries(
            ukm::builders::AdsInterestGroup_AuctionLatency_V2::kEntryName,
            {metric_name});
    EXPECT_THAT(entries, testing::SizeIs(1));
    if (entries.size() != 1) {
      return std::nullopt;
    }

    EXPECT_EQ(entries.at(0).source_id, source_id_);
    if (entries.at(0).source_id != source_id_) {
      return std::nullopt;
    }

    EXPECT_TRUE(entries.at(0).metrics.contains(metric_name))
        << "Missing expected metric, " << metric_name;
    if (!entries.at(0).metrics.contains(metric_name)) {
      return std::nullopt;
    }

    return entries.at(0).metrics[metric_name];
  }

  AuctionMetricsRecorder& recorder() { return recorder_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  ukm::SourceId source_id_;
  AuctionMetricsRecorder recorder_;
  base::HistogramTester histogram_tester_;
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

TEST_F(AuctionMetricsRecorderTest, NoLoadInterestGroupPhaseLatencyAndEndTime) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);
  EXPECT_FALSE(HasMetric(UkmEntry::kLoadInterestGroupPhaseLatencyInMillisName));
  EXPECT_FALSE(HasMetric(UkmEntry::kLoadInterestGroupPhaseEndTimeInMillisName));
}

TEST_F(AuctionMetricsRecorderTest, LoadInterestGroupPhaseLatencyAndEndTime) {
  FastForwardTime(base::Milliseconds(722));
  recorder().OnLoadInterestGroupPhaseComplete();
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // phase latency is reported as 700 (duration bucketing)
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kLoadInterestGroupPhaseLatencyInMillisName),
      700);

  // phase end time is reported as 720 (linear 10 ms bucketing)
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kLoadInterestGroupPhaseEndTimeInMillisName),
      720);
}

TEST_F(AuctionMetricsRecorderTest,
       WorkletCreationPhaseMetricsHaveNoValuesForNoWorkletsCreated) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_FALSE(HasMetric(UkmEntry::kWorkletCreationPhaseStartTimeInMillisName));
  EXPECT_FALSE(HasMetric(UkmEntry::kWorkletCreationPhaseEndTimeInMillisName));
}

TEST_F(AuctionMetricsRecorderTest, WorkletCreationPhaseMetricsWithOneRecord) {
  FastForwardTime(base::Milliseconds(722));
  recorder().OnWorkletRequested();  // start of phase at 722
  FastForwardTime(base::Milliseconds(100));
  recorder().OnWorkletReady();  // end of phase at 822
  FastForwardTime(base::Milliseconds(100));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 722 becomes 720 with 10 ms linear bucketing.
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kWorkletCreationPhaseStartTimeInMillisName),
      720);
  // 822 becomes 820 with 10 ms linear bucketing.
  EXPECT_EQ(GetMetricValue(UkmEntry::kWorkletCreationPhaseEndTimeInMillisName),
            820);
}

TEST_F(AuctionMetricsRecorderTest, WorkletCreationPhaseMetricsWithTwoRecords) {
  FastForwardTime(base::Milliseconds(722));
  recorder().OnWorkletRequested();  // start of phase at 722
  FastForwardTime(base::Milliseconds(100));
  recorder().OnWorkletReady();
  FastForwardTime(base::Milliseconds(100));
  recorder().OnWorkletRequested();
  FastForwardTime(base::Milliseconds(100));
  recorder().OnWorkletReady();  // end of phase at 1022
  FastForwardTime(base::Milliseconds(100));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 722 becomes 720 with 10 ms linear bucketing.
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kWorkletCreationPhaseStartTimeInMillisName),
      720);
  // 1022 becomes 1020 with 10 ms linear bucketing.
  EXPECT_EQ(GetMetricValue(UkmEntry::kWorkletCreationPhaseEndTimeInMillisName),
            1020);
}

TEST_F(AuctionMetricsRecorderTest, NumInterestGroups) {
  recorder().SetNumInterestGroups(42);
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 42 becomes 38 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumInterestGroupsName), 38);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.NumInterestGroups",
      /*sample=*/42, /*expected_bucket_count=*/1);
}

TEST_F(AuctionMetricsRecorderTest, NumOwnersWithInterestGroups) {
  recorder().SetNumOwnersWithInterestGroups(62);
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 62 becomes 58 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumOwnersWithInterestGroupsName), 58);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.NumOwnersWithInterestGroups",
      /*sample=*/62, /*expected_bucket_count=*/1);
}

TEST_F(AuctionMetricsRecorderTest, NumOwnersWithoutInterestGroups) {
  recorder().SetNumOwnersWithoutInterestGroups(62);
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 62 becomes 58 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumOwnersWithoutInterestGroupsName), 58);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.NumOwnersWithoutInterestGroups",
      /*sample=*/62, /*expected_bucket_count=*/1);
}

TEST_F(AuctionMetricsRecorderTest, NumSellersWithBidders) {
  recorder().SetNumSellersWithBidders(72);
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 72 becomes 67 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumSellersWithBiddersName), 67);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.NumSellersWithBidders",
      /*sample=*/72, /*expected_bucket_count=*/1);
}

// TODO rest of the histograms, mock AuctionMetricsRecorder,
// use that in auction_runner_unittest.cc.

TEST_F(AuctionMetricsRecorderTest,
       NumNegativeInterestGroupsHasNoValueIfNeverRecorded) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);
  EXPECT_FALSE(HasMetric(UkmEntry::kNumNegativeInterestGroupsName));
  histogram_tester().ExpectTotalCount(
      /*name=*/"Ads.InterestGroup.Auction.NumNegativeInterestGroups",
      /*expected_count=*/0);
}

TEST_F(AuctionMetricsRecorderTest, NumNegativeInterestGroups) {
  recorder().RecordNegativeInterestGroups(11);
  recorder().RecordNegativeInterestGroups(12);
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 23 becomes 22 because of bucketing.
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumNegativeInterestGroupsName), 22);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.NumNegativeInterestGroups",
      /*sample=*/23, /*expected_bucket_count=*/1);
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
          /*wasm_url=*/std::nullopt,
          /*signals_url=*/std::nullopt,
          /*needs_cors_for_additional_bid=*/false,
          /*experiment_group_id=*/std::nullopt,
          /*trusted_bidding_signals_slot_size_param=*/"",
          /*trusted_signals_coordinator=*/std::nullopt);
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

TEST_F(AuctionMetricsRecorderTest, NumAdditionalBidsSentForScoring) {
  for (size_t i = 0; i < 21; ++i) {
    recorder().RecordAdditionalBidResult(AdditionalBidResult::kSentForScoring);
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 21 becomes 19 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumAdditionalBidsSentForScoringName), 19);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.AdditionalBids.Result",
      /*sample=*/AdditionalBidResult::kSentForScoring,
      /*expected_bucket_count=*/21);
}

TEST_F(AuctionMetricsRecorderTest, NumAdditionalBidsNegativeTargeted) {
  for (size_t i = 0; i < 24; ++i) {
    recorder().RecordAdditionalBidResult(
        AdditionalBidResult::kNegativeTargeted);
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 24 becomes 22 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumAdditionalBidsNegativeTargetedName),
            22);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.AdditionalBids.Result",
      /*sample=*/AdditionalBidResult::kNegativeTargeted,
      /*expected_bucket_count=*/24);
}

TEST_F(AuctionMetricsRecorderTest,
       SetNumAdditionalBidsRejectedDueToInvalidBase64) {
  for (size_t i = 0; i < 31; ++i) {
    recorder().RecordAdditionalBidResult(
        AdditionalBidResult::kRejectedDueToInvalidBase64);
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 31 becomes 29 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumAdditionalBidsRejectedDueToInvalidBase64Name),
            29);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.AdditionalBids.Result",
      /*sample=*/AdditionalBidResult::kRejectedDueToInvalidBase64,
      /*expected_bucket_count=*/31);
}

TEST_F(AuctionMetricsRecorderTest,
       NumAdditionalBidsRejectedDueToSignedBidJsonParseError) {
  for (size_t i = 0; i < 42; ++i) {
    recorder().RecordAdditionalBidResult(
        AdditionalBidResult::kRejectedDueToSignedBidJsonParseError);
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 42 becomes 38 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kNumAdditionalBidsRejectedDueToSignedBidJsonParseErrorName),
      38);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.AdditionalBids.Result",
      /*sample=*/AdditionalBidResult::kRejectedDueToSignedBidJsonParseError,
      /*expected_bucket_count=*/42);
}

TEST_F(AuctionMetricsRecorderTest,
       NumAdditionalBidsRejectedDueToSignedBidDecodeError) {
  for (size_t i = 0; i < 30; ++i) {
    recorder().RecordAdditionalBidResult(
        AdditionalBidResult::kRejectedDueToSignedBidDecodeError);
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 30 becomes 29 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kNumAdditionalBidsRejectedDueToSignedBidDecodeErrorName),
      29);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.AdditionalBids.Result",
      /*sample=*/AdditionalBidResult::kRejectedDueToSignedBidDecodeError,
      /*expected_bucket_count=*/30);
}

TEST_F(AuctionMetricsRecorderTest,
       NumAdditionalBidsRejectedDueToJsonParseError) {
  for (size_t i = 0; i < 43; ++i) {
    recorder().RecordAdditionalBidResult(
        AdditionalBidResult::kRejectedDueToJsonParseError);
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 43 becomes 38 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumAdditionalBidsRejectedDueToJsonParseErrorName),
            38);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.AdditionalBids.Result",
      /*sample=*/AdditionalBidResult::kRejectedDueToJsonParseError,
      /*expected_bucket_count=*/43);
}

TEST_F(AuctionMetricsRecorderTest, NumAdditionalBidsRejectedDueToDecodeError) {
  for (size_t i = 0; i < 30; ++i) {
    recorder().RecordAdditionalBidResult(
        AdditionalBidResult::kRejectedDueToDecodeError);
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 30 becomes 29 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kNumAdditionalBidsRejectedDueToDecodeErrorName),
      29);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.AdditionalBids.Result",
      /*sample=*/AdditionalBidResult::kRejectedDueToDecodeError,
      /*expected_bucket_count=*/30);
}

TEST_F(AuctionMetricsRecorderTest,
       NumAdditionalBidsRejectedDueToBuyerNotAllowed) {
  for (size_t i = 0; i < 35; ++i) {
    recorder().RecordAdditionalBidResult(
        AdditionalBidResult::kRejectedDueToBuyerNotAllowed);
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 35 becomes 33 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumAdditionalBidsRejectedDueToBuyerNotAllowedName),
            33);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.AdditionalBids.Result",
      /*sample=*/AdditionalBidResult::kRejectedDueToBuyerNotAllowed,
      /*expected_bucket_count=*/35);
}

TEST_F(AuctionMetricsRecorderTest,
       NumAdditionalBidsRejectedDueToCurrencyMismatch) {
  for (size_t i = 0; i < 42; ++i) {
    recorder().RecordAdditionalBidResult(
        AdditionalBidResult::kRejectedDueToCurrencyMismatch);
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 42 becomes 38 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumAdditionalBidsRejectedDueToCurrencyMismatchName),
            38);
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.AdditionalBids.Result",
      /*sample=*/AdditionalBidResult::kRejectedDueToCurrencyMismatch,
      /*expected_bucket_count=*/42);
}

TEST_F(AuctionMetricsRecorderTest, AdditionalBidsDecodeLatency) {
  for (size_t i = 0; i < 23; ++i) {
    recorder().RecordAdditionalBidDecodeLatency(base::Milliseconds(105));
  }
  for (size_t i = 0; i < 27; ++i) {
    recorder().RecordAdditionalBidDecodeLatency(base::Milliseconds(305));
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // Mean latency is 213, which becomes 200 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMeanAdditionalBidDecodeLatencyInMillisName),
      200);
  // Max latency is 305, which becomes 300 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMaxAdditionalBidDecodeLatencyInMillisName),
      300);

  // 105 becomes 96 and 305 becomes 268 because of bucketing
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Ads.InterestGroup.Auction.AdditionalBids.DecodeLatency"),
              base::BucketsAre(base::Bucket(96, 23), base::Bucket(268, 27)));
}

TEST_F(AuctionMetricsRecorderTest,
       RecordNegativeInterestGroupIgnoredDueToInvalidSignature) {
  for (size_t i = 0; i < 41; ++i) {
    recorder().RecordNegativeInterestGroupIgnoredDueToInvalidSignature();
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 41 becomes 38 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kNumNegativeInterestGroupsIgnoredDueToInvalidSignatureName),
      38);
}

TEST_F(AuctionMetricsRecorderTest,
       RecordNegativeInterestGroupIgnoredDueToJoiningOriginMismatch) {
  for (size_t i = 0; i < 20; ++i) {
    recorder().RecordNegativeInterestGroupIgnoredDueToJoiningOriginMismatch();
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 20 becomes 19 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::
              kNumNegativeInterestGroupsIgnoredDueToJoiningOriginMismatchName),
      19);
}

TEST_F(AuctionMetricsRecorderTest, KAnonymityBidMode) {
  recorder().SetKAnonymityBidMode(
      auction_worklet::mojom::KAnonymityBidMode::kEnforce);
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(GetMetricValue(UkmEntry::kKAnonymityBidModeName),
            static_cast<int64_t>(
                auction_worklet::mojom::KAnonymityBidMode::kEnforce));
}

TEST_F(AuctionMetricsRecorderTest, NumConfigPromises) {
  recorder().SetNumConfigPromises(14);
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 14 becomes 13 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumConfigPromisesName), 13);
}

TEST_F(AuctionMetricsRecorderTest, OnConfigPromisesResolvedNeverCalled) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(GetMetricValue(UkmEntry::kNumAuctionsWithConfigPromisesName), 0);
  EXPECT_FALSE(
      HasMetric(UkmEntry::kMeanConfigPromisesResolvedLatencyInMillisName));
  EXPECT_FALSE(
      HasMetric(UkmEntry::kMaxConfigPromisesResolvedLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanConfigPromisesResolvedCriticalPathLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMaxConfigPromisesResolvedCriticalPathLatencyInMillisName));

  histogram_tester().ExpectTotalCount(
      /*name=*/"Ads.InterestGroup.Auction.ConfigPromises.Latency",
      /*expected_count=*/0);
  histogram_tester().ExpectTotalCount(
      /*name=*/"Ads.InterestGroup.Auction.ConfigPromises.CriticalPathLatency",
      /*expected_count=*/0);
}

TEST_F(AuctionMetricsRecorderTest, OnConfigPromisesResolvedCalledOnce) {
  FastForwardTime(base::Milliseconds(310));
  recorder().OnLoadInterestGroupPhaseComplete();
  FastForwardTime(base::Milliseconds(410));
  recorder().OnConfigPromisesResolved();
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(GetMetricValue(UkmEntry::kNumAuctionsWithConfigPromisesName), 1);

  // Latency is reported as 700 and not 720 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMeanConfigPromisesResolvedLatencyInMillisName),
      700);
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMaxConfigPromisesResolvedLatencyInMillisName),
      700);

  // Latency is reported as 400 and not 410 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanConfigPromisesResolvedCriticalPathLatencyInMillisName),
      400);
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMaxConfigPromisesResolvedCriticalPathLatencyInMillisName),
      400);

  // 720's bucket starts at 633
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Ads.InterestGroup.Auction.ConfigPromises.Latency"),
              base::BucketsAre(base::Bucket(633, 1)));
  // 410's bucket starts at 378
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Ads.InterestGroup.Auction.ConfigPromises.CriticalPathLatency"),
      base::BucketsAre(base::Bucket(378, 1)));
}

TEST_F(AuctionMetricsRecorderTest, OnConfigPromisesResolvedCalledTwice) {
  FastForwardTime(base::Milliseconds(210));
  recorder().OnConfigPromisesResolved();  // At 210, CP latency of 0
  FastForwardTime(base::Milliseconds(210));
  recorder().OnLoadInterestGroupPhaseComplete();
  FastForwardTime(base::Milliseconds(210));
  recorder().OnConfigPromisesResolved();  // At 630, CP latency of 210
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(GetMetricValue(UkmEntry::kNumAuctionsWithConfigPromisesName), 2);

  // Latency is reported as 400 and not 420 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMeanConfigPromisesResolvedLatencyInMillisName),
      400);
  // Latency is reported as 600 and not 630 because of bucketing
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kMaxConfigPromisesResolvedLatencyInMillisName),
      600);

  // Latency is reported as 100 and not 105 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanConfigPromisesResolvedCriticalPathLatencyInMillisName),
      100);
  // Latency is reported as 200 and not 210 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMaxConfigPromisesResolvedCriticalPathLatencyInMillisName),
      200);

  // 210's bucket starts at 190, and 630's bucket starts at 533
  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Ads.InterestGroup.Auction.ConfigPromises.Latency"),
              base::BucketsAre(base::Bucket(190, 1), base::Bucket(533, 1)));
  // 0's bucket starts at 0, and 210's bucket starts at 190
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Ads.InterestGroup.Auction.ConfigPromises.CriticalPathLatency"),
      base::BucketsAre(base::Bucket(0, 1), base::Bucket(190, 1)));
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

TEST_F(AuctionMetricsRecorderTest, NumInterestGroupsWithOtherMultiBid) {
  for (size_t i = 0; i < 30; ++i) {
    recorder().RecordInterestGroupWithOtherMultiBid();
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 30 becomes 29 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumInterestGroupsWithOtherMultiBidName),
            29);
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
      {/*code_ready_latency=*/std::nullopt,
       /*config_promises_latency=*/std::nullopt,
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_bidding_signals_latency=*/std::nullopt,
       /*deps_wait_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_finish_time*/ base::TimeTicks::Now()});

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
       /*trusted_bidding_signals_latency=*/base::Milliseconds(405),
       /*deps_wait_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_finish_time*/ base::TimeTicks::Now()});
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
       /*trusted_bidding_signals_latency=*/base::Milliseconds(405),
       /*deps_wait_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_finish_time*/ base::TimeTicks::Now()});
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(305),
       /*config_promises_latency=*/base::Milliseconds(405),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(505),
       /*trusted_bidding_signals_latency=*/base::Milliseconds(605),
       /*deps_wait_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_finish_time*/ base::TimeTicks::Now()});
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
       /*trusted_bidding_signals_latency=*/base::Milliseconds(405),
       /*deps_wait_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_finish_time*/ base::TimeTicks::Now()});
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(305),
       /*config_promises_latency=*/base::Milliseconds(405),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(505),
       /*trusted_bidding_signals_latency=*/base::Milliseconds(605),
       /*deps_wait_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_finish_time*/ base::TimeTicks::Now()});
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(-200),
       /*config_promises_latency=*/base::Milliseconds(-300),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(-400),
       /*trusted_bidding_signals_latency=*/base::Milliseconds(-500),
       /*deps_wait_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_finish_time*/ base::TimeTicks::Now()});
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
         /*trusted_bidding_signals_latency=*/std::nullopt,
         /*deps_wait_start_time*/ base::TimeTicks::Now(),
         /*generate_bid_start_time*/ base::TimeTicks::Now(),
         /*generate_bid_finish_time*/ base::TimeTicks::Now()});
  }
  for (int i = 0; i < 14; ++i) {
    recorder().RecordGenerateBidDependencyLatencies(
        {/*code_ready_latency=*/base::Milliseconds(100),
         /*config_promises_latency=*/base::Milliseconds(405),
         /*direct_from_seller_signals_latency=*/std::nullopt,
         /*trusted_bidding_signals_latency=*/base::Milliseconds(50),
         /*deps_wait_start_time*/ base::TimeTicks::Now(),
         /*generate_bid_start_time*/ base::TimeTicks::Now(),
         /*generate_bid_finish_time*/ base::TimeTicks::Now()});
  }
  for (int i = 0; i < 18; ++i) {
    recorder().RecordGenerateBidDependencyLatencies(
        {/*code_ready_latency=*/base::Milliseconds(50),
         /*config_promises_latency=*/std::nullopt,
         /*direct_from_seller_signals_latency=*/base::Milliseconds(505),
         /*trusted_bidding_signals_latency=*/base::Milliseconds(100),
         /*deps_wait_start_time*/ base::TimeTicks::Now(),
         /*generate_bid_start_time*/ base::TimeTicks::Now(),
         /*generate_bid_finish_time*/ base::TimeTicks::Now()});
  }
  for (int i = 0; i < 21; ++i) {
    recorder().RecordGenerateBidDependencyLatencies(
        {/*code_ready_latency=*/std::nullopt,
         /*config_promises_latency=*/base::Milliseconds(100),
         /*direct_from_seller_signals_latency=*/base::Milliseconds(50),
         /*trusted_bidding_signals_latency=*/base::Milliseconds(605),
         /*deps_wait_start_time*/ base::TimeTicks::Now(),
         /*generate_bid_start_time*/ base::TimeTicks::Now(),
         /*generate_bid_finish_time*/ base::TimeTicks::Now()});
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
       /*config_promises_latency=*/std::nullopt,
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_bidding_signals_latency=*/std::nullopt,
       /*deps_wait_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_finish_time*/ base::TimeTicks::Now()});
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(405),
       /*config_promises_latency=*/std::nullopt,
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_bidding_signals_latency=*/std::nullopt,
       /*deps_wait_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_finish_time*/ base::TimeTicks::Now()});
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
       /*config_promises_latency=*/std::nullopt,
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_bidding_signals_latency=*/std::nullopt,
       /*deps_wait_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_finish_time*/ base::TimeTicks::Now()});
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(405),
       /*config_promises_latency=*/std::nullopt,
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_bidding_signals_latency=*/std::nullopt,
       /*deps_wait_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_finish_time*/ base::TimeTicks::Now()});
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(-100),
       /*config_promises_latency=*/std::nullopt,
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_bidding_signals_latency=*/std::nullopt,
       /*deps_wait_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_start_time*/ base::TimeTicks::Now(),
       /*generate_bid_finish_time*/ base::TimeTicks::Now()});
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
       GenerateBidPhaseMetricsHaveNoValuesForNoGenerateBids) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_FALSE(HasMetric(UkmEntry::kBidSignalsFetchPhaseStartTimeInMillisName));
  EXPECT_FALSE(HasMetric(UkmEntry::kBidSignalsFetchPhaseEndTimeInMillisName));
  EXPECT_FALSE(HasMetric(UkmEntry::kBidGenerationPhaseStartTimeInMillisName));
  EXPECT_FALSE(HasMetric(UkmEntry::kBidGenerationPhaseEndTimeInMillisName));
}

// Times for this test:
// - GenerateBid called at 205
// - Signals fetched by (205 + 410) = 615
// - GenerateBid complete by (615 + 190) = 805
TEST_F(AuctionMetricsRecorderTest, GenerateBidPhaseMetricsWithOneRecord) {
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/std::nullopt,
       /*config_promises_latency=*/std::nullopt,
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_bidding_signals_latency=*/std::nullopt,
       /*deps_wait_start_time=*/FastForwardTime(base::Milliseconds(205)),
       /*generate_bid_start_time=*/FastForwardTime(base::Milliseconds(410)),
       /*generate_bid_finish_time=*/FastForwardTime(base::Milliseconds(190))});
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 205 becomes 200 with 10 ms linear bucketing.
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kBidSignalsFetchPhaseStartTimeInMillisName),
      200);
  // 615 becomes 610 with 10 ms linear bucketing.
  EXPECT_EQ(GetMetricValue(UkmEntry::kBidSignalsFetchPhaseEndTimeInMillisName),
            610);
  // 615 becomes 610 with 10 ms linear bucketing.
  EXPECT_EQ(GetMetricValue(UkmEntry::kBidGenerationPhaseStartTimeInMillisName),
            610);
  // 805 becomes 800 with 10 ms linear bucketing.
  EXPECT_EQ(GetMetricValue(UkmEntry::kBidGenerationPhaseEndTimeInMillisName),
            800);
}

// Times for this test:
// - GenerateBid[bid2] called at 115
// - GenerateBid[bid1] called at (115 + 105) = 220
// - Signals fetched[bid1] by (220 + 415) = 635
// - GenerateBid complete[bid1] by (635 + 250) = 885
// - Signals fetched[bid2] by (885 + 610) = 1495
// - GenerateBid complete[bid2] by (1495 + 100) = 1595
//
// Phases:
// - BidSignalsFetch start: min(115, 220) = 115
// - BidSignalsFetch end: max(635, 1495) = 1495
// - BidGeneration start: min(635, 1495) = 635
// - BidGeneration end: max(885, 1595) = 1595
TEST_F(AuctionMetricsRecorderTest, GenerateBidPhaseMetricsWithTwoRecords) {
  base::TimeTicks bid2_start_time = FastForwardTime(base::Milliseconds(115));

  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/std::nullopt,
       /*config_promises_latency=*/std::nullopt,
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_bidding_signals_latency=*/std::nullopt,
       /*deps_wait_start_time=*/FastForwardTime(base::Milliseconds(105)),
       /*generate_bid_start_time=*/FastForwardTime(base::Milliseconds(415)),
       /*generate_bid_finish_time=*/FastForwardTime(base::Milliseconds(250))});
  recorder().RecordGenerateBidDependencyLatencies(
      {/*code_ready_latency=*/std::nullopt,
       /*config_promises_latency=*/std::nullopt,
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_bidding_signals_latency=*/std::nullopt,
       /*deps_wait_start_time=*/bid2_start_time,
       /*generate_bid_start_time=*/FastForwardTime(base::Milliseconds(610)),
       /*generate_bid_finish_time=*/FastForwardTime(base::Milliseconds(100))});
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 115 becomes 110 with 10 ms linear bucketing.
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kBidSignalsFetchPhaseStartTimeInMillisName),
      110);
  // 1495 becomes 1490 with 10 ms linear bucketing.
  EXPECT_EQ(GetMetricValue(UkmEntry::kBidSignalsFetchPhaseEndTimeInMillisName),
            1490);
  // 635 becomes 630 with 10 ms linear bucketing.
  EXPECT_EQ(GetMetricValue(UkmEntry::kBidGenerationPhaseStartTimeInMillisName),
            630);
  // 1595 becomes 1590 with 10 ms linear bucketing.
  EXPECT_EQ(GetMetricValue(UkmEntry::kBidGenerationPhaseEndTimeInMillisName),
            1590);
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

TEST_F(AuctionMetricsRecorderTest,
       ScoreAdLatencyMetricsHaveNoValuesForNoGenerateBids) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_FALSE(HasMetric(UkmEntry::kMeanScoreAdLatencyInMillisName));
  EXPECT_FALSE(HasMetric(UkmEntry::kMaxScoreAdLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest, ScoreAdLatencyWithOneRecord) {
  recorder().RecordScoreAdLatency(base::Milliseconds(305));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 305 becomes 300 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanScoreAdLatencyInMillisName), 300);
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxScoreAdLatencyInMillisName), 300);
}

TEST_F(AuctionMetricsRecorderTest, ScoreAdLatencyWithTwoRecords) {
  recorder().RecordScoreAdLatency(base::Milliseconds(405));
  recorder().RecordScoreAdLatency(base::Milliseconds(605));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 505 becomes 500 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanScoreAdLatencyInMillisName), 500);
  // 605 becomes 600 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxScoreAdLatencyInMillisName), 600);
}

TEST_F(AuctionMetricsRecorderTest, ScoreAdLatencyIgnoresNegativeValues) {
  recorder().RecordScoreAdLatency(base::Milliseconds(405));
  recorder().RecordScoreAdLatency(base::Milliseconds(605));
  recorder().RecordScoreAdLatency(base::Milliseconds(-400));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 505 becomes 500 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanScoreAdLatencyInMillisName), 500);
  // 605 becomes 600 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxScoreAdLatencyInMillisName), 600);
}

TEST_F(AuctionMetricsRecorderTest,
       ScoreAdFlowLatencyMetricsHaveNoValuesForNoGenerateBids) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_FALSE(HasMetric(UkmEntry::kMeanScoreAdFlowLatencyInMillisName));
  EXPECT_FALSE(HasMetric(UkmEntry::kMaxScoreAdFlowLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest, ScoreAdFlowLatencyWithOneRecord) {
  recorder().RecordScoreAdFlowLatency(base::Milliseconds(405));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 405 becomes 400 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanScoreAdFlowLatencyInMillisName), 400);
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxScoreAdFlowLatencyInMillisName), 400);
}

TEST_F(AuctionMetricsRecorderTest, ScoreAdFlowLatencyWithTwoRecords) {
  recorder().RecordScoreAdFlowLatency(base::Milliseconds(505));
  recorder().RecordScoreAdFlowLatency(base::Milliseconds(705));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 605 becomes 600 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanScoreAdFlowLatencyInMillisName), 600);
  // 705 becomes 700 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxScoreAdFlowLatencyInMillisName), 700);
}

TEST_F(AuctionMetricsRecorderTest, ScoreAdFlowLatencyIgnoresNegativeValues) {
  recorder().RecordScoreAdFlowLatency(base::Milliseconds(505));
  recorder().RecordScoreAdFlowLatency(base::Milliseconds(705));
  recorder().RecordScoreAdFlowLatency(base::Milliseconds(-500));
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 605 becomes 600 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanScoreAdFlowLatencyInMillisName), 600);
  // 705 becomes 700 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxScoreAdFlowLatencyInMillisName), 700);
}

TEST_F(AuctionMetricsRecorderTest,
       ScoreAdDependencyLatencyMetricsHaveNoValuesForNoScoreAds) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_FALSE(HasMetric(UkmEntry::kMeanScoreAdCodeReadyLatencyInMillisName));
  EXPECT_FALSE(HasMetric(UkmEntry::kMaxScoreAdCodeReadyLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanScoreAdDirectFromSellerSignalsLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMaxScoreAdDirectFromSellerSignalsLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanScoreAdTrustedScoringSignalsLatencyInMillisName));
  EXPECT_FALSE(
      HasMetric(UkmEntry::kMaxScoreAdTrustedScoringSignalsLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest,
       ScoreAdDependencyLatencyMetricsHaveNoValuesForNullOptLatencies) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);
  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/std::nullopt,
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_scoring_signals_latency=*/std::nullopt,
       /*deps_wait_start_time=*/base::TimeTicks::Now(),
       /*score_ad_start_time=*/base::TimeTicks::Now(),
       /*score_ad_finish_time=*/base::TimeTicks::Now()});

  EXPECT_FALSE(HasMetric(UkmEntry::kMeanScoreAdCodeReadyLatencyInMillisName));
  EXPECT_FALSE(HasMetric(UkmEntry::kMaxScoreAdCodeReadyLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanScoreAdDirectFromSellerSignalsLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMaxScoreAdDirectFromSellerSignalsLatencyInMillisName));
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanScoreAdTrustedScoringSignalsLatencyInMillisName));
  EXPECT_FALSE(
      HasMetric(UkmEntry::kMaxScoreAdTrustedScoringSignalsLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest,
       ScoreAdDependencyLatencyMetricsWithOneRecord) {
  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(105),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(305),
       /*trusted_scoring_signals_latency=*/base::Milliseconds(405),
       /*deps_wait_start_time=*/base::TimeTicks::Now(),
       /*score_ad_start_time=*/base::TimeTicks::Now(),
       /*score_ad_finish_time=*/base::TimeTicks::Now()});
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 105 becomes 100 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanScoreAdCodeReadyLatencyInMillisName),
            100);
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxScoreAdCodeReadyLatencyInMillisName),
            100);

  // 305 becomes 300 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanScoreAdDirectFromSellerSignalsLatencyInMillisName),
      300);
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMaxScoreAdDirectFromSellerSignalsLatencyInMillisName),
      300);

  // 405 becomes 400 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMeanScoreAdTrustedScoringSignalsLatencyInMillisName),
            400);
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMaxScoreAdTrustedScoringSignalsLatencyInMillisName),
            400);
}

TEST_F(AuctionMetricsRecorderTest,
       ScoreAdDependencyLatencyMetricsWithTwoRecords) {
  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(105),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(305),
       /*trusted_scoring_signals_latency=*/base::Milliseconds(405),
       /*deps_wait_start_time=*/base::TimeTicks::Now(),
       /*score_ad_start_time=*/base::TimeTicks::Now(),
       /*score_ad_finish_time=*/base::TimeTicks::Now()});
  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(305),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(505),
       /*trusted_scoring_signals_latency=*/base::Milliseconds(605),
       /*deps_wait_start_time=*/base::TimeTicks::Now(),
       /*score_ad_start_time=*/base::TimeTicks::Now(),
       /*score_ad_finish_time=*/base::TimeTicks::Now()});
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 205 becomes 200 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanScoreAdCodeReadyLatencyInMillisName),
            200);
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxScoreAdCodeReadyLatencyInMillisName),
            300);

  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanScoreAdDirectFromSellerSignalsLatencyInMillisName),
      400);
  // 505 becomes 500 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMaxScoreAdDirectFromSellerSignalsLatencyInMillisName),
      500);

  // 505 becomes 500 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMeanScoreAdTrustedScoringSignalsLatencyInMillisName),
            500);
  // 605 becomes 600 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMaxScoreAdTrustedScoringSignalsLatencyInMillisName),
            600);
}

TEST_F(AuctionMetricsRecorderTest,
       ScoreAdDependencyLatencyMetricsIgnoresNegativeValues) {
  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(105),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(305),
       /*trusted_scoring_signals_latency=*/base::Milliseconds(405),
       /*deps_wait_start_time=*/base::TimeTicks::Now(),
       /*score_ad_start_time=*/base::TimeTicks::Now(),
       /*score_ad_finish_time=*/base::TimeTicks::Now()});
  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(305),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(505),
       /*trusted_scoring_signals_latency=*/base::Milliseconds(605),
       /*deps_wait_start_time=*/base::TimeTicks::Now(),
       /*score_ad_start_time=*/base::TimeTicks::Now(),
       /*score_ad_finish_time=*/base::TimeTicks::Now()});
  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(-200),
       /*direct_from_seller_signals_latency=*/base::Milliseconds(-400),
       /*trusted_scoring_signals_latency=*/base::Milliseconds(-500),
       /*deps_wait_start_time=*/base::TimeTicks::Now(),
       /*score_ad_start_time=*/base::TimeTicks::Now(),
       /*score_ad_finish_time=*/base::TimeTicks::Now()});
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 205 becomes 200 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMeanScoreAdCodeReadyLatencyInMillisName),
            200);
  // 305 becomes 300 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kMaxScoreAdCodeReadyLatencyInMillisName),
            300);

  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMeanScoreAdDirectFromSellerSignalsLatencyInMillisName),
      400);
  // 505 becomes 500 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::kMaxScoreAdDirectFromSellerSignalsLatencyInMillisName),
      500);

  // 505 becomes 500 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMeanScoreAdTrustedScoringSignalsLatencyInMillisName),
            500);
  // 605 becomes 600 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMaxScoreAdTrustedScoringSignalsLatencyInMillisName),
            600);
}

TEST_F(AuctionMetricsRecorderTest,
       ScoreAdCriticalPathMetricsHasNoValuesWhenNoneAreRecorded) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(GetMetricValue(UkmEntry::kNumScoreAdCodeReadyOnCriticalPathName),
            0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::kMeanScoreAdCodeReadyCriticalPathLatencyInMillisName));

  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumScoreAdDirectFromSellerSignalsOnCriticalPathName),
            0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::
          kMeanScoreAdDirectFromSellerSignalsCriticalPathLatencyInMillisName));

  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumScoreAdTrustedScoringSignalsOnCriticalPathName),
            0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::
          kMeanScoreAdTrustedScoringSignalsCriticalPathLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest,
       ScoreAdCriticalPathMetricsRecordedManyTimes) {
  using auction_worklet::mojom::ScoreAdDependencyLatencies;
  for (int i = 0; i < 12; ++i) {
    recorder().RecordScoreAdDependencyLatencies(
        {/*code_ready_latency=*/base::Milliseconds(305),
         /*direct_from_seller_signals_latency=*/base::Milliseconds(50),
         /*trusted_scoring_signals_latency=*/std::nullopt,
         /*deps_wait_start_time=*/base::TimeTicks::Now(),
         /*score_ad_start_time=*/base::TimeTicks::Now(),
         /*score_ad_finish_time=*/base::TimeTicks::Now()});
  }
  for (int i = 0; i < 18; ++i) {
    recorder().RecordScoreAdDependencyLatencies(
        {/*code_ready_latency=*/base::Milliseconds(50),
         /*direct_from_seller_signals_latency=*/base::Milliseconds(505),
         /*trusted_scoring_signals_latency=*/base::Milliseconds(100),
         /*deps_wait_start_time=*/base::TimeTicks::Now(),
         /*score_ad_start_time=*/base::TimeTicks::Now(),
         /*score_ad_finish_time=*/base::TimeTicks::Now()});
  }
  for (int i = 0; i < 21; ++i) {
    recorder().RecordScoreAdDependencyLatencies(
        {/*code_ready_latency=*/std::nullopt,
         /*direct_from_seller_signals_latency=*/base::Milliseconds(50),
         /*trusted_scoring_signals_latency=*/base::Milliseconds(605),
         /*deps_wait_start_time=*/base::TimeTicks::Now(),
         /*score_ad_start_time=*/base::TimeTicks::Now(),
         /*score_ad_finish_time=*/base::TimeTicks::Now()});
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 12 becomes 11 because of bucketing
  EXPECT_EQ(GetMetricValue(UkmEntry::kNumScoreAdCodeReadyOnCriticalPathName),
            11);
  // 255 becomes 200 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMeanScoreAdCodeReadyCriticalPathLatencyInMillisName),
            200);

  // 18 becomes 17 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumScoreAdDirectFromSellerSignalsOnCriticalPathName),
            17);
  // 405 becomes 400 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::
              kMeanScoreAdDirectFromSellerSignalsCriticalPathLatencyInMillisName),
      400);

  // 21 becomes 19 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumScoreAdTrustedScoringSignalsOnCriticalPathName),
            19);
  // 555 becomes 500 because of bucketing
  EXPECT_EQ(
      GetMetricValue(
          UkmEntry::
              kMeanScoreAdTrustedScoringSignalsCriticalPathLatencyInMillisName),
      500);
}

TEST_F(AuctionMetricsRecorderTest, ScoreAdCriticalPathMetricsComputeMean) {
  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(205),
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_scoring_signals_latency=*/std::nullopt,
       /*deps_wait_start_time=*/base::TimeTicks::Now(),
       /*score_ad_start_time=*/base::TimeTicks::Now(),
       /*score_ad_finish_time=*/base::TimeTicks::Now()});
  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(405),
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_scoring_signals_latency=*/std::nullopt,
       /*deps_wait_start_time=*/base::TimeTicks::Now(),
       /*score_ad_start_time=*/base::TimeTicks::Now(),
       /*score_ad_finish_time=*/base::TimeTicks::Now()});
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(GetMetricValue(UkmEntry::kNumScoreAdCodeReadyOnCriticalPathName),
            2);
  // 305 becomes 300 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMeanScoreAdCodeReadyCriticalPathLatencyInMillisName),
            300);

  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumScoreAdDirectFromSellerSignalsOnCriticalPathName),
            0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::
          kMeanScoreAdDirectFromSellerSignalsCriticalPathLatencyInMillisName));

  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumScoreAdTrustedScoringSignalsOnCriticalPathName),
            0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::
          kMeanScoreAdTrustedScoringSignalsCriticalPathLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest,
       ScoreAdCriticalPathMetricsIgnoreNegativeValues) {
  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(205),
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_scoring_signals_latency=*/std::nullopt,
       /*deps_wait_start_time=*/base::TimeTicks::Now(),
       /*score_ad_start_time=*/base::TimeTicks::Now(),
       /*score_ad_finish_time=*/base::TimeTicks::Now()});
  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(405),
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_scoring_signals_latency=*/std::nullopt,
       /*deps_wait_start_time=*/base::TimeTicks::Now(),
       /*score_ad_start_time=*/base::TimeTicks::Now(),
       /*score_ad_finish_time=*/base::TimeTicks::Now()});
  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/base::Milliseconds(-100),
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_scoring_signals_latency=*/std::nullopt,
       /*deps_wait_start_time=*/base::TimeTicks::Now(),
       /*score_ad_start_time=*/base::TimeTicks::Now(),
       /*score_ad_finish_time=*/base::TimeTicks::Now()});
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_EQ(GetMetricValue(UkmEntry::kNumScoreAdCodeReadyOnCriticalPathName),
            2);
  // 305 becomes 300 because of bucketing
  EXPECT_EQ(GetMetricValue(
                UkmEntry::kMeanScoreAdCodeReadyCriticalPathLatencyInMillisName),
            300);

  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumScoreAdDirectFromSellerSignalsOnCriticalPathName),
            0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::
          kMeanScoreAdDirectFromSellerSignalsCriticalPathLatencyInMillisName));

  EXPECT_EQ(GetMetricValue(
                UkmEntry::kNumScoreAdTrustedScoringSignalsOnCriticalPathName),
            0);
  EXPECT_FALSE(HasMetric(
      UkmEntry::
          kMeanScoreAdTrustedScoringSignalsCriticalPathLatencyInMillisName));
}

TEST_F(AuctionMetricsRecorderTest,
       ScoreAdPhaseMetricsHaveNoValuesForNoScoreAds) {
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  EXPECT_FALSE(
      HasMetric(UkmEntry::kScoreSignalsFetchPhaseStartTimeInMillisName));
  EXPECT_FALSE(HasMetric(UkmEntry::kScoreSignalsFetchPhaseEndTimeInMillisName));
  EXPECT_FALSE(HasMetric(UkmEntry::kScoringPhaseStartTimeInMillisName));
  EXPECT_FALSE(HasMetric(UkmEntry::kScoringPhaseEndTimeInMillisName));
}

// Times for this test:
// - ScoreAd called at 205
// - Signals fetched by (205 + 410) = 615
// - ScoreAd complete by (615 + 190) = 805
TEST_F(AuctionMetricsRecorderTest, ScoreAdPhaseMetricsWithOneRecord) {
  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/std::nullopt,
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_scoring_signals_latency=*/std::nullopt,
       /*deps_wait_start_time=*/FastForwardTime(base::Milliseconds(205)),
       /*score_ad_start_time=*/FastForwardTime(base::Milliseconds(410)),
       /*score_ad_finish_time=*/FastForwardTime(base::Milliseconds(190))});
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 205 becomes 200 with 10 ms linear bucketing.
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kScoreSignalsFetchPhaseStartTimeInMillisName),
      200);
  // 615 becomes 610 with 10 ms linear bucketing.
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kScoreSignalsFetchPhaseEndTimeInMillisName),
      610);
  // 615 becomes 610 with 10 ms linear bucketing.
  EXPECT_EQ(GetMetricValue(UkmEntry::kScoringPhaseStartTimeInMillisName), 610);
  // 805 becomes 800 with 10 ms linear bucketing.
  EXPECT_EQ(GetMetricValue(UkmEntry::kScoringPhaseEndTimeInMillisName), 800);
}

// Times for this test:
// - ScoreAd[bid2] called at 115
// - ScoreAd[bid1] called at (115 + 105) = 220
// - Signals fetched[bid1] by (220 + 415) = 635
// - ScoreAd complete[bid1] by (635 + 250) = 885
// - Signals fetched[bid2] by (885 + 610) = 1495
// - ScoreAd complete[bid2] by (1495 + 100) = 1595
//
// Phases:
// - ScoreSignalsFetch start: min(115, 220) = 115
// - ScoreSignalsFetch end: max(635, 1495) = 1495
// - Scoring start: min(635, 1495) = 635
// - Scoring end: max(885, 1595) = 1595
TEST_F(AuctionMetricsRecorderTest, ScoreAdPhaseMetricsWithTwoRecords) {
  base::TimeTicks bid2_start_time = FastForwardTime(base::Milliseconds(115));

  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/std::nullopt,
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_scoring_signals_latency=*/std::nullopt,
       /*deps_wait_start_time=*/FastForwardTime(base::Milliseconds(105)),
       /*score_ad_start_time=*/FastForwardTime(base::Milliseconds(415)),
       /*score_ad_finish_time=*/FastForwardTime(base::Milliseconds(250))});
  recorder().RecordScoreAdDependencyLatencies(
      {/*code_ready_latency=*/std::nullopt,
       /*direct_from_seller_signals_latency=*/std::nullopt,
       /*trusted_scoring_signals_latency=*/std::nullopt,
       /*deps_wait_start_time=*/bid2_start_time,
       /*score_ad_start_time=*/FastForwardTime(base::Milliseconds(610)),
       /*score_ad_finish_time=*/FastForwardTime(base::Milliseconds(100))});
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 115 becomes 110 with 10 ms linear bucketing.
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kScoreSignalsFetchPhaseStartTimeInMillisName),
      110);
  // 1495 becomes 1490 with 10 ms linear bucketing.
  EXPECT_EQ(
      GetMetricValue(UkmEntry::kScoreSignalsFetchPhaseEndTimeInMillisName),
      1490);
  // 635 becomes 630 with 10 ms linear bucketing.
  EXPECT_EQ(GetMetricValue(UkmEntry::kScoringPhaseStartTimeInMillisName), 630);
  // 1595 becomes 1590 with 10 ms linear bucketing.
  EXPECT_EQ(GetMetricValue(UkmEntry::kScoringPhaseEndTimeInMillisName), 1590);
}

TEST_F(AuctionMetricsRecorderTest, MultiBidCount) {
  recorder().RecordNumberOfBidsFromGenerateBid(4, 10);
  recorder().RecordNumberOfBidsFromGenerateBid(2, 2);
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  histogram_tester().ExpectBucketCount(
      /*name=*/"Ads.InterestGroup.Auction.NumBidsGeneratedAtOnce",
      /*sample=*/10, /*expected_count=*/1);

  histogram_tester().ExpectBucketCount(
      /*name=*/"Ads.InterestGroup.Auction.NumBidsGeneratedAtOnce",
      /*sample=*/2, /*expected_count=*/1);

  histogram_tester().ExpectTotalCount(
      /*name=*/"Ads.InterestGroup.Auction.NumBidsGeneratedAtOnce", 2);

  // 6/12 are k-Anon
  histogram_tester().ExpectUniqueSample(
      /*name=*/"Ads.InterestGroup.Auction.PercentBidsKAnon",
      /*sample=*/50, /*expected_bucket_count=*/1);
}

}  // namespace
}  // namespace content
