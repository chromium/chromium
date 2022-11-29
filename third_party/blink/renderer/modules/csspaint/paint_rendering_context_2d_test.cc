// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style_test_utils.h"

namespace blink {
namespace {

static const int kWidth = 50;
static const int kHeight = 75;
static const float kZoom = 1.0;

void TrySettingStrokeStyle(V8TestingScope& v8_testing_scope,
                           PaintRenderingContext2D* ctx,
                           const String& expected,
                           const String& value) {
  auto* script_state = v8_testing_scope.GetScriptState();
  SetStrokeStyleString(ctx, script_state, "red");
  SetStrokeStyleString(ctx, script_state, value);
  EXPECT_EQ(expected, GetStrokeStyleAsString(ctx, script_state));
}

TEST(PaintRenderingContext2DTest, testParseColorOrCurrentColor) {
  V8TestingScope v8_testing_scope;
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  context_settings->setAlpha(false);
  PaintRenderingContext2D* ctx = MakeGarbageCollected<PaintRenderingContext2D>(
      gfx::Size(kWidth, kHeight), context_settings, kZoom,
      1.0 /* device_scale_factor */,
      scheduler::GetSingleThreadTaskRunnerForTesting());
  TrySettingStrokeStyle(v8_testing_scope, ctx, "#0000ff", "blue");
  TrySettingStrokeStyle(v8_testing_scope, ctx, "#000000", "currentColor");
}

TEST(PaintRenderingContext2DTest, testWidthAndHeight) {
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  PaintRenderingContext2D* ctx = MakeGarbageCollected<PaintRenderingContext2D>(
      gfx::Size(kWidth, kHeight), context_settings, kZoom,
      1.0 /* device_scale_factor */,
      scheduler::GetSingleThreadTaskRunnerForTesting());
  EXPECT_EQ(kWidth, ctx->Width());
  EXPECT_EQ(kHeight, ctx->Height());
}

TEST(PaintRenderingContext2DTest, testBasicState) {
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  PaintRenderingContext2D* ctx = MakeGarbageCollected<PaintRenderingContext2D>(
      gfx::Size(kWidth, kHeight), context_settings, kZoom,
      1.0 /* device_scale_factor */,
      scheduler::GetSingleThreadTaskRunnerForTesting());

  const double kShadowBlurBefore = 2;
  const double kShadowBlurAfter = 3;

  const String line_join_before = "bevel";
  const String line_join_after = "round";

  ctx->setShadowBlur(kShadowBlurBefore);
  ctx->setLineJoin(line_join_before);
  EXPECT_EQ(kShadowBlurBefore, ctx->shadowBlur());
  EXPECT_EQ(line_join_before, ctx->lineJoin());

  ctx->save();

  ctx->setShadowBlur(kShadowBlurAfter);
  ctx->setLineJoin(line_join_after);
  EXPECT_EQ(kShadowBlurAfter, ctx->shadowBlur());
  EXPECT_EQ(line_join_after, ctx->lineJoin());

  ctx->restore();

  EXPECT_EQ(kShadowBlurBefore, ctx->shadowBlur());
  EXPECT_EQ(line_join_before, ctx->lineJoin());
}

TEST(PaintRenderingContext2DTest, setTransformWithDeviceScaleFactor) {
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  float device_scale_factor = 1.23;
  PaintRenderingContext2D* ctx = MakeGarbageCollected<PaintRenderingContext2D>(
      gfx::Size(kWidth, kHeight), context_settings, kZoom, device_scale_factor,
      scheduler::GetSingleThreadTaskRunnerForTesting());
  DOMMatrix* matrix = ctx->getTransform();
  EXPECT_TRUE(matrix->isIdentity());
  ctx->setTransform(2.1, 2.5, 1.4, 2.3, 20, 50);
  matrix = ctx->getTransform();
  EXPECT_FLOAT_EQ(matrix->a(), 2.1);
  EXPECT_FLOAT_EQ(matrix->b(), 2.5);
  EXPECT_FLOAT_EQ(matrix->c(), 1.4);
  EXPECT_FLOAT_EQ(matrix->d(), 2.3);
  EXPECT_FLOAT_EQ(matrix->e(), 20);
  EXPECT_FLOAT_EQ(matrix->f(), 50);
}

TEST(PaintRenderingContext2DTest, setTransformWithDefaultDeviceScaleFactor) {
  PaintRenderingContext2DSettings* context_settings =
      PaintRenderingContext2DSettings::Create();
  PaintRenderingContext2D* ctx = MakeGarbageCollected<PaintRenderingContext2D>(
      gfx::Size(kWidth, kHeight), context_settings, kZoom,
      1.0 /* device_scale_factor */,
      scheduler::GetSingleThreadTaskRunnerForTesting());
  DOMMatrix* matrix = ctx->getTransform();
  EXPECT_TRUE(matrix->isIdentity());
  ctx->setTransform(1.2, 2.3, 3.4, 4.5, 56, 67);
  matrix = ctx->getTransform();
  EXPECT_FLOAT_EQ(matrix->a(), 1.2);
  EXPECT_FLOAT_EQ(matrix->b(), 2.3);
  EXPECT_FLOAT_EQ(matrix->c(), 3.4);
  EXPECT_FLOAT_EQ(matrix->d(), 4.5);
  EXPECT_FLOAT_EQ(matrix->e(), 56);
  EXPECT_FLOAT_EQ(matrix->f(), 67);
}

}  // namespace
}  // namespace blink
