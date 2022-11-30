// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/surface_texture_gl_owner.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class SurfaceTextureTransformTest : public testing::Test {
 public:
  SurfaceTextureTransformTest() {}
  ~SurfaceTextureTransformTest() override {}

 protected:
  gfx::Size GetRotatedSize(gfx::Size size, bool rotated) {
    if (rotated)
      return gfx::Size(size.height(), size.width());
    return size;
  }

  void DoTest(float matrix[16],
              gfx::Size rotated_visible_size,
              gfx::Size expected_coded_size,
              bool rotated) {
    gfx::Size coded_size;
    gfx::Rect visible_rect;
    ASSERT_TRUE(SurfaceTextureGLOwner::DecomposeTransform(
        matrix, rotated_visible_size, &coded_size, &visible_rect));

    EXPECT_EQ(coded_size, expected_coded_size);
    EXPECT_EQ(visible_rect.origin(), gfx::Point(0, 0));
    EXPECT_EQ(visible_rect.size(),
              GetRotatedSize(rotated_visible_size, rotated));
  }
};

TEST_F(SurfaceTextureTransformTest, Rotation0) {
  float matrix[16] = {1.000000, 0.000000,  0.000000, 0.000000,
                      0.000000, -0.990809, 0.000000, 0.000000,
                      0.000000, 0.000000,  1.000000, 0.000000,
                      0.000000, 0.991728,  0.000000, 1.000000};
  gfx::Size coded_size(1920, 1088);  // Has padding
  gfx::Size rotated_visible_size(1920, 1080);

  DoTest(matrix, rotated_visible_size, coded_size, false);
}

TEST_F(SurfaceTextureTransformTest, Rotation90) {
  float matrix[16] = {0.000000,  -0.990809, 0.000000, 0.000000,
                      -1.000000, 0.000000,  0.000000, 0.000000,
                      0.000000,  0.000000,  1.000000, 0.000000,
                      1.000000,  0.991728,  0.000000, 1.000000};
  gfx::Size coded_size(1920, 1088);  // Has padding
  gfx::Size rotated_visible_size(1080, 1920);

  DoTest(matrix, rotated_visible_size, coded_size, true);
}

TEST_F(SurfaceTextureTransformTest, Rotation180) {
  float matrix[16] = {-1.000000, 0.000000, 0.000000, 0.000000,
                      0.000000,  0.990809, 0.000000, 0.000000,
                      0.000000,  0.000000, 1.000000, 0.000000,
                      1.000000,  0.000919, 0.000000, 1.000000};
  gfx::Size coded_size(1920, 1088);  // Has padding
  gfx::Size rotated_visible_size(1920, 1080);

  DoTest(matrix, rotated_visible_size, coded_size, false);
}

TEST_F(SurfaceTextureTransformTest, Rotation270) {
  float matrix[16] = {0.000000, 0.990809, 0.000000, 0.000000,
                      1.000000, 0.000000, 0.000000, 0.000000,
                      0.000000, 0.000000, 1.000000, 0.000000,
                      0.000000, 0.000919, 0.000000, 1.000000};
  gfx::Size coded_size(1920, 1088);  // Has padding
  gfx::Size rotated_visible_size(1080, 1920);

  DoTest(matrix, rotated_visible_size, coded_size, true);
}

TEST_F(SurfaceTextureTransformTest, SmallSize) {
  float matrix[16] = {0.000000, 0.000000, 0.000000, 0.000000,
                      0.000000, 0.000000, 0.000000, 0.000000,
                      0.000000, 0.000000, 0.000000, 0.000000,
                      0.000000, 0.000000, 0.000000, 0.000000};
  // With video size less then 4x4 matrix might not make sense because of
  // shrinking crop rect by pixel from each side. It's important to not crash in
  // that case.
  gfx::Size landscape_size(4, 2);
  gfx::Size portrait_size(2, 4);

  // We assume coded size is the same as video size in this case.
  DoTest(matrix, landscape_size, landscape_size, false);
  DoTest(matrix, portrait_size, portrait_size, false);
}

// This tests case where matrix was created with |shrink_amount|=0, e.g no
// linear filtering.
TEST_F(SurfaceTextureTransformTest, NoShrink) {
  float matrix[16] = {0.986111, 0.000000,  0.000000, 0.000000,
                      0.000000, -1.000000, 0.000000, 0.000000,
                      0.000000, 0.000000,  1.000000, 0.000000,
                      0.000000, 1.000000,  0.000000, 1.000000};
  gfx::Size coded_size(432, 240);
  gfx::Size rotated_visible_size(426, 240);

  DoTest(matrix, rotated_visible_size, coded_size, false);
}

// This tests case where |shrink_amount|=0.5, e.g rgb codec size.
TEST_F(SurfaceTextureTransformTest, Shrink_05) {
  float matrix[16] = {0.986111, 0.000000,  0.000000, 0.000000,
                      0.000000, -1.000000, 0.000000, 0.000000,
                      0.000000, 0.000000,  1.000000, 0.000000,
                      0.001157, 1.000000,  0.000000, 1.000000};
  gfx::Size coded_size(432, 240);
  gfx::Size rotated_visible_size(427, 240);

  DoTest(matrix, rotated_visible_size, coded_size, false);
}

}  // namespace gpu
