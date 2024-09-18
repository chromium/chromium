// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_and_auction_serializer.h"

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/test_interest_group_builder.h"

namespace content {

namespace {

const size_t kEncryptionOverhead = 56;

constexpr char kOriginStringA[] = "https://a.test";
constexpr char kOriginStringB[] = "https://b.test";
constexpr char kOriginStringC[] = "https://c.test";
constexpr char kOriginStringD[] = "https://d.test";

// Bidder overhead is 1 byte for tag/length of buyer, 14 bytes for buyer, 1 byte
// for tag of serialized data, 1 byte for length of serialized data (length <
// 256).
const size_t kBidderOverhead = 1 + 14 + 1 + 1;

StorageInterestGroup MakeInterestGroup(blink::InterestGroup interest_group) {
  // Create fake previous wins. The time of these wins is ignored, since the
  // InterestGroupManager attaches the current time when logging a win.
  std::vector<blink::mojom::PreviousWinPtr> previous_wins;
  // Log a time that's before now, so that any new entry will have the largest
  // time.
  base::Time the_past = base::Time::Now() - base::Milliseconds(1);
  previous_wins.push_back(
      blink::mojom::PreviousWin::New(the_past, R"({"adRenderId": 0})"));
  previous_wins.push_back(
      blink::mojom::PreviousWin::New(the_past, R"({"adRenderId": 1})"));
  previous_wins.push_back(
      blink::mojom::PreviousWin::New(the_past, R"({"adRenderId": 2})"));

  StorageInterestGroup storage_group;
  storage_group.interest_group = std::move(interest_group);
  storage_group.bidding_browser_signals =
      blink::mojom::BiddingBrowserSignals::New(3, 5, std::move(previous_wins),
                                               false);
  storage_group.joining_origin = storage_group.interest_group.owner;
  return storage_group;
}

scoped_refptr<StorageInterestGroups> CreateInterestGroups(url::Origin owner) {
  std::vector<blink::InterestGroup::Ad> ads;
  for (int i = 0; i < 100; i++) {
    ads.emplace_back(
        owner.GetURL().Resolve(base::StringPrintf("/%03i.html", i)), "metadata",
        /*size_group=*/std::nullopt,
        /*buyer_reporting_id=*/std::nullopt,
        /*buyer_and_seller_reporting_id=*/std::nullopt,
        /*selectable_buyer_and_seller_reporting_ids=*/std::nullopt,
        /*ad_render_id=*/base::StringPrintf("%03i", i));
  }
  std::vector<StorageInterestGroup> groups;
  for (int i = 0; i < 100; i++) {
    groups.emplace_back(MakeInterestGroup(
        blink::TestInterestGroupBuilder(owner, base::StringPrintf("%03i", i))
            .SetBiddingUrl(owner.GetURL().Resolve("/bidding_script.js"))
            .SetPriority(i)  // Set a priority for deterministic ordering.
            .SetAds(ads)
            .Build()));
  }
  return base::MakeRefCounted<StorageInterestGroups>(std::move(groups));
}

class BiddingAndAuctionSerializerTest : public testing::Test {
 public:
  void AddGroupsToSerializer(BiddingAndAuctionSerializer& serializer) {
    for (const auto& owner : {kOriginA, kOriginB, kOriginC, kOriginD}) {
      serializer.AddGroups(owner, CreateInterestGroups(owner));
    }
  }

 protected:
  const GURL kUrlA = GURL(kOriginStringA);
  const url::Origin kOriginA = url::Origin::Create(kUrlA);
  const GURL kUrlB = GURL(kOriginStringB);
  const url::Origin kOriginB = url::Origin::Create(kUrlB);
  const GURL kUrlC = GURL(kOriginStringC);
  const url::Origin kOriginC = url::Origin::Create(kUrlC);
  const GURL kUrlD = GURL(kOriginStringD);
  const url::Origin kOriginD = url::Origin::Create(kUrlD);
};

TEST_F(BiddingAndAuctionSerializerTest, SerializeWithDefaultConfig) {
  base::HistogramTester histogram_tester;

  BiddingAndAuctionSerializer serializer;
  serializer.SetPublisher("foo");
  serializer.SetGenerationId(
      base::Uuid::ParseCaseInsensitive("00000000-0000-0000-0000-000000000000"));
  serializer.SetConfig(blink::mojom::AuctionDataConfig::New());
  serializer.SetDebugReportInLockout(false);

  AddGroupsToSerializer(serializer);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), 5 * 1024 - kEncryptionOverhead);
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 4);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumGroups", 400, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.RelativeCompressedSize", 1, 1);
}

