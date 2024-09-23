/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/transforms/transform_operations.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/transforms/interpolated_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/matrix_3d_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/matrix_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/perspective_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/rotate_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/scale_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/skew_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/translate_transform_operation.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace blink {

static const TransformOperations kIdentityOperations;

static void EmpiricallyTestBounds(const TransformOperations& from,
                                  const TransformOperations& to,
                                  const double& min_progress,
                                  const double& max_progress) {
  gfx::BoxF box(200, 500, 100, 100, 300, 200);
  gfx::BoxF bounds;

  EXPECT_TRUE(
      to.BlendedBoundsForBox(box, from, min_progress, max_progress, &bounds));
  bool first_time = true;

  gfx::BoxF empirical_bounds;
  static const size_t kNumSteps = 10;
  for (size_t step = 0; step < kNumSteps; ++step) {
    float t = step / (kNumSteps - 1);
    t = min_progress + (max_progress - min_progress) * t;
    TransformOperations operations = from.Blend(to, t);
    gfx::Transform matrix;
    operations.Apply(gfx::SizeF(0, 0), matrix);
    gfx::BoxF transformed = matrix.MapBox(box);

    if (first_time)
      empirical_bounds = transformed;
    else
      empirical_bounds.Union(transformed);
    first_time = false;
  }

  // Check whether |bounds| contains |empirical_bounds|.
  gfx::BoxF expanded_bounds = bounds;
  float tolerance =
      std::max({bounds.width(), bounds.height(), bounds.depth()}) * 1e-6f;
  expanded_bounds.ExpandTo(empirical_bounds);
  EXPECT_BOXF_NEAR(bounds, expanded_bounds, tolerance)
      << " Expected to contain: " << empirical_bounds.ToString();
}

TEST(TransformOperationsTest, AbsoluteAnimatedTranslatedBoundsTest) {
  TransformOperations from_ops;
  TransformOperations to_ops;
  from_ops.Operations().push_back(
      MakeGarbageCollected<TranslateTransformOperation>(
          Length::Fixed(-30), Length::Fixed(20), 15,
          TransformOperation::kTranslate3D));
  to_ops.Operations().push_back(
      MakeGarbageCollected<TranslateTransformOperation>(
          Length::Fixed(10), Length::Fixed(10), 200,
          TransformOperation::kTranslate3D));
  gfx::BoxF box(0, 0, 0, 10, 10, 10);
  gfx::BoxF bounds;

  EXPECT_TRUE(
      to_ops.BlendedBoundsForBox(box, kIdentityOperations, 0, 1, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(0, 0, 0, 20, 20, 210), bounds);

  EXPECT_TRUE(
      kIdentityOperations.BlendedBoundsForBox(box, to_ops, 0, 1, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(0, 0, 0, 20, 20, 210), bounds);

  EXPECT_TRUE(
      kIdentityOperations.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(-30, 0, 0, 40, 30, 25), bounds);

  EXPECT_TRUE(to_ops.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(-30, 10, 15, 50, 20, 195), bounds);

  EXPECT_TRUE(to_ops.BlendedBoundsForBox(box, from_ops, -0.5, 1.25, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(-50, 7.5, -77.5, 80, 27.5, 333.75), bounds);
}

TEST(TransformOperationsTest, EmpiricalAnimatedTranslatedBoundsTest) {
  float test_transforms[][2][3] = {{{0, 0, 0}, {10, 10, 0}},
                                   {{-100, 202.5, -32.6}, {43.2, 56.1, 89.75}},
                                   {{43.2, 56.1, 89.75}, {-100, 202.5, -32.6}}};

  // All progressions for animations start and end at 0, 1 respectively,
  // we can go outside of these bounds, but will always at least contain
  // [0,1].
  float progress[][2] = {{0, 1}, {-.25, 1.25}};

  for (size_t i = 0; i < std::size(test_transforms); ++i) {
    for (size_t j = 0; j < std::size(progress); ++j) {
      TransformOperations from_ops;
      TransformOperations to_ops;
      from_ops.Operations().push_back(
          MakeGarbageCollected<TranslateTransformOperation>(
              Length::Fixed(test_transforms[i][0][0]),
              Length::Fixed(test_transforms[i][0][1]), test_transforms[i][0][2],
              TransformOperation::kTranslate3D));
      to_ops.Operations().push_back(
          MakeGarbageCollected<TranslateTransformOperation>(
              Length::Fixed(test_transforms[i][1][0]),
              Length::Fixed(test_transforms[i][1][1]), test_transforms[i][1][2],
              TransformOperation::kTranslate3D));
      EmpiricallyTestBounds(from_ops, to_ops, progress[j][0], progress[j][1]);
    }
  }
}

TEST(TransformOperationsTest, AbsoluteAnimatedScaleBoundsTest) {
  TransformOperations from_ops;
  TransformOperations to_ops;
  from_ops.Operations().push_back(MakeGarbageCollected<ScaleTransformOperation>(
      4, -3, TransformOperation::kScale));
  to_ops.Operations().push_back(MakeGarbageCollected<ScaleTransformOperation>(
      5, 2, TransformOperation::kScale));

  gfx::BoxF box(0, 0, 0, 10, 10, 10);
  gfx::BoxF bounds;

  EXPECT_TRUE(
      to_ops.BlendedBoundsForBox(box, kIdentityOperations, 0, 1, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(0, 0, 0, 50, 20, 10), bounds);

  EXPECT_TRUE(
      kIdentityOperations.BlendedBoundsForBox(box, to_ops, 0, 1, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(0, 0, 0, 50, 20, 10), bounds);

  EXPECT_TRUE(
      kIdentityOperations.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(0, -30, 0, 40, 40, 10), bounds);

  EXPECT_TRUE(to_ops.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(0, -30, 0, 50, 50, 10), bounds);

  EXPECT_TRUE(to_ops.BlendedBoundsForBox(box, from_ops, -0.5, 1.25, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(0, -55, 0, 52.5, 87.5, 10), bounds);
}

TEST(TransformOperationsTest, EmpiricalAnimatedScaleBoundsTest) {
  float test_transforms[][2][3] = {{{1, 1, 1}, {10, 10, -32}},
                                   {{1, 2, 5}, {-1, -2, -4}},
                                   {{0, 0, 0}, {1, 2, 3}},
                                   {{0, 0, 0}, {0, 0, 0}}};

  // All progressions for animations start and end at 0, 1 respectively,
  // we can go outside of these bounds, but will always at least contain
  // [0,1].
  float progress[][2] = {{0, 1}, {-.25f, 1.25f}};

  for (size_t i = 0; i < std::size(test_transforms); ++i) {
    for (size_t j = 0; j < std::size(progress); ++j) {
      TransformOperations from_ops;
      TransformOperations to_ops;
      from_ops.Operations().push_back(
          MakeGarbageCollected<TranslateTransformOperation>(
              Length::Fixed(test_transforms[i][0][0]),
              Length::Fixed(test_transforms[i][0][1]), test_transforms[i][0][2],
              TransformOperation::kTranslate3D));
      to_ops.Operations().push_back(
          MakeGarbageCollected<TranslateTransformOperation>(
              Length::Fixed(test_transforms[i][1][0]),
              Length::Fixed(test_transforms[i][1][1]), test_transforms[i][1][2],
              TransformOperation::kTranslate3D));
      EmpiricallyTestBounds(from_ops, to_ops, progress[j][0], progress[j][1]);
    }
  }
}

TEST(TransformOperationsTest, AbsoluteAnimatedRotationBounds) {
  TransformOperations from_ops;
  TransformOperations to_ops;
  from_ops.Operations().push_back(
      MakeGarbageCollected<RotateTransformOperation>(
          0, TransformOperation::kRotate));
  to_ops.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      360, TransformOperation::kRotate));
  float sqrt2 = sqrt(2.0f);
  gfx::BoxF box(-sqrt2, -sqrt2, 0, sqrt2, sqrt2, 0);
  gfx::BoxF bounds;

  // Since we're rotating 360 degrees, any box with dimensions between 0 and
  // 2 * sqrt(2) should give the same result.
  float sizes[] = {0, 0.1f, sqrt2, 2 * sqrt2};
  to_ops.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds);
  for (float size : sizes) {
    box.set_size(size, size, 0);

    EXPECT_TRUE(to_ops.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds));
    EXPECT_BOXF_EQ(gfx::BoxF(-2, -2, 0, 4, 4, 0), bounds);
  }
}

TEST(TransformOperationsTest, AbsoluteAnimatedExtremeRotationBounds) {
  // If the normal is off-plane, we can have up to 6 exrema (min/max in each
  // dimension between) the endpoints of the arg. This makes sure we are
  // catching all 6.
  TransformOperations from_ops;
  TransformOperations to_ops;
  from_ops.Operations().push_back(
      MakeGarbageCollected<RotateTransformOperation>(
          1, 1, 1, 30, TransformOperation::kRotate3D));
  to_ops.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      1, 1, 1, 390, TransformOperation::kRotate3D));

  gfx::BoxF box(1, 0, 0, 0, 0, 0);
  gfx::BoxF bounds;
  float min = -1 / 3.0f;
  float max = 1;
  float size = max - min;
  EXPECT_TRUE(to_ops.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(min, min, min, size, size, size), bounds);
}

TEST(TransformOperationsTest, AbsoluteAnimatedAxisRotationBounds) {
  // We can handle rotations about a single axis. If the axes are different,
  // we revert to matrix interpolation for which inflated bounds cannot be
  // computed.
  TransformOperations from_ops;
  TransformOperations to_same;
  TransformOperations to_opposite;
  TransformOperations to_different;
  from_ops.Operations().push_back(
      MakeGarbageCollected<RotateTransformOperation>(
          1, 1, 1, 30, TransformOperation::kRotate3D));
  to_same.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      1, 1, 1, 390, TransformOperation::kRotate3D));
  to_opposite.Operations().push_back(
      MakeGarbageCollected<RotateTransformOperation>(
          -1, -1, -1, 390, TransformOperation::kRotate3D));
  to_different.Operations().push_back(
      MakeGarbageCollected<RotateTransformOperation>(
          1, 3, 1, 390, TransformOperation::kRotate3D));

  gfx::BoxF box(1, 0, 0, 0, 0, 0);
  gfx::BoxF bounds;
  EXPECT_TRUE(to_same.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds));
  EXPECT_FALSE(to_opposite.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds));
  EXPECT_FALSE(to_different.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds));
}

