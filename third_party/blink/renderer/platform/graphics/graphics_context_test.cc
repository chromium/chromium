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
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

#include <memory>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkShader.h"

namespace blink {
namespace {

#define EXPECT_EQ_RECT(a, b)       \
  EXPECT_EQ(a.x(), b.x());         \
  EXPECT_EQ(a.y(), b.y());         \
  EXPECT_EQ(a.width(), b.width()); \
  EXPECT_EQ(a.height(), b.height());

#define EXPECT_OPAQUE_PIXELS_IN_RECT(bitmap, opaqueRect)          \
  {                                                               \
    for (int y = opaqueRect.y(); y < opaqueRect.bottom(); ++y)    \
      for (int x = opaqueRect.x(); x < opaqueRect.right(); ++x) { \
        int alpha = *bitmap.getAddr32(x, y) >> 24;                \
        EXPECT_EQ(255, alpha);                                    \
      }                                                           \
  }

#define EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, opaqueRect) \
  {                                                           \
    for (int y = 0; y < bitmap.height(); ++y)                 \
      for (int x = 0; x < bitmap.width(); ++x) {              \
        int alpha = *bitmap.getAddr32(x, y) >> 24;            \
        bool is_opaque = opaqueRect.Contains(x, y);           \
        EXPECT_EQ(is_opaque, alpha == 255);                   \
      }                                                       \
  }

AutoDarkMode AutoDarkModeDisabled() {
  return AutoDarkMode(DarkModeFilter::ElementRole::kBackground, false);
}

TEST(GraphicsContextTest, Recording) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(0);
  SkiaPaintCanvas canvas(bitmap);

  PaintController paint_controller;
  GraphicsContext context(paint_controller);

  Color opaque = Color::FromRGBA(255, 0, 0, 255);

  context.BeginRecording();
  context.FillRect(gfx::RectF(0, 0, 50, 50), opaque, AutoDarkModeDisabled(),
                   SkBlendMode::kSrcOver);
  canvas.drawPicture(context.EndRecording());
  EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, gfx::Rect(0, 0, 50, 50))

  context.BeginRecording();
  context.FillRect(gfx::RectF(0, 0, 100, 100), opaque, AutoDarkModeDisabled(),
                   SkBlendMode::kSrcOver);
  // Make sure the opaque region was unaffected by the rect drawn during
  // recording.
  EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, gfx::Rect(0, 0, 50, 50))

  canvas.drawPicture(context.EndRecording());
  EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, gfx::Rect(0, 0, 100, 100))
}

TEST(GraphicsContextTest, UnboundedDrawsAreClipped) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(400, 400);
  bitmap.eraseColor(0);
  SkiaPaintCanvas canvas(bitmap);

  Color opaque = Color::FromRGBA(255, 0, 0, 255);
  Color transparent = Color::kTransparent;

  PaintController paint_controller;
  GraphicsContext context(paint_controller);
  context.BeginRecording();

  context.SetShouldAntialias(false);

  // Make the device opaque in 10,10 40x40.
  context.FillRect(gfx::RectF(10, 10, 40, 40), opaque, AutoDarkModeDisabled(),
                   SkBlendMode::kSrcOver);
  canvas.drawPicture(context.EndRecording());
  EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, gfx::Rect(10, 10, 40, 40));

  context.BeginRecording();
  // Clip to the left edge of the opaque area.
  context.Clip(gfx::Rect(10, 10, 10, 40));

  // Draw a path that gets clipped. This should destroy the opaque area, but
  // only inside the clip.
  Path path;
  path.MoveTo(gfx::PointF(10, 10));
  path.AddLineTo(gfx::PointF(40, 40));
  cc::PaintFlags flags;
  flags.setColor(transparent.Rgb());
  flags.setBlendMode(SkBlendMode::kSrcOut);
  context.DrawPath(path.GetSkPath(), flags, AutoDarkModeDisabled());

  canvas.drawPicture(context.EndRecording());
  EXPECT_OPAQUE_PIXELS_IN_RECT(bitmap, gfx::Rect(20, 10, 30, 40));
}

class GraphicsContextDarkModeTest : public testing::Test {
 protected:
  void SetUp() override {
    bitmap_.allocN32Pixels(4, 1);
    bitmap_.eraseColor(0);
    canvas_ = std::make_unique<SkiaPaintCanvas>(bitmap_);
  }