TEST_F(BiddingAndAuctionSerializerTest, SerializeWithLargeRequestSize) {
  base::HistogramTester histogram_tester;

  const size_t kRequestSize = 4000;
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = kRequestSize;

  BiddingAndAuctionSerializer serializer;
  serializer.SetPublisher("foo");
  serializer.SetGenerationId(
      base::Uuid::ParseCaseInsensitive("00000000-0000-0000-0000-000000000000"));
  serializer.SetTimestamp(base::Time::FromMillisecondsSinceUnixEpoch(0));
  serializer.SetConfig(std::move(config));
  serializer.SetDebugReportInLockout(false);

  AddGroupsToSerializer(serializer);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), kRequestSize - kEncryptionOverhead);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 0, 4);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumGroups", 400, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.RelativeCompressedSize", 1, 1);
}

TEST_F(BiddingAndAuctionSerializerTest, SerializeWithSmallRequestSize) {
  base::HistogramTester histogram_tester;

  const size_t kRequestSize = 2000;
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = kRequestSize;

  BiddingAndAuctionSerializer serializer;
  serializer.SetPublisher("foo");
  serializer.SetGenerationId(
      base::Uuid::ParseCaseInsensitive("00000000-0000-0000-0000-000000000000"));
  serializer.SetTimestamp(base::Time::FromMillisecondsSinceUnixEpoch(0));
  serializer.SetConfig(std::move(config));
  serializer.SetDebugReportInLockout(false);

  AddGroupsToSerializer(serializer);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), kRequestSize - kEncryptionOverhead);
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 4);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumGroups", 107, 1);
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.ServerAuction.Request.RelativeCompressedSize", 1);
}

TEST_F(BiddingAndAuctionSerializerTest, SerializeWithTooSmallRequestSize) {
  base::HistogramTester histogram_tester;

  const size_t kRequestSize = 220;
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = kRequestSize;

  BiddingAndAuctionSerializer serializer;
  serializer.SetPublisher("foo");
  serializer.SetGenerationId(
      base::Uuid::ParseCaseInsensitive("00000000-0000-0000-0000-000000000000"));
  serializer.SetTimestamp(base::Time::FromMillisecondsSinceUnixEpoch(0));
  serializer.SetConfig(std::move(config));
  serializer.SetDebugReportInLockout(false);

  AddGroupsToSerializer(serializer);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), 0u);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 2, 1);
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.ServerAuction.Request.NumGroups", 0);
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.ServerAuction.Request.RelativeCompressedSize", 0);
}

TEST_F(BiddingAndAuctionSerializerTest, SerializeWithPerOwnerSize) {
  base::HistogramTester histogram_tester;

  const size_t kRequestSize = 3600;
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = kRequestSize;

  config->per_buyer_configs[kOriginA] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/1200);
  config->per_buyer_configs[kOriginB] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/1200);
  config->per_buyer_configs[kOriginC] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/1200);
  config->per_buyer_configs[kOriginD] =
      blink::mojom::AuctionDataBuyerConfig::New();

  BiddingAndAuctionSerializer serializer;
  serializer.SetPublisher("foo");
  serializer.SetGenerationId(
      base::Uuid::ParseCaseInsensitive("00000000-0000-0000-0000-000000000000"));
  serializer.SetTimestamp(base::Time::FromMillisecondsSinceUnixEpoch(0));
  serializer.SetConfig(std::move(config));
  serializer.SetDebugReportInLockout(false);

  AddGroupsToSerializer(serializer);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), kRequestSize - kEncryptionOverhead);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 0, 4);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumGroups", 400, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.RelativeCompressedSize", 1, 1);
}

