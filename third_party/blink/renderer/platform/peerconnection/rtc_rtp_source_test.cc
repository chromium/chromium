// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_source.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/webrtc/api/rtp_headers.h"
#include "third_party/webrtc/api/transport/rtp/rtp_source.h"
#include "third_party/webrtc/api/units/timestamp.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace blink {
namespace {

constexpr webrtc::Timestamp kTimestamp = webrtc::Timestamp::Millis(12345678);
constexpr uint32_t kSourceId = 5;
constexpr webrtc::RtpSourceType kSourceType = webrtc::RtpSourceType::SSRC;
constexpr uint32_t kRtpTimestamp = 112233;

// Q32x32 formatted timestamps.
constexpr uint64_t kUint64One = 1;
constexpr uint64_t kQ32x32Time1000ms = kUint64One << 32;
constexpr uint64_t kQ32x32Time1250ms = kQ32x32Time1000ms | kUint64One << 30;
constexpr uint64_t kQ32x32Time1500ms = kQ32x32Time1000ms | kUint64One << 31;
constexpr int64_t kQ32x32TimeNegative500ms = -(kUint64One << 31);

}  // namespace

TEST(RtcRtpSource, BasicPropertiesAreSetAndReturned) {
  webrtc::RtpSource rtp_source(kTimestamp, kSourceId, kSourceType,
                               kRtpTimestamp, webrtc::RtpSource::Extensions());

  RTCRtpSource rtc_rtp_source(rtp_source);

  EXPECT_EQ(rtc_rtp_source.Timestamp(), ConvertToBaseTimeTicks(kTimestamp));
  EXPECT_EQ(rtc_rtp_source.Source(), kSourceId);
  EXPECT_EQ(rtc_rtp_source.SourceType(), RTCRtpSource::Type::kSSRC);
  EXPECT_EQ(rtc_rtp_source.RtpTimestamp(), kRtpTimestamp);
}

// The Timestamp() function relies on the fact that Base::TimeTicks() and
// rtc::TimeMicros() share the same implementation.
TEST(RtcRtpSource, BaseTimeTicksAndRtcMicrosAreTheSame) {
  base::TimeTicks first_chromium_timestamp = base::TimeTicks::Now();
  base::TimeTicks webrtc_timestamp =
      ConvertToBaseTimeTicks(webrtc::Timestamp::Micros(rtc::TimeMicros()));
  base::TimeTicks second_chromium_timestamp = base::TimeTicks::Now();

  // Test that the timestamps are correctly ordered, which they can only be if
  // the clocks are the same (assuming at least one of the clocks is functioning
  // correctly).
  EXPECT_GE((webrtc_timestamp - first_chromium_timestamp).InMillisecondsF(),
            0.0f);
  EXPECT_GE((second_chromium_timestamp - webrtc_timestamp).InMillisecondsF(),
            0.0f);
}

TEST(RtcRtpSource, AbsoluteCaptureTimeSetAndReturnedNoOffset) {
  constexpr webrtc::AbsoluteCaptureTime kAbsCaptureTime{
      .absolute_capture_timestamp = kQ32x32Time1000ms};
  webrtc::RtpSource rtp_source(
      kTimestamp, kSourceId, kSourceType, kRtpTimestamp,
      /*extensions=*/{.absolute_capture_time = kAbsCaptureTime});
  RTCRtpSource rtc_rtp_source(rtp_source);
  EXPECT_EQ(rtc_rtp_source.CaptureTimestamp(), 1000);
  EXPECT_FALSE(rtc_rtp_source.SenderCaptureTimeOffset().has_value());
}

TEST(RtcRtpSource, AbsoluteCaptureTimeSetAndReturnedWithZeroOffset) {
  constexpr webrtc::AbsoluteCaptureTime kAbsCaptureTime{
      .absolute_capture_timestamp = kQ32x32Time1250ms,
      .estimated_capture_clock_offset = 0};
  webrtc::RtpSource rtp_source(
      kTimestamp, kSourceId, kSourceType, kRtpTimestamp,
      /*extensions=*/{.absolute_capture_time = kAbsCaptureTime});
  RTCRtpSource rtc_rtp_source(rtp_source);
  EXPECT_EQ(rtc_rtp_source.CaptureTimestamp(), 1250);
  ASSERT_TRUE(rtc_rtp_source.SenderCaptureTimeOffset().has_value());
  EXPECT_EQ(rtc_rtp_source.SenderCaptureTimeOffset(), 0);
}

TEST(RtcRtpSource, AbsoluteCaptureTimeSetAndReturnedWithPositiveOffset) {
  constexpr webrtc::AbsoluteCaptureTime kAbsCaptureTime{
      .absolute_capture_timestamp = kQ32x32Time1250ms,
      .estimated_capture_clock_offset = kQ32x32Time1500ms};
  webrtc::RtpSource rtp_source(
      kTimestamp, kSourceId, kSourceType, kRtpTimestamp,
      /*extensions=*/{.absolute_capture_time = kAbsCaptureTime});
  RTCRtpSource rtc_rtp_source(rtp_source);
  EXPECT_EQ(rtc_rtp_source.CaptureTimestamp(), 1250);
  ASSERT_TRUE(rtc_rtp_source.SenderCaptureTimeOffset().has_value());
  EXPECT_EQ(rtc_rtp_source.SenderCaptureTimeOffset(), 1500);
}

TEST(RtcRtpSource, AbsoluteCaptureTimeSetAndReturnedWithNegativeOffset) {
  constexpr webrtc::AbsoluteCaptureTime kAbsCaptureTime{
      .absolute_capture_timestamp = kQ32x32Time1250ms,
      .estimated_capture_clock_offset = kQ32x32TimeNegative500ms};
  webrtc::RtpSource rtp_source(
      kTimestamp, kSourceId, kSourceType, kRtpTimestamp,
      /*extensions=*/{.absolute_capture_time = kAbsCaptureTime});
  RTCRtpSource rtc_rtp_source(rtp_source);
  EXPECT_EQ(rtc_rtp_source.CaptureTimestamp(), 1250);
  EXPECT_EQ(rtc_rtp_source.SenderCaptureTimeOffset(), -500);
}

}  // namespace blink