TEST(TransformOperationsTest, AbsoluteAnimatedOnAxisRotationBounds) {
  // If we rotate a point that is on the axis of rotation, the box should not
  // change at all.
  TransformOperations from_ops;
  TransformOperations to_ops;
  from_ops.Operations().push_back(
      MakeGarbageCollected<RotateTransformOperation>(
          1, 1, 1, 30, TransformOperation::kRotate3D));
  to_ops.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      1, 1, 1, 390, TransformOperation::kRotate3D));

  gfx::BoxF box(1, 1, 1, 0, 0, 0);
  gfx::BoxF bounds;

  EXPECT_TRUE(to_ops.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds));
  EXPECT_BOXF_EQ(box, bounds);
}

// This would have been best as anonymous structs, but |std::size|
// does not get along with anonymous structs once we support C++11
// std::size will automatically support anonymous structs.

struct ProblematicAxisTest {
  double x;
  double y;
  double z;
  gfx::BoxF expected;
};

TEST(TransformOperationsTest, AbsoluteAnimatedProblematicAxisRotationBounds) {
  // Zeros in the components of the axis of rotation turned out to be tricky to
  // deal with in practice. This function tests some potentially problematic
  // axes to ensure sane behavior.

  // Some common values used in the expected boxes.
  float dim1 = 0.292893f;
  float dim2 = sqrt(2.0f);
  float dim3 = 2 * dim2;

  ProblematicAxisTest tests[] = {
      {0, 0, 0, gfx::BoxF(1, 1, 1, 0, 0, 0)},
      {1, 0, 0, gfx::BoxF(1, -dim2, -dim2, 0, dim3, dim3)},
      {0, 1, 0, gfx::BoxF(-dim2, 1, -dim2, dim3, 0, dim3)},
      {0, 0, 1, gfx::BoxF(-dim2, -dim2, 1, dim3, dim3, 0)},
      {1, 1, 0, gfx::BoxF(dim1, dim1, -1, dim2, dim2, 2)},
      {0, 1, 1, gfx::BoxF(-1, dim1, dim1, 2, dim2, dim2)},
      {1, 0, 1, gfx::BoxF(dim1, -1, dim1, dim2, 2, dim2)}};

  for (size_t i = 0; i < std::size(tests); ++i) {
    float x = tests[i].x;
    float y = tests[i].y;
    float z = tests[i].z;
    TransformOperations from_ops;
    from_ops.Operations().push_back(
        MakeGarbageCollected<RotateTransformOperation>(
            x, y, z, 0, TransformOperation::kRotate3D));
    TransformOperations to_ops;
    to_ops.Operations().push_back(
        MakeGarbageCollected<RotateTransformOperation>(
            x, y, z, 360, TransformOperation::kRotate3D));
    gfx::BoxF box(1, 1, 1, 0, 0, 0);
    gfx::BoxF bounds;

    EXPECT_TRUE(to_ops.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds));
    EXPECT_BOXF_NEAR(tests[i].expected, bounds, 1e-6f);
  }
}

