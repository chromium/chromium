// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/peer_connection_util.h"

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <vector>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

base::TimeTicks GetTimeOriginTimeTicks(V8TestingScope& v8_scope) {
  return DOMWindowPerformance::performance(v8_scope.GetWindow())
      ->GetTimeOriginInternal();
}

DOMHighResTimeStamp GetTimeOriginNtp(V8TestingScope& v8_scope) {
  return DOMWindowPerformance::performance(v8_scope.GetWindow())->timeOrigin() +
         2208988800000.0;
}

}  // namespace

TEST(PeerConnectionUtilTest, RTCTimeStampFromTimeTicks) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  // Use timestamps precise to 0.1ms, since that is the precision of
  // DOMHighResTimeStamp without cross-origin isolation.
  std::vector<double> timestamps_ms = {123.4, -123.4};
  for (double timestamp_ms : timestamps_ms) {
    DOMHighResTimeStamp timestamp = RTCTimeStampFromTimeTicks(
        v8_scope.GetExecutionContext(),
        GetTimeOriginTimeTicks(v8_scope) + base::Milliseconds(timestamp_ms));
    // Use 0.2ms as tolerance to account for the 0.1ms precision.
    EXPECT_LE(std::abs(timestamp - timestamp_ms), 0.2);
  }
}

TEST(PeerConnectionUtilTest, CalculateRTCEncodedFrameTimeDelta) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  // Use timestamps precise to 0.1ms, since that is the precision of
  // DOMHighResTimeStamp without cross-origin isolation.
  std::vector<double> timedeltas_ms = {123.4, -123.4};
  for (double timedelta_ms : timedeltas_ms) {
    DOMHighResTimeStamp timestamp = CalculateRTCEncodedFrameTimeDelta(
        v8_scope.GetExecutionContext(), base::Milliseconds(timedelta_ms));
    // Use 0.2ms as tolerance to account for the 0.1ms precision.
    EXPECT_LE(std::abs(timestamp - timedelta_ms), 0.2);
  }
}

TEST(PeerConnectionUtilTest,
     RTCEncodedFrameTimestampFromCaptureTimeInfoTimeTicks) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  std::vector<double> timestamps_ms = {123.4, -123.4};
  for (double timestamp_ms : timestamps_ms) {
    DOMHighResTimeStamp timestamp = RTCEncodedFrameTimestampFromCaptureTimeInfo(
        v8_scope.GetExecutionContext(),
        {.capture_time = (GetTimeOriginTimeTicks(v8_scope) +
                          base::Milliseconds(timestamp_ms))
                             .since_origin(),
         .clock_type = CaptureTimeInfo::ClockType::kTimeTicks});
    // Use 0.2ms as tolerance to account for the 0.1ms precision.
    EXPECT_LE(std::abs(timestamp - timestamp_ms), 0.2);
  }
}

TEST(PeerConnectionUtilTest, RTCEncodedFrameTimestampFromCaptureTimeInfoNtp) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  std::vector<double> timestamps_ms = {123.4, -123.4};
  for (double timestamp_ms : timestamps_ms) {
    DOMHighResTimeStamp timestamp = RTCEncodedFrameTimestampFromCaptureTimeInfo(
        v8_scope.GetExecutionContext(),
        {.capture_time = base::Milliseconds(timestamp_ms),
         .clock_type = CaptureTimeInfo::ClockType::kNtpRealClock});
    DOMHighResTimeStamp expected_timestamp =
        timestamp_ms - GetTimeOriginNtp(v8_scope);
    // Use 0.2ms as tolerance to account for the 0.1ms precision.
    EXPECT_LE(std::abs(timestamp - expected_timestamp), 0.2);
  }
}

TEST(PeerConnectionUtilTest, RTCEncodedFrameTimestampToCaptureTimeTimeTicks) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  std::vector<double> timestamps_ms = {123.456, -123.456};
  for (double timestamp_ms : timestamps_ms) {
    base::TimeDelta capture_time = RTCEncodedFrameTimestampToCaptureTime(
        v8_scope.GetExecutionContext(), timestamp_ms,
        CaptureTimeInfo::ClockType::kTimeTicks);
    base::TimeDelta expected_time =
        GetTimeOriginTimeTicks(v8_scope).since_origin() +
        base::Milliseconds(timestamp_ms);
    EXPECT_LE((capture_time - expected_time).magnitude(),
              base::Milliseconds(0.2));
  }
}

TEST(PeerConnectionUtilTest, RTCEncodedFrameTimestampToCaptureTimeNtp) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  std::vector<double> timestamps_ms = {123.456, -123.456};
  for (double timestamp_ms : timestamps_ms) {
    base::TimeDelta capture_time = RTCEncodedFrameTimestampToCaptureTime(
        v8_scope.GetExecutionContext(), timestamp_ms,
        CaptureTimeInfo::ClockType::kNtpRealClock);
    base::TimeDelta expected_time =
        base::Milliseconds(GetTimeOriginNtp(v8_scope)) +
        base::Milliseconds(timestamp_ms);
    EXPECT_LE((capture_time - expected_time).magnitude(),
              base::Milliseconds(0.2));
  }
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
