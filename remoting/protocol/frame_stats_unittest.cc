// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/frame_stats.h"
#include "remoting/proto/video_stats.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

class FrameStatsTest : public testing::Test {};

TEST_F(FrameStatsTest, ToStatsMessageAndBack_RestoresFrameStats) {
  HostFrameStats stats;
  stats.frame_size = 10;
  stats.capture_delay = base::TimeDelta::FromSeconds(11);
  stats.encode_delay = base::TimeDelta::FromSeconds(12);
  stats.capture_pending_delay = base::TimeDelta::FromSeconds(13);
  stats.capture_overhead_delay = base::TimeDelta::FromSeconds(14);
  stats.encode_pending_delay = base::TimeDelta::FromSeconds(15);
  stats.send_pending_delay = base::TimeDelta::FromSeconds(16);
  stats.rtt_estimate = base::TimeDelta::FromSeconds(17);
  stats.bandwidth_estimate_kbps = 18;
  stats.capturer_id = 19;
  stats.frame_quality = 20;
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
}

}  // namespace protocol
}  // namespace remoting
