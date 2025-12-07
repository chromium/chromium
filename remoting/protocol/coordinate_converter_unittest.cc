// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/coordinate_converter.h"

#include <optional>

#include "base/containers/span.h"
#include "remoting/proto/control.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting::protocol {

namespace {

MATCHER_P2(EqualsOptionalDesktopVector, x, y, "") {
  if (!arg.has_value()) {
    *result_listener << "which is nullopt";
    return false;
  }
  const webrtc::DesktopVector& vec = arg.value();
  if (vec.x() == x && vec.y() == y) {
    return true;
  }
  *result_listener << "which is (" << vec.x() << ", " << vec.y() << ")";
  return false;
}

struct Screen {
  int id;
  int x;
  int y;
  int width;
  int height;
};

// o--------------+
// | screen_id 1  |
// | 200x100      |
// +--------------+------------------+
//                | screen_id 2      |
//                | 300x200          |
//                |                  |
//                +------------------+
constexpr Screen kSimpleLayout[] = {{1, 0, 0, 200, 100},
                                    {2, 200, 100, 300, 200}};

VideoLayout BuildLayout(base::span<const Screen> screens) {
  VideoLayout layout;
  for (const Screen& screen : screens) {
    VideoTrackLayout* track = layout.add_video_track();
    track->set_screen_id(screen.id);
    track->set_position_x(screen.x);
    track->set_position_y(screen.y);
    track->set_width(screen.width);
    track->set_height(screen.height);
  }
  return layout;
}

FractionalCoordinate BuildFractionalCoordinates(std::optional<int> id,
                                                float x,
                                                float y) {
  FractionalCoordinate result;
  if (id.has_value()) {
    result.set_screen_id(id.value());
  }
  result.set_x(x);
  result.set_y(y);
  return result;
}

}  // namespace

class CoordinateConverterTest : public testing::Test {
 protected:
  CoordinateConverter converter_;
};

TEST_F(CoordinateConverterTest, BasicCalculation) {
  converter_.set_video_layout(BuildLayout(kSimpleLayout));

  EXPECT_THAT(converter_.ToGlobalAbsoluteCoordinate(
                  BuildFractionalCoordinates(1, 0.0, 0.0)),
              EqualsOptionalDesktopVector(0, 0));
  EXPECT_THAT(converter_.ToGlobalAbsoluteCoordinate(
                  BuildFractionalCoordinates(1, 0.5, 0.5)),
              EqualsOptionalDesktopVector(100, 50));
  EXPECT_THAT(converter_.ToGlobalAbsoluteCoordinate(
                  BuildFractionalCoordinates(1, 1.0, 1.0)),
              EqualsOptionalDesktopVector(199, 99));
  EXPECT_THAT(converter_.ToGlobalAbsoluteCoordinate(
                  BuildFractionalCoordinates(2, 0.0, 0.0)),
              EqualsOptionalDesktopVector(200, 100));
  EXPECT_THAT(converter_.ToGlobalAbsoluteCoordinate(
                  BuildFractionalCoordinates(2, 0.5, 0.5)),
              EqualsOptionalDesktopVector(350, 200));
  EXPECT_THAT(converter_.ToGlobalAbsoluteCoordinate(
                  BuildFractionalCoordinates(2, 1.0, 1.0)),
              EqualsOptionalDesktopVector(499, 299));
}

TEST_F(CoordinateConverterTest, InvalidScreenId) {
  converter_.set_video_layout(BuildLayout(kSimpleLayout));

  EXPECT_EQ(converter_.ToGlobalAbsoluteCoordinate(
                BuildFractionalCoordinates(9, 0.5, 0.5)),
            std::nullopt);
}

TEST_F(CoordinateConverterTest, FallbackUsedIfNoScreenId) {
  converter_.set_video_layout(BuildLayout(kSimpleLayout));
  converter_.set_fallback_geometry(
      webrtc::DesktopRect::MakeXYWH(200, 100, 300, 200));

  EXPECT_THAT(converter_.ToGlobalAbsoluteCoordinate(
                  BuildFractionalCoordinates(std::nullopt, 0.5, 0.5)),
              EqualsOptionalDesktopVector(350, 200));
}

TEST_F(CoordinateConverterTest, NoScreenIdAndNoFallback) {
  converter_.set_video_layout(BuildLayout(kSimpleLayout));
  converter_.set_fallback_geometry({});

  EXPECT_EQ(converter_.ToGlobalAbsoluteCoordinate(
                BuildFractionalCoordinates(std::nullopt, 0.5, 0.5)),
            std::nullopt);
}

TEST_F(CoordinateConverterTest, Clamping) {
  converter_.set_video_layout(BuildLayout(kSimpleLayout));

  // Values outside [0, 1] should be clamped.
  EXPECT_THAT(converter_.ToGlobalAbsoluteCoordinate(
                  BuildFractionalCoordinates(1, -0.1, -0.1)),
              EqualsOptionalDesktopVector(0, 0));
  EXPECT_THAT(converter_.ToGlobalAbsoluteCoordinate(
                  BuildFractionalCoordinates(1, 1.1, 1.1)),
              EqualsOptionalDesktopVector(199, 99));
}

}  // namespace remoting::protocol
