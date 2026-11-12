// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_active_map.h"

#include <cstdint>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

TEST(VideoEncoderActiveMapTest, InitializesWithRoundedMacroblockDimensions) {
  VideoEncoderActiveMap active_map;
  // 1920 / 16 = 120, (1080 + 15) / 16 = 68.
  active_map.Initialize(webrtc::DesktopSize(1920, 1080));
  EXPECT_EQ(120u, active_map.width());
  EXPECT_EQ(68u, active_map.height());
  EXPECT_EQ(8160u, active_map.total_blocks());
  EXPECT_EQ(0u, active_map.active_blocks());
}

TEST(VideoEncoderActiveMapTest, TracksUniqueActiveBlocksAcrossUpdates) {
  VideoEncoderActiveMap active_map;
  active_map.Initialize(webrtc::DesktopSize(64, 64));
  ASSERT_EQ(4u, active_map.width());
  ASSERT_EQ(4u, active_map.height());
  ASSERT_EQ(16u, active_map.total_blocks());

  // Single 16x16 macroblock at (0, 0).
  active_map.Update(
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeXYWH(0, 0, 16, 16)));
  EXPECT_EQ(1u, active_map.active_blocks());
  EXPECT_EQ(1u, active_map.as_span()[0]);

  // Repeating the same region does not double-count active blocks.
  active_map.Update(
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeXYWH(0, 0, 16, 16)));
  EXPECT_EQ(1u, active_map.active_blocks());

  // Rect straddling 2x2 macroblocks at (8, 8) -> (24, 24) adds 3 new blocks.
  active_map.Update(
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeXYWH(8, 8, 16, 16)));
  EXPECT_EQ(4u, active_map.active_blocks());

  // Empty region (such as an extrapolated top-off frame) preserves active map.
  active_map.Update(webrtc::DesktopRegion());
  EXPECT_EQ(4u, active_map.active_blocks());
}

TEST(VideoEncoderActiveMapTest, SetFullAndClearUpdateBlockCount) {
  VideoEncoderActiveMap active_map;
  active_map.Initialize(webrtc::DesktopSize(64, 64));

  active_map.SetFull();
  EXPECT_EQ(16u, active_map.active_blocks());
  for (uint8_t val : active_map.as_span()) {
    EXPECT_EQ(1u, val);
  }

  // Subsequent updates while full are no-ops.
  active_map.Update(
      webrtc::DesktopRegion(webrtc::DesktopRect::MakeXYWH(0, 0, 32, 32)));
  EXPECT_EQ(16u, active_map.active_blocks());

  active_map.Clear();
  EXPECT_EQ(0u, active_map.active_blocks());
  for (uint8_t val : active_map.as_span()) {
    EXPECT_EQ(0u, val);
  }
}

}  // namespace remoting