TEST(TransformOperationsTest, BlendedBoundsForRotationEmpiricalTests) {
  float axes[][3] = {{1, 1, 1},  {-1, -1, -1}, {-1, 2, 3},  {1, -2, 3},
                     {0, 0, 0},  {1, 0, 0},    {0, 1, 0},   {0, 0, 1},
                     {1, 1, 0},  {0, 1, 1},    {1, 0, 1},   {-1, 0, 0},
                     {0, -1, 0}, {0, 0, -1},   {-1, -1, 0}, {0, -1, -1},
                     {-1, 0, -1}};

  float angles[][2] = {{5, 100},     {10, 5},       {0, 360},   {20, 180},
                       {-20, -180},  {180, -220},   {220, 320}, {1020, 1120},
                       {-3200, 120}, {-9000, -9050}};

  float progress[][2] = {{0, 1}, {-0.25f, 1.25f}};

  for (size_t i = 0; i < std::size(axes); ++i) {
    for (size_t j = 0; j < std::size(angles); ++j) {
      for (size_t k = 0; k < std::size(progress); ++k) {
        float x = axes[i][0];
        float y = axes[i][1];
        float z = axes[i][2];

        TransformOperations from_ops;
        TransformOperations to_ops;

        from_ops.Operations().push_back(
            MakeGarbageCollected<RotateTransformOperation>(
                x, y, z, angles[j][0], TransformOperation::kRotate3D));
        to_ops.Operations().push_back(
            MakeGarbageCollected<RotateTransformOperation>(
                x, y, z, angles[j][1], TransformOperation::kRotate3D));
        EmpiricallyTestBounds(from_ops, to_ops, progress[k][0], progress[k][1]);
      }
    }
  }
}