TEST_F(BiddingAndAuctionSerializerTest,
       SerializeWithPerOwnerSizeBiggerThanRequestSize) {
  base::HistogramTester histogram_tester;

  const size_t kRequestSize = 2000;
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = kRequestSize;

  config->per_buyer_configs[kOriginA] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/4000);
  config->per_buyer_configs[kOriginB] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/4000);
  config->per_buyer_configs[kOriginC] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/4000);
  config->per_buyer_configs[kOriginD] =
      blink::mojom::AuctionDataBuyerConfig::New();

  BiddingAndAuctionSerializer serializer;
  serializer.SetPublisher("foo");
  serializer.SetGenerationId(
      base::Uuid::ParseCaseInsensitive("00000000-0000-0000-0000-000000000000"));
  serializer.SetTimestamp(base::Time::FromMillisecondsSinceUnixEpoch(0));
  serializer.SetConfig(std::move(config));
  serializer.SetDebugReportInLockout(false);

  AddGroupsToSerializer(serializer);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), kRequestSize - kEncryptionOverhead);

  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 3);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumGroups", 192, 1);
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.ServerAuction.Request.RelativeCompressedSize", 1);
}

TEST_F(BiddingAndAuctionSerializerTest, SerializeWithPerOwnerSizeExpands) {
  base::HistogramTester histogram_tester;

  const size_t kRequestSize = 6000;
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = kRequestSize;

  config->per_buyer_configs[kOriginA] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginB] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginC] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginD] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);

  BiddingAndAuctionSerializer serializer;
  serializer.SetPublisher("foo");
  serializer.SetGenerationId(
      base::Uuid::ParseCaseInsensitive("00000000-0000-0000-0000-000000000000"));
  serializer.SetTimestamp(base::Time::FromMillisecondsSinceUnixEpoch(0));
  serializer.SetConfig(std::move(config));
  serializer.SetDebugReportInLockout(false);

  AddGroupsToSerializer(serializer);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), kRequestSize - kEncryptionOverhead);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 0, 4);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumGroups", 400, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.RelativeCompressedSize", 1, 1);
}

TEST_F(BiddingAndAuctionSerializerTest, SerializeWithPerOwnerSizeShrinks) {
  base::HistogramTester histogram_tester;

  const size_t kRequestSize = 2000;
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = kRequestSize;

  config->per_buyer_configs[kOriginA] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/1000);
  config->per_buyer_configs[kOriginB] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/10000);
  config->per_buyer_configs[kOriginC] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/1000);
  config->per_buyer_configs[kOriginD] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/10000);

  BiddingAndAuctionSerializer serializer;
  serializer.SetPublisher("foo");
  serializer.SetGenerationId(
      base::Uuid::ParseCaseInsensitive("00000000-0000-0000-0000-000000000000"));
  serializer.SetTimestamp(base::Time::FromMillisecondsSinceUnixEpoch(0));
  serializer.SetConfig(std::move(config));
  serializer.SetDebugReportInLockout(false);

  AddGroupsToSerializer(serializer);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), kRequestSize - kEncryptionOverhead);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 0, 2);
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 4);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumGroups", 200, 1);
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.ServerAuction.Request.RelativeCompressedSize", 1);
}

TEST_F(BiddingAndAuctionSerializerTest, SerializeWithFixedSizeGroups) {
  base::HistogramTester histogram_tester;

  const size_t kRequestSize = 3000;
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = kRequestSize;

  config->per_buyer_configs[kOriginA] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginB] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginC] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginD] =
      blink::mojom::AuctionDataBuyerConfig::New();

  BiddingAndAuctionSerializer serializer;
  serializer.SetPublisher("foo");
  serializer.SetGenerationId(
      base::Uuid::ParseCaseInsensitive("00000000-0000-0000-0000-000000000000"));
  serializer.SetTimestamp(base::Time::FromMillisecondsSinceUnixEpoch(0));
  serializer.SetConfig(std::move(config));
  serializer.SetDebugReportInLockout(false);

  AddGroupsToSerializer(serializer);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), kRequestSize - kEncryptionOverhead);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 3, 3);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 4);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumGroups", 95, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.RelativeCompressedSize", 1, 1);
}

// Test that the encrypted request still has the full size even when the
// specified buyers are not on the device.
TEST_F(BiddingAndAuctionSerializerTest, SerializeWithNoGroupsSetBuyersFixed) {
  const size_t kRequestSize = 3000;
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = kRequestSize;

  config->per_buyer_configs[kOriginA] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginB] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginC] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginD] =
      blink::mojom::AuctionDataBuyerConfig::New();

  BiddingAndAuctionSerializer serializer;
  serializer.SetPublisher("foo");
  serializer.SetGenerationId(
      base::Uuid::ParseCaseInsensitive("00000000-0000-0000-0000-000000000000"));
  serializer.SetTimestamp(base::Time::FromMillisecondsSinceUnixEpoch(0));
  serializer.SetConfig(std::move(config));
  serializer.SetDebugReportInLockout(false);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), kRequestSize - kEncryptionOverhead);
}

