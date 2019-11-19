// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/webrtc_uma_histograms.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace blink {

class MockPerSessionWebRTCAPIMetrics : public PerSessionWebRTCAPIMetrics {
 public:
  MockPerSessionWebRTCAPIMetrics() {}

  using PerSessionWebRTCAPIMetrics::LogUsageOnlyOnce;

  MOCK_METHOD1(LogUsage, void(WebRTCAPIName));
};

class PerSessionWebRTCAPIMetricsTest
    : public testing::Test,
      public testing::WithParamInterface<WebRTCAPIName> {
 public:
  PerSessionWebRTCAPIMetricsTest() = default;
  ~PerSessionWebRTCAPIMetricsTest() override = default;

 protected:
  MockPerSessionWebRTCAPIMetrics metrics;
};

TEST_P(PerSessionWebRTCAPIMetricsTest, NoCallOngoing) {
  WebRTCAPIName api_name = GetParam();
  EXPECT_CALL(metrics, LogUsage(api_name)).Times(1);
  metrics.LogUsageOnlyOnce(api_name);
}

TEST_P(PerSessionWebRTCAPIMetricsTest, CallOngoing) {
  WebRTCAPIName api_name = GetParam();
  metrics.IncrementStreamCounter();
  EXPECT_CALL(metrics, LogUsage(api_name)).Times(1);
  metrics.LogUsageOnlyOnce(api_name);
}

INSTANTIATE_TEST_SUITE_P(
    PerSessionWebRTCAPIMetricsTest,
    PerSessionWebRTCAPIMetricsTest,
    ::testing::ValuesIn({WebRTCAPIName::kGetUserMedia,
                         WebRTCAPIName::kGetDisplayMedia,
                         WebRTCAPIName::kEnumerateDevices,
                         WebRTCAPIName::kRTCPeerConnection}));

TEST(PerSessionWebRTCAPIMetrics, NoCallOngoingMultiplePC) {
  MockPerSessionWebRTCAPIMetrics metrics;
  EXPECT_CALL(metrics, LogUsage(WebRTCAPIName::kRTCPeerConnection)).Times(1);
  metrics.LogUsageOnlyOnce(WebRTCAPIName::kRTCPeerConnection);
  metrics.LogUsageOnlyOnce(WebRTCAPIName::kRTCPeerConnection);
  metrics.LogUsageOnlyOnce(WebRTCAPIName::kRTCPeerConnection);
}

TEST(PerSessionWebRTCAPIMetrics, BeforeAfterCallMultiplePC) {
  MockPerSessionWebRTCAPIMetrics metrics;
  EXPECT_CALL(metrics, LogUsage(WebRTCAPIName::kRTCPeerConnection)).Times(1);
  metrics.LogUsageOnlyOnce(WebRTCAPIName::kRTCPeerConnection);
  metrics.LogUsageOnlyOnce(WebRTCAPIName::kRTCPeerConnection);
  metrics.IncrementStreamCounter();
  metrics.IncrementStreamCounter();
  metrics.LogUsageOnlyOnce(WebRTCAPIName::kRTCPeerConnection);
  metrics.DecrementStreamCounter();
  metrics.LogUsageOnlyOnce(WebRTCAPIName::kRTCPeerConnection);
  metrics.DecrementStreamCounter();
  EXPECT_CALL(metrics, LogUsage(WebRTCAPIName::kRTCPeerConnection)).Times(1);
  metrics.LogUsageOnlyOnce(WebRTCAPIName::kRTCPeerConnection);
  metrics.LogUsageOnlyOnce(WebRTCAPIName::kRTCPeerConnection);
}

}  // namespace blink