TEST(TransformOperationsTest, AbsoluteAnimatedPerspectiveBoundsTest) {
  TransformOperations from_ops;
  TransformOperations to_ops;
  from_ops.Operations().push_back(
      MakeGarbageCollected<PerspectiveTransformOperation>(20));
  to_ops.Operations().push_back(
      MakeGarbageCollected<PerspectiveTransformOperation>(40));
  gfx::BoxF box(0, 0, 0, 10, 10, 10);
  gfx::BoxF bounds;
  to_ops.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds);
  EXPECT_BOXF_EQ(gfx::BoxF(0, 0, 0, 20, 20, 20), bounds);

  from_ops.BlendedBoundsForBox(box, to_ops, -0.25, 1.25, &bounds);
  // The perspective range was [20, 40] and blending will extrapolate that to
  // [17.777..., 53.333...].  The cube has w/h/d of 10 and the observer is at
  // 17.777..., so the face closest the observer is 17.777...-10=7.777...
  double projected_size = 10.0 / 7.7777778 * 17.7777778;
  EXPECT_BOXF_EQ(
      gfx::BoxF(0, 0, 0, projected_size, projected_size, projected_size),
      bounds);
}

TEST(TransformOperationsTest, EmpiricalAnimatedPerspectiveBoundsTest) {
  float depths[][2] = {
      {600, 400}, {800, 1000}, {800, std::numeric_limits<float>::infinity()}};

  float progress[][2] = {{0, 1}, {-0.1f, 1.1f}};

  for (size_t i = 0; i < std::size(depths); ++i) {
    for (size_t j = 0; j < std::size(progress); ++j) {
      TransformOperations from_ops;
      TransformOperations to_ops;

      from_ops.Operations().push_back(
          MakeGarbageCollected<PerspectiveTransformOperation>(depths[i][0]));
      to_ops.Operations().push_back(
          MakeGarbageCollected<PerspectiveTransformOperation>(depths[i][1]));

      EmpiricallyTestBounds(from_ops, to_ops, progress[j][0], progress[j][1]);
    }
  }
}

TEST(TransformOperationsTest, AnimatedSkewBoundsTest) {
  TransformOperations from_ops;
  TransformOperations to_ops;
  from_ops.Operations().push_back(MakeGarbageCollected<SkewTransformOperation>(
      -45, 0, TransformOperation::kSkew));
  to_ops.Operations().push_back(MakeGarbageCollected<SkewTransformOperation>(
      0, 45, TransformOperation::kSkew));
  gfx::BoxF box(0, 0, 0, 10, 10, 10);
  gfx::BoxF bounds;

  to_ops.BlendedBoundsForBox(box, kIdentityOperations, 0, 1, &bounds);
  EXPECT_BOXF_EQ(gfx::BoxF(0, 0, 0, 10, 20, 10), bounds);

  kIdentityOperations.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds);
  EXPECT_BOXF_EQ(gfx::BoxF(-10, 0, 0, 20, 10, 10), bounds);

  to_ops.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds);
  EXPECT_BOXF_EQ(gfx::BoxF(-10, 0, 0, 20, 20, 10), bounds);

  from_ops.BlendedBoundsForBox(box, to_ops, 0, 1, &bounds);
  EXPECT_BOXF_EQ(gfx::BoxF(-10, 0, 0, 20, 20, 10), bounds);
}

