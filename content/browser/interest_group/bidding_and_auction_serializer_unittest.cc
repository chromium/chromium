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

StorageInterestGroup MakeInterestGroup(blink::InterestGroup interest_group) {
  // Create fake previous wins. The time of these wins is ignored, since the
  // InterestGroupManager attaches the current time when logging a win.
  std::vector<auction_worklet::mojom::PreviousWinPtr> previous_wins;
  // Log a time that's before now, so that any new entry will have the largest
  // time.
  base::Time the_past = base::Time::Now() - base::Milliseconds(1);
  previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
      the_past, R"({"adRenderId": 0})"));
  previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
      the_past, R"({"adRenderId": 1})"));
  previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
      the_past, R"({"adRenderId": 2})"));

  StorageInterestGroup storage_group;
  storage_group.interest_group = std::move(interest_group);
  storage_group.bidding_browser_signals =
      auction_worklet::mojom::BiddingBrowserSignals::New(
          3, 5, std::move(previous_wins), false);
  storage_group.joining_origin = storage_group.interest_group.owner;
  return storage_group;
}

scoped_refptr<StorageInterestGroups> CreateInterestGroups(url::Origin owner) {
  std::vector<blink::InterestGroup::Ad> ads;
  for (int i = 0; i < 100; i++) {
    ads.emplace_back(owner.GetURL().Resolve(base::StringPrintf("/%i.html", i)),
                     "metadata",
                     /*size_group=*/std::nullopt,
                     /*buyer_reporting_id=*/std::nullopt,
                     /*buyer_and_seller_reporting_id=*/std::nullopt,
                     /*ad_render_id=*/base::NumberToString(i));
  }
  std::vector<StorageInterestGroup> groups;
  for (int i = 0; i < 100; i++) {
    groups.emplace_back(MakeInterestGroup(
        blink::TestInterestGroupBuilder(owner, base::NumberToString(i))
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

  AddGroupsToSerializer(serializer);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), 4096u - kEncryptionOverhead);
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 0);
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
  serializer.SetConfig(std::move(config));

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
  serializer.SetConfig(std::move(config));

  AddGroupsToSerializer(serializer);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), kRequestSize - kEncryptionOverhead);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 4, 4);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumGroups", 154, 1);
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.ServerAuction.Request.RelativeCompressedSize", 1);
}

TEST_F(BiddingAndAuctionSerializerTest, SerializeWithTooSmallRequestSize) {
  base::HistogramTester histogram_tester;

  const size_t kRequestSize = 200;
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = kRequestSize;

  BiddingAndAuctionSerializer serializer;
  serializer.SetPublisher("foo");
  serializer.SetGenerationId(
      base::Uuid::ParseCaseInsensitive("00000000-0000-0000-0000-000000000000"));
  serializer.SetConfig(std::move(config));

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

  const size_t kRequestSize = 3000;
  blink::mojom::AuctionDataConfigPtr config =
      blink::mojom::AuctionDataConfig::New();
  config->request_size = kRequestSize;

  config->per_buyer_configs[kOriginA] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/1000);
  config->per_buyer_configs[kOriginB] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/1000);
  config->per_buyer_configs[kOriginC] =
      blink::mojom::AuctionDataBuyerConfig::New(/*size=*/1000);
  config->per_buyer_configs[kOriginD] =
      blink::mojom::AuctionDataBuyerConfig::New();

  BiddingAndAuctionSerializer serializer;
  serializer.SetPublisher("foo");
  serializer.SetGenerationId(
      base::Uuid::ParseCaseInsensitive("00000000-0000-0000-0000-000000000000"));
  serializer.SetConfig(std::move(config));

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
  serializer.SetConfig(std::move(config));

  AddGroupsToSerializer(serializer);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), kRequestSize - kEncryptionOverhead);

  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 0, 2);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 4, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumGroups", 236, 1);
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
  serializer.SetConfig(std::move(config));

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
  serializer.SetConfig(std::move(config));

  AddGroupsToSerializer(serializer);

  BiddingAndAuctionData data = serializer.Build();
  EXPECT_EQ(data.request.size(), kRequestSize - kEncryptionOverhead);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 3, 2);
  histogram_tester.ExpectBucketCount(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 0, 2);
  histogram_tester.ExpectTotalCount(
      "Ads.InterestGroup.ServerAuction.Request.NumIterations", 4);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.NumGroups", 200, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.ServerAuction.Request.RelativeCompressedSize", 1, 1);
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
  serializer.SetConfig(std::move(config));

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

}  // namespace
}  // namespace content
