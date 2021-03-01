#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/transport/rtp/rtp_source.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace blink {

TEST(RtcRtpSource, BasicPropertiesAreSetAndReturned) {
  int64_t kTimestampMs = 12345678;
  uint32_t kSourceId = 5;
  webrtc::RtpSourceType kSourceType = webrtc::RtpSourceType::SSRC;
  uint32_t kRtpTimestamp = 112233;
  webrtc::RtpSource rtp_source(kTimestampMs, kSourceId, kSourceType,
                               kRtpTimestamp, webrtc::RtpSource::Extensions());

  RTCRtpSource rtc_rtp_source(rtp_source);

  EXPECT_EQ((rtc_rtp_source.Timestamp() - base::TimeTicks()).InMilliseconds(),
            kTimestampMs);
  EXPECT_EQ(rtc_rtp_source.Source(), kSourceId);
  EXPECT_EQ(rtc_rtp_source.SourceType(), RTCRtpSource::Type::kSSRC);
  EXPECT_EQ(rtc_rtp_source.RtpTimestamp(), kRtpTimestamp);
}

// The Timestamp() function relies on the fact that Base::TimeTicks() and
// rtc::TimeMicros() share the same implementation.
TEST(RtcRtpSource, BaseTimeTicksAndRtcMicrosAreTheSame) {
  base::TimeTicks first_chromium_timestamp = base::TimeTicks::Now();
  base::TimeTicks webrtc_timestamp =
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(rtc::TimeMicros());
  base::TimeTicks second_chromium_timestamp = base::TimeTicks::Now();

  // Test that the timestamps are correctly ordered, which they can only be if
  // the clocks are the same (assuming at least one of the clocks is functioning
  // correctly).
  EXPECT_GE((webrtc_timestamp - first_chromium_timestamp).InMillisecondsF(),
            0.0f);
  EXPECT_GE((second_chromium_timestamp - webrtc_timestamp).InMillisecondsF(),
            0.0f);
}

}  // namespace blink