TEST(TransformOperationsTest, NonCommutativeRotations) {
  TransformOperations from_ops;
  from_ops.Operations().push_back(
      MakeGarbageCollected<RotateTransformOperation>(
          1, 0, 0, 0, TransformOperation::kRotate3D));
  from_ops.Operations().push_back(
      MakeGarbageCollected<RotateTransformOperation>(
          0, 1, 0, 0, TransformOperation::kRotate3D));
  TransformOperations to_ops;
  to_ops.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      1, 0, 0, 45, TransformOperation::kRotate3D));
  to_ops.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      0, 1, 0, 135, TransformOperation::kRotate3D));

  gfx::BoxF box(0, 0, 0, 1, 1, 1);
  gfx::BoxF bounds;

  double min_progress = 0;
  double max_progress = 1;
  EXPECT_TRUE(to_ops.BlendedBoundsForBox(box, from_ops, min_progress,
                                         max_progress, &bounds));

  TransformOperations operations = to_ops.Blend(from_ops, max_progress);
  gfx::Transform blended_transform;
  operations.Apply(gfx::SizeF(0, 0), blended_transform);

  gfx::Point3F blended_point(0.9f, 0.9f, 0);
  blended_point = blended_transform.MapPoint(blended_point);
  gfx::BoxF expanded_bounds = bounds;
  expanded_bounds.ExpandTo(blended_point);

  EXPECT_BOXF_EQ(bounds, expanded_bounds);
}

TEST(TransformOperationsTest, NonInvertibleBlendTest) {
  TransformOperations from_ops;
  TransformOperations to_ops;

  from_ops.Operations().push_back(
      MakeGarbageCollected<TranslateTransformOperation>(
          Length::Fixed(5), Length::Fixed(-5), TransformOperation::kTranslate));
  to_ops.Operations().push_back(
      MakeGarbageCollected<MatrixTransformOperation>(0, 0, 0, 0, 0, 0));

  EXPECT_EQ(from_ops, to_ops.Blend(from_ops, 0.25));
  EXPECT_EQ(to_ops, to_ops.Blend(from_ops, 0.5));
  EXPECT_EQ(to_ops, to_ops.Blend(from_ops, 0.75));
}

