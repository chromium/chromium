// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "media/base/video_transformation.h"

#include <math.h>

#include <limits>

#include "base/logging.h"
#include "base/numerics/angle_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class VideoTransformationTest : public testing::Test {
 public:
  VideoTransformationTest() = default;
  VideoTransformationTest(const VideoTransformationTest&) = delete;
  VideoTransformationTest& operator=(const VideoTransformationTest&) = delete;
  ~VideoTransformationTest() override = default;
};

// List of matrices possible when limited to {0, 90, 180, 270} & hflip
// (mirrored). Verified via ffplay.
//
// MATRICES = {
//   "0":    [65536, 0, 0, 65536],
//   "0m":   [-65536, 0, 0, 65536],
//   "90":   [0, 65536, -65536, 0],
//   "90m":  [0, 65536, 65536, 0],
//   "180":  [-65536, 0, 0, -65536],
//   "180m": [65536, 0, 0, -65536],
//   "270":  [0, -65536, 65536, 0],
//   "270m": [0, -65536, -65536, 0]
// }
TEST_F(VideoTransformationTest, MatrixToVideoTransformation) {
  std::array<int32_t, 4> mat = {65536, 0, 0, 65536};
  auto t = VideoTransformation(mat);
  EXPECT_EQ(t.rotation, VIDEO_ROTATION_0);
  EXPECT_FALSE(t.mirrored);

  mat = {-65536, 0, 0, 65536};
  t = VideoTransformation(mat);
  EXPECT_EQ(t.rotation, VIDEO_ROTATION_0);
  EXPECT_TRUE(t.mirrored);

  mat = {0, 65536, -65536, 0};
  t = VideoTransformation(mat);
  EXPECT_EQ(t.rotation, VIDEO_ROTATION_90);
  EXPECT_FALSE(t.mirrored);

  mat = {0, 65536, 65536, 0};
  t = VideoTransformation(mat);
  EXPECT_EQ(t.rotation, VIDEO_ROTATION_90);
  EXPECT_TRUE(t.mirrored);

  mat = {-65536, 0, 0, -65536};
  t = VideoTransformation(mat);
  EXPECT_EQ(t.rotation, VIDEO_ROTATION_180);
  EXPECT_FALSE(t.mirrored);

  mat = {65536, 0, 0, -65536};
  t = VideoTransformation(mat);
  EXPECT_EQ(t.rotation, VIDEO_ROTATION_180);
  EXPECT_TRUE(t.mirrored);

  mat = {0, -65536, 65536, 0};
  t = VideoTransformation(mat);
  EXPECT_EQ(t.rotation, VIDEO_ROTATION_270);
  EXPECT_FALSE(t.mirrored);

  mat = {0, -65536, -65536, 0};
  t = VideoTransformation(mat);
  EXPECT_EQ(t.rotation, VIDEO_ROTATION_270);
  EXPECT_TRUE(t.mirrored);
}

TEST_F(VideoTransformationTest, ComputeMatrix) {
  // Standard 90 degree increments with no rotation all end up rotated normally
  VideoTransformation transformation = VideoTransformation(VIDEO_ROTATION_0);
  EXPECT_EQ(VideoTransformation(transformation.GetMatrix()),
            VideoTransformation(VIDEO_ROTATION_0, false));

  transformation = VideoTransformation(VIDEO_ROTATION_90, false);
  EXPECT_EQ(VideoTransformation(transformation.GetMatrix()),
            VideoTransformation(VIDEO_ROTATION_90, false));

  transformation = VideoTransformation(VIDEO_ROTATION_180);
  EXPECT_EQ(VideoTransformation(transformation.GetMatrix()),
            VideoTransformation(VIDEO_ROTATION_180, false));

  transformation = VideoTransformation(VIDEO_ROTATION_270);
  EXPECT_EQ(VideoTransformation(transformation.GetMatrix()),
            VideoTransformation(VIDEO_ROTATION_270, false));

  // Test the mirrored cases
  transformation = VideoTransformation(VIDEO_ROTATION_0, true);
  EXPECT_EQ(VideoTransformation(transformation.GetMatrix()),
            VideoTransformation(VIDEO_ROTATION_0, true));

  transformation = VideoTransformation(VIDEO_ROTATION_90, true);
  EXPECT_EQ(VideoTransformation(transformation.GetMatrix()),
            VideoTransformation(VIDEO_ROTATION_90, true));

  transformation = VideoTransformation(VIDEO_ROTATION_180, true);
  EXPECT_EQ(VideoTransformation(transformation.GetMatrix()),
            VideoTransformation(VIDEO_ROTATION_180, true));

  transformation = VideoTransformation(VIDEO_ROTATION_270, true);
  EXPECT_EQ(VideoTransformation(transformation.GetMatrix()),
            VideoTransformation(VIDEO_ROTATION_270, true));
}