// Test that the encrypted request still has the full size even when the
// specified buyers are not on the device.
TEST_F(BiddingAndAuctionSerializerTest,
       SerializeWithNoGroupsSetBuyersProportional) {
  const size_t kRequestSize = 3000;
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = kRequestSize;

  config->per_buyer_configs[kOriginA] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginB] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginC] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginD] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);

  BiddingAndAuctionSerializer serializer;
  serializer.SetPublisher("foo");
  serializer.SetGenerationId(
      base::Uuid::ParseCaseInsensitive("00000000-0000-0000-0000-000000000000"));
  serializer.SetTimestamp(base::Time::FromMillisecondsSinceUnixEpoch(0));
  serializer.SetConfig(std::move(config));
  serializer.SetDebugReportInLockout(false);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), kRequestSize - kEncryptionOverhead);
}

class TargetSizeEstimatorTest : public testing::Test {
 protected:
  const GURL kUrlA = GURL(kOriginStringA);
  const url::Origin kOriginA = url::Origin::Create(kUrlA);
  const GURL kUrlB = GURL(kOriginStringB);
  const url::Origin kOriginB = url::Origin::Create(kUrlB);
  const GURL kUrlC = GURL(kOriginStringC);
  const url::Origin kOriginC = url::Origin::Create(kUrlC);
  const GURL kUrlD = GURL(kOriginStringD);
  const url::Origin kOriginD = url::Origin::Create(kUrlD);
};

TEST_F(TargetSizeEstimatorTest, TotalSizeExceedsSizeT) {
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = 100;
  config->per_buyer_configs[kOriginA] =
      blink::mojom::AuctionDataBuyerConfig::New(
          /*size=*/std::numeric_limits<size_t>::max());
  config->per_buyer_configs[kOriginB] =
      blink::mojom::AuctionDataBuyerConfig::New(
          /*size=*/std::numeric_limits<size_t>::max());

  BiddingAndAuctionSerializer::TargetSizeEstimator estimator(0, &*config);
  // Values passed to UpdatePerBuyerMaxSize do not include overhead.
  estimator.UpdatePerBuyerMaxSize(kOriginA, 100 - kBidderOverhead);
  estimator.UpdatePerBuyerMaxSize(kOriginB, 100 - kBidderOverhead);
  // Value returned from EstimateTargetSize do not include overhead.
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginA, 0u), 50 - kBidderOverhead);
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginB, 50u), 50 - kBidderOverhead);
}

TEST_F(TargetSizeEstimatorTest, LargeSizeBeforeGroups) {
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = std::numeric_limits<size_t>::max();
  config->per_buyer_configs[kOriginA] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginB] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);

  BiddingAndAuctionSerializer::TargetSizeEstimator estimator(
      std::numeric_limits<size_t>::max(), &*config);
  estimator.UpdatePerBuyerMaxSize(kOriginA, 100);
  estimator.UpdatePerBuyerMaxSize(kOriginB, 100);
  // No space left for groups.
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginA, 0), 0u);
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginB, 0), 0u);
}

TEST_F(TargetSizeEstimatorTest, FixedSizeGroups) {
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = 300;
  config->per_buyer_configs[kOriginA] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginB] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/100);
  config->per_buyer_configs[kOriginC] =
      blink::mojom::AuctionDataBuyerConfig::New();
  BiddingAndAuctionSerializer::TargetSizeEstimator estimator(0, &*config);
  // Values passed to UpdatePerBuyerMaxSize do not include overhead.
  estimator.UpdatePerBuyerMaxSize(kOriginA, 100 - kBidderOverhead);
  estimator.UpdatePerBuyerMaxSize(kOriginB, 100 - kBidderOverhead);
  estimator.UpdatePerBuyerMaxSize(kOriginC, 100 - kBidderOverhead);
  // Value returned from EstimateTargetSize do not include overhead.
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginA, 0), 100 - kBidderOverhead);
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginB, 100), 100 - kBidderOverhead);
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginC, 200), 100 - kBidderOverhead);
}