TEST(TransformOperationsTest, AbsoluteSequenceBoundsTest) {
  TransformOperations from_ops;
  TransformOperations to_ops;

  from_ops.Operations().push_back(
      MakeGarbageCollected<TranslateTransformOperation>(
          Length::Fixed(1), Length::Fixed(-5), 1,
          TransformOperation::kTranslate3D));
  from_ops.Operations().push_back(MakeGarbageCollected<ScaleTransformOperation>(
      -1, 2, 3, TransformOperation::kScale3D));
  from_ops.Operations().push_back(
      MakeGarbageCollected<TranslateTransformOperation>(
          Length::Fixed(2), Length::Fixed(4), -1,
          TransformOperation::kTranslate3D));

  to_ops.Operations().push_back(
      MakeGarbageCollected<TranslateTransformOperation>(
          Length::Fixed(13), Length::Fixed(-1), 5,
          TransformOperation::kTranslate3D));
  to_ops.Operations().push_back(MakeGarbageCollected<ScaleTransformOperation>(
      -3, -2, 5, TransformOperation::kScale3D));
  to_ops.Operations().push_back(
      MakeGarbageCollected<TranslateTransformOperation>(
          Length::Fixed(6), Length::Fixed(-2), 3,
          TransformOperation::kTranslate3D));

  gfx::BoxF box(1, 2, 3, 4, 4, 4);
  gfx::BoxF bounds;

  EXPECT_TRUE(to_ops.BlendedBoundsForBox(box, from_ops, -0.5, 1.5, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(-57, -59, -1, 76, 112, 80), bounds);

  EXPECT_TRUE(to_ops.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(-32, -25, 7, 42, 44, 48), bounds);

  EXPECT_TRUE(
      to_ops.BlendedBoundsForBox(box, kIdentityOperations, 0, 1, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(-33, -13, 3, 57, 19, 52), bounds);

  EXPECT_TRUE(
      kIdentityOperations.BlendedBoundsForBox(box, from_ops, 0, 1, &bounds));
  EXPECT_BOXF_EQ(gfx::BoxF(-7, -3, 2, 15, 23, 20), bounds);
}

TEST(TransformOperationsTest, ZoomTest) {
  double zoom_factor = 1.25;

  gfx::Point3F original_point(2, 3, 4);

  TransformOperations ops;
  ops.Operations().push_back(MakeGarbageCollected<TranslateTransformOperation>(
      Length::Fixed(1), Length::Fixed(2), 3, TransformOperation::kTranslate3D));
  ops.Operations().push_back(
      MakeGarbageCollected<PerspectiveTransformOperation>(1234));
  ops.Operations().push_back(
      MakeGarbageCollected<Matrix3DTransformOperation>(gfx::Transform::ColMajor(
          1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)));

  // Apply unzoomed ops to unzoomed units, then zoom in
  gfx::Point3F unzoomed_point = original_point;
  TransformOperations unzoomed_ops = ops;
  gfx::Transform unzoomed_matrix;
  ops.Apply(gfx::SizeF(0, 0), unzoomed_matrix);
  gfx::Point3F result1 = unzoomed_matrix.MapPoint(unzoomed_point);
  result1.Scale(zoom_factor, zoom_factor, zoom_factor);

  // Apply zoomed ops to zoomed units
  gfx::Point3F zoomed_point = original_point;
  zoomed_point.Scale(zoom_factor, zoom_factor, zoom_factor);
  TransformOperations zoomed_ops = ops.Zoom(zoom_factor);
  gfx::Transform zoomed_matrix;
  zoomed_ops.Apply(gfx::SizeF(0, 0), zoomed_matrix);
  gfx::Point3F result2 = zoomed_matrix.MapPoint(zoomed_point);

  EXPECT_POINT3F_EQ(result1, result2);
}

TEST(TransformOperationsTest, PerspectiveOpsTest) {
  TransformOperations ops;
  EXPECT_FALSE(ops.HasPerspective());
  EXPECT_FALSE(ops.HasNonPerspective3DOperation());
  EXPECT_FALSE(ops.HasNonTrivial3DComponent());

  ops.Operations().push_back(MakeGarbageCollected<TranslateTransformOperation>(
      Length::Fixed(1), Length::Fixed(2), TransformOperation::kTranslate));
  EXPECT_FALSE(ops.HasPerspective());
  EXPECT_FALSE(ops.HasNonPerspective3DOperation());
  EXPECT_FALSE(ops.HasNonTrivial3DComponent());

  ops.Operations().push_back(
      MakeGarbageCollected<PerspectiveTransformOperation>(1234));
  EXPECT_TRUE(ops.HasPerspective());
  EXPECT_FALSE(ops.HasNonPerspective3DOperation());
  EXPECT_FALSE(ops.HasNonTrivial3DComponent());

  ops.Operations().push_back(MakeGarbageCollected<TranslateTransformOperation>(
      Length::Fixed(1), Length::Fixed(2), 3, TransformOperation::kTranslate3D));
  EXPECT_TRUE(ops.HasPerspective());
  EXPECT_TRUE(ops.HasNonPerspective3DOperation());
  EXPECT_TRUE(ops.HasNonTrivial3DComponent());
}

TEST(TransformOperationsTest, CanBlendWithSkewTest) {
  TransformOperations ops_x, ops_y, ops_skew, ops_skew2;
  ops_x.Operations().push_back(MakeGarbageCollected<SkewTransformOperation>(
      45, 0, TransformOperation::kSkewX));
  ops_y.Operations().push_back(MakeGarbageCollected<SkewTransformOperation>(
      0, 45, TransformOperation::kSkewY));
  ops_skew.Operations().push_back(MakeGarbageCollected<SkewTransformOperation>(
      45, 0, TransformOperation::kSkew));
  ops_skew2.Operations().push_back(MakeGarbageCollected<SkewTransformOperation>(
      0, 45, TransformOperation::kSkew));

  EXPECT_TRUE(ops_x.Operations()[0]->CanBlendWith(*ops_x.Operations()[0]));
  EXPECT_TRUE(ops_y.Operations()[0]->CanBlendWith(*ops_y.Operations()[0]));

  EXPECT_FALSE(ops_x.Operations()[0]->CanBlendWith(*ops_y.Operations()[0]));
  EXPECT_FALSE(ops_x.Operations()[0]->CanBlendWith(*ops_skew.Operations()[0]));
  EXPECT_FALSE(ops_y.Operations()[0]->CanBlendWith(*ops_skew.Operations()[0]));

  EXPECT_TRUE(
      ops_skew.Operations()[0]->CanBlendWith(*ops_skew2.Operations()[0]));

  ASSERT_TRUE(IsA<SkewTransformOperation>(
      *ops_skew.Blend(ops_skew2, 0.5).Operations()[0]));
  ASSERT_TRUE(IsA<Matrix3DTransformOperation>(
      *ops_x.Blend(ops_y, 0.5).Operations()[0]));
}

TEST(TransformOperationsTest, CanBlendWithMatrixTest) {
  TransformOperations ops_a, ops_b;
  ops_a.Operations().push_back(
      MakeGarbageCollected<MatrixTransformOperation>(1, 0, 0, 1, 0, 0));
  ops_a.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      0, TransformOperation::kRotate));
  ops_b.Operations().push_back(
      MakeGarbageCollected<MatrixTransformOperation>(2, 0, 0, 2, 0, 0));
  ops_b.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      360, TransformOperation::kRotate));

  EXPECT_TRUE(ops_a.Operations()[0]->CanBlendWith(*ops_b.Operations()[0]));

  TransformOperations ops_blended = ops_a.Blend(ops_b, 0.5);
  ASSERT_EQ(ops_blended.Operations().size(), 2u);
  ASSERT_TRUE(IsA<MatrixTransformOperation>(*ops_blended.Operations()[0]));
  ASSERT_TRUE(IsA<RotateTransformOperation>(*ops_blended.Operations()[1]));
  EXPECT_EQ(To<RotateTransformOperation>(*ops_blended.Operations()[1]).Angle(),
            180.0);
}

