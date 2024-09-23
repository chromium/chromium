/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/animation/animation_translation_util.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/transforms/matrix_3d_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/rotate_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/scale_transform_operation.h"
#include "third_party/blink/renderer/platform/transforms/transform_operations.h"
#include "third_party/blink/renderer/platform/transforms/translate_transform_operation.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/transform_operations.h"

namespace blink {

TEST(AnimationTranslationUtilTest, transformsWork) {
  TransformOperations ops;
  gfx::TransformOperations out_ops;

  ops.Operations().push_back(MakeGarbageCollected<TranslateTransformOperation>(
      Length::Fixed(2), Length::Fixed(0), TransformOperation::kTranslateX));
  ops.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      0.1, 0.2, 0.3, 200000.4, TransformOperation::kRotate3D));
  ops.Operations().push_back(MakeGarbageCollected<ScaleTransformOperation>(
      50.2, 100, -4, TransformOperation::kScale3D));
  ToGfxTransformOperations(ops, &out_ops, gfx::SizeF());

  EXPECT_EQ(3UL, out_ops.size());
  const float kErr = 0.0001;

  auto& op0 = out_ops.at(0);
  EXPECT_EQ(gfx::TransformOperation::TRANSFORM_OPERATION_TRANSLATE, op0.type);
  EXPECT_NEAR(op0.translate.x, 2.0f, kErr);
  EXPECT_NEAR(op0.translate.y, 0.0f, kErr);
  EXPECT_NEAR(op0.translate.z, 0.0f, kErr);

  auto& op1 = out_ops.at(1);
  EXPECT_EQ(gfx::TransformOperation::TRANSFORM_OPERATION_ROTATE, op1.type);
  EXPECT_NEAR(op1.rotate.axis.x, 0.1f, kErr);
  EXPECT_NEAR(op1.rotate.axis.y, 0.2f, kErr);
  EXPECT_NEAR(op1.rotate.axis.z, 0.3f, kErr);
  EXPECT_NEAR(op1.rotate.angle, 200000.4f, 0.01f);

  auto& op2 = out_ops.at(2);
  EXPECT_EQ(gfx::TransformOperation::TRANSFORM_OPERATION_SCALE, op2.type);
  EXPECT_NEAR(op2.scale.x, 50.2f, kErr);
  EXPECT_NEAR(op2.scale.y, 100.0f, kErr);
  EXPECT_NEAR(op2.scale.z, -4.0f, kErr);
}

TEST(AnimationTranslationUtilTest, RelativeTranslate) {
  TransformOperations ops;
  ops.Operations().push_back(MakeGarbageCollected<TranslateTransformOperation>(
      Length::Percent(50), Length::Percent(50),
      TransformOperation::kTranslate));

  gfx::TransformOperations out_ops;
  ToGfxTransformOperations(ops, &out_ops, gfx::SizeF(200, 100));
  ASSERT_EQ(out_ops.size(), 1u);

  auto& op0 = out_ops.at(0);
  EXPECT_EQ(gfx::TransformOperation::TRANSFORM_OPERATION_TRANSLATE, op0.type);
  EXPECT_EQ(op0.translate.x, 100.0f);
  EXPECT_EQ(op0.translate.y, 50.0f);
  EXPECT_EQ(op0.translate.z, 0.0f);
}

TEST(AnimationTranslationUtilTest, RelativeInterpolated) {
  TransformOperations ops_a, ops_b;
  ops_a.Operations().push_back(
      MakeGarbageCollected<TranslateTransformOperation>(
          Length::Percent(50), Length::Fixed(0),
          TransformOperation::kTranslate));
  ops_b.Operations().push_back(MakeGarbageCollected<RotateTransformOperation>(
      3600, TransformOperation::kRotate));

  TransformOperations ops_c = ops_b.Blend(ops_a, 0.5);

  gfx::TransformOperations out_ops;
  ToGfxTransformOperations(ops_c, &out_ops, gfx::SizeF(100, 100));
  ASSERT_EQ(out_ops.size(), 1u);

  auto& op0 = out_ops.at(0);
  gfx::TransformOperations ops_expected;
  ops_expected.AppendTranslate(25, 0, 0);
  EXPECT_EQ(gfx::TransformOperation::TRANSFORM_OPERATION_MATRIX, op0.type);
  EXPECT_TRANSFORM_NEAR(op0.matrix, ops_expected.at(0).matrix, 1e-6f);
}

}  // namespace blink