TEST_F(TargetSizeEstimatorTest, FixedSizeGroupsShrink) {
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = 300;
  config->per_buyer_configs[kOriginA] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/50);
  config->per_buyer_configs[kOriginB] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/50);
  config->per_buyer_configs[kOriginC] =
      blink::mojom::AuctionDataBuyerConfig::New();
  config->per_buyer_configs[kOriginD] =
      blink::mojom::AuctionDataBuyerConfig::New();
  BiddingAndAuctionSerializer::TargetSizeEstimator estimator(0, &*config);
  // Values passed to UpdatePerBuyerMaxSize do not include overhead.
  estimator.UpdatePerBuyerMaxSize(kOriginA, 100 - kBidderOverhead);
  estimator.UpdatePerBuyerMaxSize(kOriginB, 100 - kBidderOverhead);
  estimator.UpdatePerBuyerMaxSize(kOriginC, 150 - kBidderOverhead);
  estimator.UpdatePerBuyerMaxSize(kOriginD, 50 - kBidderOverhead);
  // Value returned from EstimateTargetSize do not include overhead.
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginA, 0), 50 - kBidderOverhead);
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginB, 50), 50 - kBidderOverhead);
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginC, 100), 150 - kBidderOverhead);
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginD, 250), 50 - kBidderOverhead);
}

// Specifically designed groups where each iteration in
// TargetSizeEstimator::UpdateUnsizedGroupSizes only removes 1 buyer.
TEST_F(TargetSizeEstimatorTest, EqualWorstCase) {
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = 429;  // fits all groups exactly
  config->per_buyer_configs[kOriginA] =
      blink::mojom::AuctionDataBuyerConfig::New();
  config->per_buyer_configs[kOriginB] =
      blink::mojom::AuctionDataBuyerConfig::New();
  config->per_buyer_configs[kOriginC] =
      blink::mojom::AuctionDataBuyerConfig::New();
  config->per_buyer_configs[kOriginD] =
      blink::mojom::AuctionDataBuyerConfig::New();
  BiddingAndAuctionSerializer::TargetSizeEstimator estimator(0, &*config);
  // Values passed to UpdatePerBuyerMaxSize do not include overhead.
  estimator.UpdatePerBuyerMaxSize(kOriginA, 100 - kBidderOverhead);
  estimator.UpdatePerBuyerMaxSize(kOriginB, 108 - kBidderOverhead);
  estimator.UpdatePerBuyerMaxSize(kOriginC, 110 - kBidderOverhead);
  estimator.UpdatePerBuyerMaxSize(kOriginD, 111 - kBidderOverhead);
  // Value returned from EstimateTargetSize do not include overhead.
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginA, 0), 100 - kBidderOverhead);
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginB, 100), 108 - kBidderOverhead);
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginC, 208), 110 - kBidderOverhead);
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginD, 318), 111 - kBidderOverhead);
}

// Specifically designed groups where each iteration in
// TargetSizeEstimator::UpdateSizedGroupSizes only removes 1 buyer. Note this is
// the same as the previous test only for sized groups, as equal size is a
// special case of proportional.
TEST_F(TargetSizeEstimatorTest, ProportionalWorstCase) {
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = 429;  // fits all groups exactly
  config->per_buyer_configs[kOriginA] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/1000);
  config->per_buyer_configs[kOriginB] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/1000);
  config->per_buyer_configs[kOriginC] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/1000);
  config->per_buyer_configs[kOriginD] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/1000);
  BiddingAndAuctionSerializer::TargetSizeEstimator estimator(0, &*config);
  // Values passed to UpdatePerBuyerMaxSize do not include overhead.
  estimator.UpdatePerBuyerMaxSize(kOriginA, 100 - kBidderOverhead);
  estimator.UpdatePerBuyerMaxSize(kOriginB, 108 - kBidderOverhead);
  estimator.UpdatePerBuyerMaxSize(kOriginC, 110 - kBidderOverhead);
  estimator.UpdatePerBuyerMaxSize(kOriginD, 111 - kBidderOverhead);
  // Value returned from EstimateTargetSize do not include overhead.
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginA, 0), 100 - kBidderOverhead);
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginB, 100), 108 - kBidderOverhead);
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginC, 208), 110 - kBidderOverhead);
  EXPECT_EQ(estimator.EstimateTargetSize(kOriginD, 318), 111 - kBidderOverhead);
}

}  // namespace
}  // namespace content