TEST(TransformOperationsTest, CanBlendWithMatrix3DTest) {
  TransformOperations ops_a, ops_b;
  ops_a.Operations().push_back(MakeGarbageCollected<Matrix3DTransformOperation>(
      gfx::Transform::Affine(1, 0, 0, 1, 0, 0)));
  ops_a.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      0, TransformOperation::kRotate));
  ops_b.Operations().push_back(MakeGarbageCollected<Matrix3DTransformOperation>(
      gfx::Transform::Affine(2, 0, 0, 2, 0, 0)));
  ops_b.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      360, TransformOperation::kRotate));

  EXPECT_TRUE(ops_a.Operations()[0]->CanBlendWith(*ops_b.Operations()[0]));

  TransformOperations ops_blended = ops_a.Blend(ops_b, 0.5);
  ASSERT_EQ(ops_blended.Operations().size(), 2u);
  ASSERT_TRUE(IsA<Matrix3DTransformOperation>(*ops_blended.Operations()[0]));
  ASSERT_TRUE(IsA<RotateTransformOperation>(*ops_blended.Operations()[1]));
  EXPECT_EQ(To<RotateTransformOperation>(*ops_blended.Operations()[1]).Angle(),
            180.0);
}

TEST(TransformOperationsTest, InterpolatedTransformBlendIdentityTest) {
  // When interpolating transform lists of differing lengths, the length of the
  // shorter list behaves as if it is padded with identity transforms.
  // The Blend method accepts a null from operation when blending to/from an
  // identity transform, with the direction of interpolation controlled by.
  // the blend_to_identity parameter.
  // This test verifies the correctness of interpolating between a deferred,
  // box-size-dependent matrix interpolation and an empty transform list in
  // both directions.
  TransformOperations ops_a, ops_b, ops_empty;
  ops_a.Operations().push_back(
      MakeGarbageCollected<TranslateTransformOperation>(
          Length::Percent(100), Length::Fixed(0),
          TransformOperation::kTranslate));
  ops_b.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      90, TransformOperation::kRotate));

  // Equivalent to translateX(50%) rotate(45deg) but a deferred interpolation
  TransformOperations ops_c = ops_a.Blend(ops_b, 0.5);
  ASSERT_EQ(ops_c.Operations().size(), 1u);
  ASSERT_TRUE(IsA<InterpolatedTransformOperation>(*ops_c.Operations()[0]));
  EXPECT_EQ(ops_c.BoxSizeDependencies(), TransformOperation::kDependsWidth);

  // Both should be the same and equal to translateX(12.5%) rotate(11.25deg);
  TransformOperations ops_d1 = ops_c.Blend(ops_empty, 0.25);
  TransformOperations ops_d2 = ops_empty.Blend(ops_c, 0.75);

  TransformOperations ops_d3;
  ops_d3.Operations().push_back(
      MakeGarbageCollected<TranslateTransformOperation>(
          Length::Percent(12.5), Length::Fixed(0),
          TransformOperation::kTranslate));
  ops_d3.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      11.25, TransformOperation::kRotate));

  const gfx::SizeF box_size(100, 100);
  gfx::Transform mat_d1, mat_d2, mat_d3;
  ops_d1.Apply(box_size, mat_d1);
  ops_d2.Apply(box_size, mat_d2);
  ops_d3.Apply(box_size, mat_d3);

  EXPECT_TRANSFORM_EQ(mat_d1, mat_d2);
  EXPECT_TRANSFORM_EQ(mat_d1, mat_d3);
  EXPECT_TRANSFORM_EQ(mat_d2, mat_d3);
}

