// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/video_transformation.h"

#include <math.h>

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

enum class Flip { kNone, kVertical, kHorizontal, kBoth };

// Rotation by angle Θ is represented in the matrix as:
// [ cos(Θ), -sin(Θ)]
// [ sin(Θ),  cos(Θ)]
// a vertical flip is represented by the cosine's having opposite signs
// and a horizontal flip is represented by the sine's having the same sign.
constexpr void FromAngle(double angle_degrees, int32_t* matrix, Flip flip) {
  double f_matrix[4] = {0, 0, 0, 0};
  f_matrix[0] = f_matrix[3] = cos(angle_degrees * 3.1415926535 / 180.0);
  f_matrix[1] = f_matrix[2] = sin(angle_degrees * 3.1415926535 / 180.0);
  if (flip == Flip::kVertical || flip == Flip::kBoth)
    f_matrix[3] *= -1;
  if (flip == Flip::kVertical || flip == Flip::kNone)
    f_matrix[2] *= -1;

  matrix[0] = static_cast<int32_t>(round(f_matrix[0] * (1 << 16)));
  matrix[1] = static_cast<int32_t>(round(f_matrix[1] * (1 << 16)));
  matrix[3] = static_cast<int32_t>(round(f_matrix[2] * (1 << 16)));
  matrix[4] = static_cast<int32_t>(round(f_matrix[3] * (1 << 16)));
}

}  // namespace

class VideoTransformationTest : public testing::Test {
 public:
  VideoTransformationTest() = default;
  VideoTransformationTest(const VideoTransformationTest&) = delete;
  VideoTransformationTest& operator=(const VideoTransformationTest&) = delete;
  ~VideoTransformationTest() override = default;
};

TEST_F(VideoTransformationTest, ComputeRotationAngles) {
  int32_t matrix[9];

  // Standard 90 degree increments with no rotation all end up rotated normally
  FromAngle(0.0, matrix, Flip::kNone);
  ASSERT_EQ(VideoTransformation::FromFFmpegDisplayMatrix(matrix),
            VideoTransformation(VIDEO_ROTATION_0, false));

  FromAngle(90.0, matrix, Flip::kNone);
  ASSERT_EQ(VideoTransformation::FromFFmpegDisplayMatrix(matrix),
            VideoTransformation(VIDEO_ROTATION_90, false));

  FromAngle(180.0, matrix, Flip::kNone);
  ASSERT_EQ(VideoTransformation::FromFFmpegDisplayMatrix(matrix),
            VideoTransformation(VIDEO_ROTATION_180, false));

  FromAngle(270.0, matrix, Flip::kNone);
  ASSERT_EQ(VideoTransformation::FromFFmpegDisplayMatrix(matrix),
            VideoTransformation(VIDEO_ROTATION_270, false));

  // Non-right-angle rotations all get set to 0, since we can't render those
  // properly anyway.
  FromAngle(60.0, matrix, Flip::kNone);
  ASSERT_EQ(VideoTransformation::FromFFmpegDisplayMatrix(matrix),
            VideoTransformation(VIDEO_ROTATION_0, false));

  FromAngle(45.0, matrix, Flip::kNone);
  ASSERT_EQ(VideoTransformation::FromFFmpegDisplayMatrix(matrix),
            VideoTransformation(VIDEO_ROTATION_0, false));

  // Flips can cause weird things - ie a vertical flip + 180 rotation
  // is the same thing as a horizontal flip + no rotation!
  FromAngle(180.0, matrix, Flip::kVertical);
  ASSERT_EQ(VideoTransformation::FromFFmpegDisplayMatrix(matrix),
            VideoTransformation(VIDEO_ROTATION_0, true));

  // Vertical and horizontal flipping is the same as a 180 degree rotation
  FromAngle(0.0, matrix, Flip::kBoth);
  ASSERT_EQ(VideoTransformation::FromFFmpegDisplayMatrix(matrix),
            VideoTransformation(VIDEO_ROTATION_180, true));

  // An invalid matrix is always going to give no rotation or mirroring.
  for (int i = 0; i < 9; i++)
    matrix[i] = 78123;
  ASSERT_EQ(VideoTransformation::FromFFmpegDisplayMatrix(matrix),
            VideoTransformation(VIDEO_ROTATION_0, false));
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
