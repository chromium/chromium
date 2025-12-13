// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/coordinate_conversion.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting::protocol {

TEST(CoordinateConversionTest, ToFractionalCoordinate_Valid) {
  webrtc::ScreenId screen_id = 123;
  webrtc::DesktopSize screen_size{100, 100};
  webrtc::DesktopVector coordinate{50, 80};

  FractionalCoordinate fractional =
      ToFractionalCoordinate(screen_id, screen_size, coordinate);

  EXPECT_EQ(fractional.screen_id(), screen_id);
  EXPECT_NEAR(fractional.x(), 50.0f / 99.0f, 0.0001);
  EXPECT_NEAR(fractional.y(), 80.0f / 99.0f, 0.0001);
}

TEST(CoordinateConversionTest, ToFractionalCoordinate_Edges) {
  webrtc::ScreenId screen_id = 123;
  webrtc::DesktopSize screen_size{100, 100};

  // Top-left
  FractionalCoordinate fractional =
      ToFractionalCoordinate(screen_id, screen_size, {0, 0});
  EXPECT_EQ(fractional.screen_id(), screen_id);
  EXPECT_NEAR(fractional.x(), 0.0f, 0.0001);
  EXPECT_NEAR(fractional.y(), 0.0f, 0.0001);

  // Bottom-right
  fractional = ToFractionalCoordinate(screen_id, screen_size, {99, 99});
  EXPECT_EQ(fractional.screen_id(), screen_id);
  EXPECT_NEAR(fractional.x(), 1.0f, 0.0001);
  EXPECT_NEAR(fractional.y(), 1.0f, 0.0001);
}

TEST(CoordinateConversionTest, ToFractionalCoordinate_OutOfBounds_Clamped) {
  webrtc::ScreenId screen_id = 123;
  webrtc::DesktopSize screen_size{100, 100};

  // Less than 0
  FractionalCoordinate fractional =
      ToFractionalCoordinate(screen_id, screen_size, {-1, -1});
  EXPECT_EQ(fractional.screen_id(), screen_id);
  EXPECT_NEAR(fractional.x(), 0.0f, 0.0001);
  EXPECT_NEAR(fractional.y(), 0.0f, 0.0001);

  // Greater than size - 1
  fractional = ToFractionalCoordinate(screen_id, screen_size, {100, 100});
  EXPECT_EQ(fractional.screen_id(), screen_id);
  EXPECT_NEAR(fractional.x(), 1.0f, 0.0001);
  EXPECT_NEAR(fractional.y(), 1.0f, 0.0001);
}

TEST(CoordinateConversionTest, ToFractionalCoordinate_SmallScreen) {
  webrtc::ScreenId screen_id = 123;

  // 1x1 screen
  webrtc::DesktopSize screen_size_1x1{1, 1};
  FractionalCoordinate fractional =
      ToFractionalCoordinate(screen_id, screen_size_1x1, {0, 0});
  EXPECT_EQ(fractional.screen_id(), screen_id);
  EXPECT_NEAR(fractional.x(), 0.0f, 0.0001);
  EXPECT_NEAR(fractional.y(), 0.0f, 0.0001);

  // Clamping on 1x1 screen
  fractional = ToFractionalCoordinate(screen_id, screen_size_1x1, {-1, -1});
  EXPECT_EQ(fractional.screen_id(), screen_id);
  EXPECT_NEAR(fractional.x(), 0.0f, 0.0001);
  EXPECT_NEAR(fractional.y(), 0.0f, 0.0001);

  fractional = ToFractionalCoordinate(screen_id, screen_size_1x1, {1, 1});
  EXPECT_EQ(fractional.screen_id(), screen_id);
  EXPECT_NEAR(fractional.x(), 0.0f, 0.0001);
  EXPECT_NEAR(fractional.y(), 0.0f, 0.0001);

  // 0x0 screen
  webrtc::DesktopSize screen_size_0x0{0, 0};
  fractional = ToFractionalCoordinate(screen_id, screen_size_0x0, {0, 0});
  EXPECT_EQ(fractional.screen_id(), screen_id);
  EXPECT_NEAR(fractional.x(), 0.0f, 0.0001);
  EXPECT_NEAR(fractional.y(), 0.0f, 0.0001);

  // 1x0 screen
  webrtc::DesktopSize screen_size_1x0{1, 0};
  fractional = ToFractionalCoordinate(screen_id, screen_size_1x0, {0, 0});
  EXPECT_EQ(fractional.screen_id(), screen_id);
  EXPECT_NEAR(fractional.x(), 0.0f, 0.0001);
  EXPECT_NEAR(fractional.y(), 0.0f, 0.0001);
}

}  // namespace remoting::protocol
