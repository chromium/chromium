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

using UkmEntry = ukm::builders::AdsInterestGroup_AuctionLatency;

class AuctionMetricsRecorderTest : public testing::Test {
 public:
  AuctionMetricsRecorderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        source_id_(ukm::AssignNewSourceId()),
        recorder_(source_id_) {}

  void FastForwardTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  absl::optional<int64_t> GetMetricValue(std::string metric_name) {
    std::vector<ukm::TestUkmRecorder::HumanReadableUkmEntry> entries =
        ukm_recorder_.GetEntries(
            ukm::builders::AdsInterestGroup_AuctionLatency::kEntryName,
            {metric_name});
    EXPECT_THAT(entries, testing::SizeIs(1));
    if (entries.size() != 1) {
      return absl::nullopt;
    }

    EXPECT_EQ(entries.at(0).source_id, source_id_);
    if (entries.at(0).source_id != source_id_) {
      return absl::nullopt;
    }

    EXPECT_TRUE(entries.at(0).metrics.contains(metric_name));
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
  EXPECT_THAT(GetMetricValue(UkmEntry::kNumInterestGroupsWithNoBidsName),
              testing::Eq(13));
}

TEST_F(AuctionMetricsRecorderTest, NumInterestGroupsWithOnlyNonKAnonBid) {
  for (size_t i = 0; i < 16; ++i) {
    recorder().RecordInterestGroupWithOnlyNonKAnonBid();
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 16 becomes 15 because of bucketing
  EXPECT_THAT(
      GetMetricValue(UkmEntry::kNumInterestGroupsWithOnlyNonKAnonBidName),
      testing::Eq(15));
}

TEST_F(AuctionMetricsRecorderTest,
       NumInterestGroupsWithSameBidForKAnonAndNonKAnon) {
  for (size_t i = 0; i < 20; ++i) {
    recorder().RecordInterestGroupWithSameBidForKAnonAndNonKAnon();
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 20 becomes 19 because of bucketing
  EXPECT_THAT(
      GetMetricValue(
          UkmEntry::kNumInterestGroupsWithSameBidForKAnonAndNonKAnonName),
      testing::Eq(19));
}

TEST_F(AuctionMetricsRecorderTest,
       NumInterestGroupsWithSeparateBidsForKAnonAndNonKAnon) {
  for (size_t i = 0; i < 18; ++i) {
    recorder().RecordInterestGroupWithSeparateBidsForKAnonAndNonKAnon();
  }
  recorder().OnAuctionEnd(AuctionResult::kSuccess);

  // 18 becomes 17 because of bucketing
  EXPECT_THAT(
      GetMetricValue(
          UkmEntry::kNumInterestGroupsWithSeparateBidsForKAnonAndNonKAnonName),
      testing::Eq(17));
}

}  // namespace
}  // namespace content
