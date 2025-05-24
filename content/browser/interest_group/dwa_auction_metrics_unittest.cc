// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/dwa_auction_metrics.h"

#include "base/metrics/metrics_hashes.h"
#include "base/test/scoped_feature_list.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {
class DwaAuctionMetricsTest : public testing::Test {
 public:
  DwaAuctionMetricsTest() = default;
};

// Only testing whether the recorder receives the hashed metrics. The collection
// logic is already tested in the DWA library.
TEST_F(DwaAuctionMetricsTest, TestDwaAuctionMetricsRecord) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(metrics::dwa::kDwaFeature);
  metrics::dwa::DwaRecorder::Get()->EnableRecording();
  metrics::dwa::DwaRecorder::Get()->Purge();
  ASSERT_THAT(metrics::dwa::DwaRecorder::Get()->GetEntriesForTesting(),
              testing::IsEmpty());
  DwaAuctionMetrics dwa_auction_metrics;
  dwa_auction_metrics.SetSellerInfo(
      url::Origin::Create(GURL("https://seller.test")));
  dwa_auction_metrics.OnAuctionEnd(AuctionResult::kSuccess);
  ASSERT_THAT(metrics::dwa::DwaRecorder::Get()->GetEntriesForTesting(),
              testing::SizeIs(1));
  EXPECT_THAT(
      metrics::dwa::DwaRecorder::Get()->GetEntriesForTesting()[0]->content_hash,
      testing::Eq(base::HashMetricName("seller.test")));
  EXPECT_THAT(
      metrics::dwa::DwaRecorder::Get()->GetEntriesForTesting()[0]->metrics,
      testing::UnorderedElementsAre(
          testing::Pair(base::HashMetricName("Result"), 0)));
}

}  // namespace
}  // namespace content
