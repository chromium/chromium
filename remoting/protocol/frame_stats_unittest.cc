// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/frame_stats.h"
#include "remoting/proto/video_stats.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/video/video_codec_type.h"

namespace remoting::protocol {

class FrameStatsTest : public testing::Test {};

TEST_F(FrameStatsTest, ToStatsMessageAndBack_RestoresFrameStats) {
  HostFrameStats stats;
  stats.frame_size = 10;
  stats.capture_delay = base::Seconds(11);
  stats.encode_delay = base::Seconds(12);
  stats.capture_pending_delay = base::Seconds(13);
  stats.capture_overhead_delay = base::Seconds(14);
  stats.encode_pending_delay = base::Seconds(15);
  stats.send_pending_delay = base::Seconds(16);
  stats.rtt_estimate = base::Seconds(17);
  stats.bandwidth_estimate_kbps = 18;
  stats.capturer_id = 19;
  stats.frame_quality = 20;
  stats.codec = remoting::FrameStatsMessage::AV1;
  stats.profile = 1;
  stats.encoded_rect_width = 1024;
  stats.encoded_rect_height = 768;
  FrameStatsMessage message;

  stats.ToFrameStatsMessage(&message);
  HostFrameStats newStats = HostFrameStats::FromFrameStatsMessage(message);

  EXPECT_EQ(stats.frame_size, newStats.frame_size);
  EXPECT_EQ(stats.capture_delay, newStats.capture_delay);
  EXPECT_EQ(stats.encode_delay, newStats.encode_delay);
  EXPECT_EQ(stats.capture_pending_delay, newStats.capture_pending_delay);
  EXPECT_EQ(stats.capture_overhead_delay, newStats.capture_overhead_delay);
  EXPECT_EQ(stats.encode_pending_delay, newStats.encode_pending_delay);
  EXPECT_EQ(stats.send_pending_delay, newStats.send_pending_delay);
  EXPECT_EQ(stats.rtt_estimate, newStats.rtt_estimate);
  EXPECT_EQ(stats.bandwidth_estimate_kbps, newStats.bandwidth_estimate_kbps);
  EXPECT_EQ(stats.capturer_id, newStats.capturer_id);
  EXPECT_EQ(stats.frame_quality, newStats.frame_quality);
  EXPECT_EQ(stats.codec, remoting::FrameStatsMessage::AV1);
  EXPECT_EQ(stats.profile, 1);
  EXPECT_EQ(stats.encoded_rect_width, 1024);
  EXPECT_EQ(stats.encoded_rect_height, 768);
}

}  // namespace remoting::protocol
