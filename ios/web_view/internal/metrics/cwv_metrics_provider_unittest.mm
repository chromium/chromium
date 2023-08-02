// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/metrics/cwv_metrics_provider_internal.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "components/metrics/library_support/histogram_manager.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

namespace ios_web_view {

class CWVMetricsProviderTest : public PlatformTest {
 protected:
  CWVMetricsProviderTest() {
    auto histogram_manager = std::make_unique<metrics::HistogramManager>();
    metrics_recorder_ = [[CWVMetricsProvider alloc]
        initWithHistogramManager:std::move(histogram_manager)];
  }
  CWVMetricsProvider* metrics_recorder_;
};

// Tests that -[CWVMetricsProvider consumeMetrics] consumes the histograms.
TEST_F(CWVMetricsProviderTest, ConsumeMetrics) {
  // Consume all existing metrics that may have been logged before this test.
  [metrics_recorder_ consumeMetrics];

  UMA_HISTOGRAM_ENUMERATION("CWVMetricsProviderTest", 1, 2);
  NSData* deltas = [metrics_recorder_ consumeMetrics];
  EXPECT_LT(0U, deltas.length);

  // Using C++ proto because there's no gn rule for generating ObjC protos yet.
  metrics::ChromeUserMetricsExtension uma_proto;
  ASSERT_TRUE(uma_proto.ParseFromArray(deltas.bytes, deltas.length));
  ASSERT_EQ(1, uma_proto.histogram_event_size());

  const metrics::HistogramEventProto& histogram_proto =
      uma_proto.histogram_event(0);
  ASSERT_EQ(1, histogram_proto.bucket_size());

  const metrics::HistogramEventProto_Bucket bucket = histogram_proto.bucket(0);
  EXPECT_FALSE(bucket.has_min());
  EXPECT_LE(2, bucket.max());
  EXPECT_EQ(1, bucket.count());

  // The second call should result in no deltas.
  EXPECT_NSEQ([NSData data], [metrics_recorder_ consumeMetrics]);
}

}  // namespace ios_web_view