TEST(TransformOperationsTest, BlendPercentPrefixTest) {
  TransformOperations ops_a, ops_b;
  ops_a.Operations().push_back(
      MakeGarbageCollected<TranslateTransformOperation>(
          Length::Percent(100), Length::Fixed(0),
          TransformOperation::kTranslate));
  ops_a.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      180, TransformOperation::kRotate));

  ops_b.Operations().push_back(
      MakeGarbageCollected<TranslateTransformOperation>(
          Length::Fixed(0), Length::Percent(50),
          TransformOperation::kTranslate));
  ops_b.Operations().push_back(MakeGarbageCollected<ScaleTransformOperation>(
      2, 2, TransformOperation::kScale));

  EXPECT_EQ(ops_a.BoxSizeDependencies(), TransformOperation::kDependsWidth);
  EXPECT_EQ(ops_a.BoxSizeDependencies(1), TransformOperation::kDependsNone);
  EXPECT_EQ(ops_b.BoxSizeDependencies(), TransformOperation::kDependsHeight);
  EXPECT_EQ(ops_b.BoxSizeDependencies(1), TransformOperation::kDependsNone);

  TransformOperations ops_c = ops_a.Blend(ops_b, 0.5);
  EXPECT_EQ(ops_c.BoxSizeDependencies(), TransformOperation::kDependsBoth);
  ASSERT_EQ(ops_c.Operations().size(), 2u);
  ASSERT_TRUE(IsA<TranslateTransformOperation>(*ops_c.Operations()[0]));

  // Even though both transform lists contain percents, the matrix interpolated
  // part does not, so it should interpolate to a matrix and not defer to an
  // InterpolatedTransformOperation.
  ASSERT_TRUE(IsA<Matrix3DTransformOperation>(*ops_c.Operations()[1]));
  gfx::Transform mat_c =
      To<Matrix3DTransformOperation>(*ops_c.Operations()[1]).Matrix();

  auto* translate_ref = MakeGarbageCollected<TranslateTransformOperation>(
      Length::Percent(50), Length::Percent(25), TransformOperation::kTranslate);
  // scale(1.5) rotate(90deg)
  auto matrix_ref = gfx::Transform::Affine(0, 1.5, -1.5, 0, 0, 0);
  EXPECT_EQ(*ops_c.Operations()[0], *translate_ref);
  EXPECT_TRANSFORM_NEAR(mat_c, matrix_ref, 1e-15);
}

TEST(TransformOperationsTest, SizeDependenciesCombineTest) {
  TransformOperations ops;
  ops.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      90, TransformOperation::kRotate));
  EXPECT_EQ(ops.BoxSizeDependencies(), TransformOperation::kDependsNone);

  ops.Operations().push_back(MakeGarbageCollected<TranslateTransformOperation>(
      Length::Fixed(0), Length::Percent(50), TransformOperation::kTranslate));
  EXPECT_EQ(ops.BoxSizeDependencies(), TransformOperation::kDependsHeight);

  ops.Operations().push_back(MakeGarbageCollected<TranslateTransformOperation>(
      Length::Percent(100), Length::Fixed(0), TransformOperation::kTranslate));
  EXPECT_EQ(ops.Operations()[2]->BoxSizeDependencies(),
            TransformOperation::kDependsWidth);
  EXPECT_EQ(ops.BoxSizeDependencies(), TransformOperation::kDependsBoth);
}

// https://crbug.com/1155018
TEST(TransformOperationsTest, OutOfRangePercentage) {
  TransformOperations ops;
  ops.Operations().push_back(MakeGarbageCollected<TranslateTransformOperation>(
      Length::Percent(std::numeric_limits<float>::max()), Length::Percent(50),
      TransformOperation::kTranslate));

  gfx::Transform mat;
  ops.Apply(gfx::SizeF(800, 600), mat);

  // There should not be inf or nan in the transformation result.
  for (int i = 0; i < 16; i++)
    EXPECT_TRUE(std::isfinite(mat.ColMajorData(i)));
}

TEST(TranformOperationsTest, DisallowBlockSizeDependent_Disallowed) {
  TransformOperations from_ops;
  TransformOperations to_ops;
  from_ops.Operations().push_back(
      MakeGarbageCollected<TranslateTransformOperation>(
          Length::Percent(50), Length::Fixed(20),
          TransformOperation::kTranslate));
  to_ops.Operations().push_back(MakeGarbageCollected<ScaleTransformOperation>(
      2, 2, TransformOperation::kScale));

  const wtf_size_t matching_prefix_length = 0;
  const double progress = 0.8;

  TransformOperations blended_ops = to_ops.Blend(
      from_ops, progress,
      TransformOperations::BoxSizeDependentMatrixBlending::kDisallow);
  EXPECT_EQ(blended_ops, to_ops);

  TransformOperation* blended_op =
      to_ops.BlendRemainingByUsingMatrixInterpolation(
          from_ops, matching_prefix_length, progress,
          TransformOperations::BoxSizeDependentMatrixBlending::kDisallow);
  EXPECT_EQ(blended_op, nullptr);
}

}  // namespace blink
