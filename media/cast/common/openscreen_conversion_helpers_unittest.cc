// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/common/openscreen_conversion_helpers.h"

#include "media/cast/cast_config.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openscreen/src/cast/streaming/public/encoded_frame.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"
#include "third_party/openscreen/src/cast/streaming/rtp_time.h"
#include "third_party/openscreen/src/platform/api/time.h"

namespace media::cast {

TEST(OpenscreenConversionHelpersTest, EncodedFrameConversions) {
  SenderEncodedFrame original;
  original.encoder_utilization = 0.6f;
  original.encoder_bitrate = 12345;
  original.lossiness = 0.5f;
  original.encode_completion_time =
      base::TimeTicks() + base::Milliseconds(1337);
  original.dependency =
      openscreen::cast::EncodedFrame::Dependency::kIndependent;
  original.frame_id = FrameId::first();
  original.rtp_timestamp = ToRtpTimeTicks(base::Seconds(3), 9000);
  original.reference_time = base::TimeTicks() + base::Milliseconds(1338);
  original.new_playout_delay_ms = 564;
  original.data = "i am actually a very complex video image!";

  const openscreen::cast::EncodedFrame converted =
      ToOpenscreenEncodedFrame(original);
  EXPECT_EQ(openscreen::cast::EncodedFrame::Dependency::kIndependent,
            converted.dependency);
  EXPECT_EQ(openscreen::cast::FrameId(0), converted.frame_id);
  EXPECT_EQ(openscreen::cast::RtpTimeTicks(27000), converted.rtp_timestamp);
  EXPECT_EQ(openscreen::Clock::time_point() + std::chrono::milliseconds(1338),
            converted.reference_time);
  EXPECT_EQ(std::chrono::milliseconds(564), converted.new_playout_delay);
  EXPECT_STREQ(reinterpret_cast<const char*>(converted.data.data()),
               original.data.data());
}

TEST(OpenscreenConversionHelpersTest, TimeConversions) {
  EXPECT_EQ(openscreen::Clock::time_point() + std::chrono::milliseconds(42),
            ToOpenscreenTimePoint(base::TimeTicks() + base::Milliseconds(42)));

  EXPECT_EQ(base::Milliseconds(300),
            ToTimeDelta(std::chrono::milliseconds(300)));
}

}  // namespace media::cast