TEST_F(VideoTransformationTest, ConstructFromAngle) {
  EXPECT_EQ(VideoTransformation(0.0, false),
            VideoTransformation(VIDEO_ROTATION_0, false));
  EXPECT_EQ(VideoTransformation(0.0, true),
            VideoTransformation(VIDEO_ROTATION_0, true));

  // 45 degrees rounds up. Try offset by multiples of 360, including negative.
  for (int base_turns = -2; base_turns <= 2; ++base_turns) {
    EXPECT_EQ(VideoTransformation(360 * base_turns + 44.9, false),
              VideoTransformation(VIDEO_ROTATION_0, false));
    EXPECT_EQ(VideoTransformation(360 * base_turns + 45.0, false),
              VideoTransformation(VIDEO_ROTATION_90, false));

    EXPECT_EQ(VideoTransformation(360 * base_turns + 90 + 44.9, false),
              VideoTransformation(VIDEO_ROTATION_90, false));
    EXPECT_EQ(VideoTransformation(360 * base_turns + 90 + 45.0, false),
              VideoTransformation(VIDEO_ROTATION_180, false));

    EXPECT_EQ(VideoTransformation(360 * base_turns + 180 + 44.9, false),
              VideoTransformation(VIDEO_ROTATION_180, false));
    EXPECT_EQ(VideoTransformation(360 * base_turns + 180 + 45.0, false),
              VideoTransformation(VIDEO_ROTATION_270, false));

    EXPECT_EQ(VideoTransformation(360 * base_turns + 270 + 44.9, false),
              VideoTransformation(VIDEO_ROTATION_270, false));
    EXPECT_EQ(VideoTransformation(360 * base_turns + 270 + 45.0, false),
              VideoTransformation(VIDEO_ROTATION_0, false));
  }

  // Exactly 232 degrees.
  EXPECT_EQ(VideoTransformation(std::numeric_limits<double>::lowest(), false),
            VideoTransformation(VIDEO_ROTATION_270, false));

  // Exactly 128 degrees.
  EXPECT_EQ(VideoTransformation(std::numeric_limits<double>::max(), false),
            VideoTransformation(VIDEO_ROTATION_90, false));
}

TEST_F(VideoTransformationTest, Add) {
  // kNoTransformation is the identity transformation.
  for (int rotation = 0; rotation < 360; rotation += 90) {
    VideoTransformation transformation =
        VideoTransformation(static_cast<VideoRotation>(rotation), false);
    EXPECT_EQ(transformation.add(kNoTransformation), transformation);
    EXPECT_EQ(kNoTransformation.add(transformation), transformation);

    transformation =
        VideoTransformation(static_cast<VideoRotation>(rotation), true);
    EXPECT_EQ(transformation.add(kNoTransformation), transformation);
    EXPECT_EQ(kNoTransformation.add(transformation), transformation);
  }

  // When the base transformation is not mirrored, delta rotation adds.
  EXPECT_EQ(VideoTransformation(VIDEO_ROTATION_90, false)
                .add(VideoTransformation(VIDEO_ROTATION_90, false)),
            VideoTransformation(VIDEO_ROTATION_180, false));
  EXPECT_EQ(VideoTransformation(VIDEO_ROTATION_90, false)
                .add(VideoTransformation(VIDEO_ROTATION_90, true)),
            VideoTransformation(VIDEO_ROTATION_180, true));
  EXPECT_EQ(VideoTransformation(VIDEO_ROTATION_90, false)
                .add(VideoTransformation(VIDEO_ROTATION_270, false)),
            VideoTransformation(VIDEO_ROTATION_0, false));
  EXPECT_EQ(VideoTransformation(VIDEO_ROTATION_90, false)
                .add(VideoTransformation(VIDEO_ROTATION_270, true)),
            VideoTransformation(VIDEO_ROTATION_0, true));

  // When the base transformation is mirrored, delta rotation is subtracted.
  EXPECT_EQ(VideoTransformation(VIDEO_ROTATION_90, true)
                .add(VideoTransformation(VIDEO_ROTATION_90, false)),
            VideoTransformation(VIDEO_ROTATION_0, true));
  EXPECT_EQ(VideoTransformation(VIDEO_ROTATION_90, true)
                .add(VideoTransformation(VIDEO_ROTATION_90, true)),
            VideoTransformation(VIDEO_ROTATION_0, false));
  EXPECT_EQ(VideoTransformation(VIDEO_ROTATION_90, true)
                .add(VideoTransformation(VIDEO_ROTATION_270, false)),
            VideoTransformation(VIDEO_ROTATION_180, true));
  EXPECT_EQ(VideoTransformation(VIDEO_ROTATION_90, true)
                .add(VideoTransformation(VIDEO_ROTATION_270, true)),
            VideoTransformation(VIDEO_ROTATION_180, false));
}

}  // namespace media