  void DrawColorsToContext(bool is_dark_mode_on,
                           const DarkModeSettings& settings) {
    PaintController paint_controller;
    GraphicsContext context(paint_controller);
    if (is_dark_mode_on)
      context.UpdateDarkModeSettingsForTest(settings);
    context.BeginRecording();
    context.FillRect(gfx::RectF(0, 0, 1, 1), Color::kBlack,
                     AutoDarkMode(DarkModeFilter::ElementRole::kBackground,
                                  is_dark_mode_on));
    context.FillRect(gfx::RectF(1, 0, 1, 1), Color::kWhite,
                     AutoDarkMode(DarkModeFilter::ElementRole::kBackground,
                                  is_dark_mode_on));
    context.FillRect(gfx::RectF(2, 0, 1, 1), Color::FromSkColor(SK_ColorRED),
                     AutoDarkMode(DarkModeFilter::ElementRole::kBackground,
                                  is_dark_mode_on));
    context.FillRect(gfx::RectF(3, 0, 1, 1), Color::FromSkColor(SK_ColorGRAY),
                     AutoDarkMode(DarkModeFilter::ElementRole::kBackground,
                                  is_dark_mode_on));
    // Capture the result in the bitmap.
    canvas_->drawPicture(context.EndRecording());
  }

  SkBitmap bitmap_;
  std::unique_ptr<SkiaPaintCanvas> canvas_;
};

// This is a baseline test where dark mode is turned off. Compare other variants
// of the test where dark mode is enabled.
TEST_F(GraphicsContextDarkModeTest, DarkModeOff) {
  DarkModeSettings settings;

  DrawColorsToContext(false, settings);

  EXPECT_EQ(SK_ColorBLACK, bitmap_.getColor(0, 0));
  EXPECT_EQ(SK_ColorWHITE, bitmap_.getColor(1, 0));
  EXPECT_EQ(SK_ColorRED, bitmap_.getColor(2, 0));
  EXPECT_EQ(SK_ColorGRAY, bitmap_.getColor(3, 0));
}

// Simple invert for testing. Each color component |c|
// is replaced with |255 - c| for easy testing.
TEST_F(GraphicsContextDarkModeTest, SimpleInvertForTesting) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  settings.contrast = 0;

  DrawColorsToContext(true, settings);

  EXPECT_EQ(SK_ColorWHITE, bitmap_.getColor(0, 0));
  EXPECT_EQ(SK_ColorBLACK, bitmap_.getColor(1, 0));
  EXPECT_EQ(SK_ColorCYAN, bitmap_.getColor(2, 0));
  EXPECT_EQ(0xff777777, bitmap_.getColor(3, 0));
}

// Invert brightness (with gamma correction).
TEST_F(GraphicsContextDarkModeTest, InvertBrightness) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kInvertBrightness;
  settings.contrast = 0;

  DrawColorsToContext(true, settings);

  EXPECT_EQ(SK_ColorWHITE, bitmap_.getColor(0, 0));
  EXPECT_EQ(SK_ColorBLACK, bitmap_.getColor(1, 0));
  EXPECT_EQ(SK_ColorCYAN, bitmap_.getColor(2, 0));
  EXPECT_EQ(0xffe1e1e1, bitmap_.getColor(3, 0));
}

// Invert lightness (in HSL space).
TEST_F(GraphicsContextDarkModeTest, InvertLightness) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kInvertLightness;
  settings.contrast = 0;

  DrawColorsToContext(true, settings);

  EXPECT_EQ(SK_ColorWHITE, bitmap_.getColor(0, 0));
  EXPECT_EQ(SK_ColorBLACK, bitmap_.getColor(1, 0));
  EXPECT_EQ(SK_ColorRED, bitmap_.getColor(2, 0));
  EXPECT_EQ(0xffe1e1e1, bitmap_.getColor(3, 0));
}

TEST_F(GraphicsContextDarkModeTest, InvertLightnessPlusContrast) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kInvertLightness;
  settings.contrast = 0.2;

  DrawColorsToContext(true, settings);

  EXPECT_EQ(SK_ColorWHITE, bitmap_.getColor(0, 0));
  EXPECT_EQ(SK_ColorBLACK, bitmap_.getColor(1, 0));
  EXPECT_EQ(SK_ColorRED, bitmap_.getColor(2, 0));
  EXPECT_EQ(0xfff1f1f1, bitmap_.getColor(3, 0));
}

}  // namespace
}  // namespace blink
