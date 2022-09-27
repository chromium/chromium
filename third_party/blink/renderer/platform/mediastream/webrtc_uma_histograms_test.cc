// Copyright 2014 The Chromium Authors
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

  MOCK_METHOD1(LogUsage, void(RTCAPIName));
};

class PerSessionWebRTCAPIMetricsTest
    : public testing::Test,
      public testing::WithParamInterface<RTCAPIName> {
 public:
  PerSessionWebRTCAPIMetricsTest() = default;
  ~PerSessionWebRTCAPIMetricsTest() override = default;

 protected:
  MockPerSessionWebRTCAPIMetrics metrics;
};

TEST_P(PerSessionWebRTCAPIMetricsTest, NoCallOngoing) {
  RTCAPIName api_name = GetParam();
  EXPECT_CALL(metrics, LogUsage(api_name)).Times(1);
  metrics.LogUsageOnlyOnce(api_name);
}

TEST_P(PerSessionWebRTCAPIMetricsTest, CallOngoing) {
  RTCAPIName api_name = GetParam();
  metrics.IncrementStreamCounter();
  EXPECT_CALL(metrics, LogUsage(api_name)).Times(1);
  metrics.LogUsageOnlyOnce(api_name);
}

INSTANTIATE_TEST_SUITE_P(
    PerSessionWebRTCAPIMetricsTest,
    PerSessionWebRTCAPIMetricsTest,
    ::testing::ValuesIn({RTCAPIName::kGetUserMedia,
                         RTCAPIName::kGetDisplayMedia,
                         RTCAPIName::kEnumerateDevices,
                         RTCAPIName::kRTCPeerConnection}));

TEST(PerSessionWebRTCAPIMetrics, NoCallOngoingMultiplePC) {
  MockPerSessionWebRTCAPIMetrics metrics;
  EXPECT_CALL(metrics, LogUsage(RTCAPIName::kRTCPeerConnection)).Times(1);
  metrics.LogUsageOnlyOnce(RTCAPIName::kRTCPeerConnection);
  metrics.LogUsageOnlyOnce(RTCAPIName::kRTCPeerConnection);
  metrics.LogUsageOnlyOnce(RTCAPIName::kRTCPeerConnection);
}

TEST(PerSessionWebRTCAPIMetrics, BeforeAfterCallMultiplePC) {
  MockPerSessionWebRTCAPIMetrics metrics;
  EXPECT_CALL(metrics, LogUsage(RTCAPIName::kRTCPeerConnection)).Times(1);
  metrics.LogUsageOnlyOnce(RTCAPIName::kRTCPeerConnection);
  metrics.LogUsageOnlyOnce(RTCAPIName::kRTCPeerConnection);
  metrics.IncrementStreamCounter();
  metrics.IncrementStreamCounter();
  metrics.LogUsageOnlyOnce(RTCAPIName::kRTCPeerConnection);
  metrics.DecrementStreamCounter();
  metrics.LogUsageOnlyOnce(RTCAPIName::kRTCPeerConnection);
  metrics.DecrementStreamCounter();
  EXPECT_CALL(metrics, LogUsage(RTCAPIName::kRTCPeerConnection)).Times(1);
  metrics.LogUsageOnlyOnce(RTCAPIName::kRTCPeerConnection);
  metrics.LogUsageOnlyOnce(RTCAPIName::kRTCPeerConnection);
}

}  // namespace blink
