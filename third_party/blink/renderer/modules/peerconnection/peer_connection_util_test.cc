// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_util.h"

#include <cstdint>
#include <cstdlib>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

base::TimeTicks GetTimeOrigin(V8TestingScope& v8_scope) {
  return DOMWindowPerformance::performance(v8_scope.GetWindow())
      ->GetTimeOriginInternal();
}

}  // namespace

TEST(PeerConnectionUtilTest, RTCEncodedFrameTimestampToTimeTicks) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  EXPECT_EQ(RTCEncodedFrameTimestampToTimeTicks(v8_scope.GetExecutionContext(),
                                                123.456),
            GetTimeOrigin(v8_scope) + base::Microseconds(123456));
}

TEST(PeerConnectionUtilTest, RTCEncodedFrameTimestampToTimeTicksNegative) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  EXPECT_EQ(RTCEncodedFrameTimestampToTimeTicks(v8_scope.GetExecutionContext(),
                                                -123.456),
            GetTimeOrigin(v8_scope) + base::Microseconds(-123456));
}

TEST(PeerConnectionUtilTest, CalculateRTCEncodedFrameTimestamp) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  // Use a timestamp precise to 0.1ms, since that is the precision of
  // DOMHighResTimeStamp without cross-origin isolation.
  DOMHighResTimeStamp timestamp = CalculateRTCEncodedFrameTimestamp(
      v8_scope.GetExecutionContext(),
      GetTimeOrigin(v8_scope) + base::Microseconds(123400));
  // Use 0.2ms as tolerance to account for the 0.1ms precision.
  EXPECT_LE(std::abs(timestamp - 123.4), 0.2);
}

TEST(PeerConnectionUtilTest, CalculateRTCEncodedFrameTimestampNegative) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  // Use a timestamp precise to 0.1ms, since that is the precision of
  // DOMHighResTimeStamp without cross-origin isolation.
  DOMHighResTimeStamp timestamp = CalculateRTCEncodedFrameTimestamp(
      v8_scope.GetExecutionContext(),
      GetTimeOrigin(v8_scope) + base::Microseconds(-123400));
  // Use 0.2ms as tolerance to account for the 0.1ms precision.
  EXPECT_LE(timestamp - -123.4, 0.2);
}

TEST(PeerConnectionUtilTest, CalculateRTCEncodedFrameTimeDelta) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  // Use a timestamp precise to 0.1ms, since that is the precision of
  // DOMHighResTimeStamp without cross-origin isolation.
  DOMHighResTimeStamp timestamp = CalculateRTCEncodedFrameTimeDelta(
      v8_scope.GetExecutionContext(), base::Microseconds(123400));
  // Use 0.2ms as tolerance to account for the 0.1ms precision.
  EXPECT_LE(timestamp - 123.4, 0.2);
}

TEST(PeerConnectionUtilTest, CalculateRTCEncodedFrameTimeDeltaNegative) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  // Use a timestamp precise to 0.1ms, since that is the precision of
  // DOMHighResTimeStamp without cross-origin isolation.
  DOMHighResTimeStamp timestamp = CalculateRTCEncodedFrameTimeDelta(
      v8_scope.GetExecutionContext(), base::Microseconds(-123400));
  // Use 0.2ms as tolerance to account for the 0.1ms precision.
  EXPECT_LE(timestamp - -123.4, 0.2);
}

TEST(PeerConnectionUtilTest, AudioLevelConversionRangeEndpoints) {
  EXPECT_EQ(ToLinearAudioLevel(0u), 1.0);
  EXPECT_EQ(ToLinearAudioLevel(127u), 0.0);
  EXPECT_EQ(FromLinearAudioLevel(1.0), 0u);
  EXPECT_EQ(FromLinearAudioLevel(0.0), 127u);
}

TEST(PeerConnectionUtilTest, AudioLevelConversionOutsideRange) {
  EXPECT_EQ(FromLinearAudioLevel(1.1), 0u);
  EXPECT_EQ(FromLinearAudioLevel(-0.1), 127u);
}

TEST(PeerConnectionUtilTest,
     AudioLevelConversionFromSmallLinearValueIsSilence) {
  EXPECT_EQ(FromLinearAudioLevel(1e-30), 127u);
}

TEST(PeerConnectionUtilTest, AudioLevelConversionFromLinearIsNotTooLossy) {
  constexpr double linear_audio_level = 0.34;
  double converted_linear_audio_level =
      ToLinearAudioLevel(FromLinearAudioLevel(linear_audio_level));
  EXPECT_LE(std::abs(linear_audio_level - converted_linear_audio_level) /
                linear_audio_level,
            0.1);
}

TEST(PeerConnectionUtilTest, AudioLevelConversionFromUintIsLossless) {
  constexpr uint8_t audio_level_dbov = 34u;
  double converted_audio_level_dbov =
      FromLinearAudioLevel(ToLinearAudioLevel(audio_level_dbov));
  EXPECT_EQ(audio_level_dbov, converted_audio_level_dbov);
}

}  // namespace blink
