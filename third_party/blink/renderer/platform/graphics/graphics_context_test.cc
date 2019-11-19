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

#define EXPECT_OPAQUE_PIXELS_IN_RECT(bitmap, opaqueRect)         \
  {                                                              \
    for (int y = opaqueRect.Y(); y < opaqueRect.MaxY(); ++y)     \
      for (int x = opaqueRect.X(); x < opaqueRect.MaxX(); ++x) { \
        int alpha = *bitmap.getAddr32(x, y) >> 24;               \
        EXPECT_EQ(255, alpha);                                   \
      }                                                          \
  }

#define EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, opaqueRect) \
  {                                                           \
    for (int y = 0; y < bitmap.height(); ++y)                 \
      for (int x = 0; x < bitmap.width(); ++x) {              \
        int alpha = *bitmap.getAddr32(x, y) >> 24;            \
        bool opaque = opaqueRect.Contains(x, y);              \
        EXPECT_EQ(opaque, alpha == 255);                      \
      }                                                       \
  }

TEST(GraphicsContextTest, Recording) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);
  bitmap.eraseColor(0);
  SkiaPaintCanvas canvas(bitmap);

  auto paint_controller = std::make_unique<PaintController>();
  GraphicsContext context(*paint_controller);

  Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
  FloatRect bounds(0, 0, 100, 100);

  context.BeginRecording(bounds);
  context.FillRect(FloatRect(0, 0, 50, 50), opaque, SkBlendMode::kSrcOver);
  canvas.drawPicture(context.EndRecording());
  EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, IntRect(0, 0, 50, 50))

  context.BeginRecording(bounds);
  context.FillRect(FloatRect(0, 0, 100, 100), opaque, SkBlendMode::kSrcOver);
  // Make sure the opaque region was unaffected by the rect drawn during
  // recording.
  EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, IntRect(0, 0, 50, 50))

  canvas.drawPicture(context.EndRecording());
  EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, IntRect(0, 0, 100, 100))
}

TEST(GraphicsContextTest, UnboundedDrawsAreClipped) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(400, 400);
  bitmap.eraseColor(0);
  SkiaPaintCanvas canvas(bitmap);

  Color opaque(1.0f, 0.0f, 0.0f, 1.0f);
  Color alpha(0.0f, 0.0f, 0.0f, 0.0f);
  FloatRect bounds(0, 0, 100, 100);

  auto paint_controller = std::make_unique<PaintController>();
  GraphicsContext context(*paint_controller);
  context.BeginRecording(bounds);

  context.SetShouldAntialias(false);
  context.SetMiterLimit(1);
  context.SetStrokeThickness(5);
  context.SetLineCap(kSquareCap);
  context.SetStrokeStyle(kSolidStroke);

  // Make skia unable to compute fast bounds for our paths.
  DashArray dash_array;
  dash_array.push_back(1);
  dash_array.push_back(0);
  context.SetLineDash(dash_array, 0);

  // Make the device opaque in 10,10 40x40.
  context.FillRect(FloatRect(10, 10, 40, 40), opaque, SkBlendMode::kSrcOver);
  canvas.drawPicture(context.EndRecording());
  EXPECT_OPAQUE_PIXELS_ONLY_IN_RECT(bitmap, IntRect(10, 10, 40, 40));

  context.BeginRecording(bounds);
  // Clip to the left edge of the opaque area.
  context.Clip(IntRect(10, 10, 10, 40));

  // Draw a path that gets clipped. This should destroy the opaque area, but
  // only inside the clip.
  Path path;
  path.MoveTo(FloatPoint(10, 10));
  path.AddLineTo(FloatPoint(40, 40));
  PaintFlags flags;
  flags.setColor(alpha.Rgb());
  flags.setBlendMode(SkBlendMode::kSrcOut);
  context.DrawPath(path.GetSkPath(), flags);

  canvas.drawPicture(context.EndRecording());
  EXPECT_OPAQUE_PIXELS_IN_RECT(bitmap, IntRect(20, 10, 30, 40));
}

class GraphicsContextDarkModeTest : public testing::Test {
 protected:
  void SetUp() override {
    bitmap_.allocN32Pixels(4, 1);
    bitmap_.eraseColor(0);
    canvas_ = std::make_unique<SkiaPaintCanvas>(bitmap_);
    paint_controller_ = std::make_unique<PaintController>();
    context_ = std::make_unique<GraphicsContext>(*paint_controller_);
    context_->BeginRecording(FloatRect(0, 0, 4, 1));
  }

  void DrawColorsToContext() {
    context_->FillRect(FloatRect(0, 0, 1, 1), Color(SK_ColorBLACK));
    context_->FillRect(FloatRect(1, 0, 1, 1), Color(SK_ColorWHITE));
    context_->FillRect(FloatRect(2, 0, 1, 1), Color(SK_ColorRED));
    context_->FillRect(FloatRect(3, 0, 1, 1), Color(SK_ColorGRAY));
    // Capture the result in the bitmap.
    canvas_->drawPicture(context_->EndRecording());
  }

  SkBitmap bitmap_;
  std::unique_ptr<SkiaPaintCanvas> canvas_;
  std::unique_ptr<PaintController> paint_controller_;
  std::unique_ptr<GraphicsContext> context_;
};

// This is just a baseline test, compare against the other variants
// of the test below, where dark mode is enabled.
TEST_F(GraphicsContextDarkModeTest, NoDarkMode) {
  DrawColorsToContext();

  EXPECT_EQ(SK_ColorBLACK, bitmap_.getColor(0, 0));
  EXPECT_EQ(SK_ColorWHITE, bitmap_.getColor(1, 0));
  EXPECT_EQ(SK_ColorRED, bitmap_.getColor(2, 0));
  EXPECT_EQ(SK_ColorGRAY, bitmap_.getColor(3, 0));
}

TEST_F(GraphicsContextDarkModeTest, DarkModeOff) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kOff;
  settings.grayscale = false;
  settings.contrast = 0;
  context_->SetDarkMode(settings);

  DrawColorsToContext();

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
  settings.grayscale = false;
  settings.contrast = 0;
  context_->SetDarkMode(settings);

  DrawColorsToContext();

  EXPECT_EQ(SK_ColorWHITE, bitmap_.getColor(0, 0));
  EXPECT_EQ(SK_ColorBLACK, bitmap_.getColor(1, 0));
  EXPECT_EQ(SK_ColorCYAN, bitmap_.getColor(2, 0));
  EXPECT_EQ(0xff777777, bitmap_.getColor(3, 0));
}

// Invert brightness (with gamma correction).
TEST_F(GraphicsContextDarkModeTest, InvertBrightness) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kInvertBrightness;
  settings.grayscale = false;
  settings.contrast = 0;
  context_->SetDarkMode(settings);

  DrawColorsToContext();

  EXPECT_EQ(SK_ColorWHITE, bitmap_.getColor(0, 0));
  EXPECT_EQ(SK_ColorBLACK, bitmap_.getColor(1, 0));
  EXPECT_EQ(SK_ColorCYAN, bitmap_.getColor(2, 0));
  EXPECT_EQ(0xffd8d8d8, bitmap_.getColor(3, 0));
}

// Invert lightness (in HSL space).
TEST_F(GraphicsContextDarkModeTest, InvertLightness) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kInvertLightness;
  settings.grayscale = false;
  settings.contrast = 0;
  context_->SetDarkMode(settings);

  DrawColorsToContext();

  EXPECT_EQ(SK_ColorWHITE, bitmap_.getColor(0, 0));
  EXPECT_EQ(SK_ColorBLACK, bitmap_.getColor(1, 0));
  EXPECT_EQ(SK_ColorRED, bitmap_.getColor(2, 0));
  EXPECT_EQ(0xffd8d8d8, bitmap_.getColor(3, 0));
}

// Invert lightness plus grayscale.
TEST_F(GraphicsContextDarkModeTest, InvertLightnessPlusGrayscale) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kInvertLightness;
  settings.grayscale = true;
  settings.contrast = 0;
  context_->SetDarkMode(settings);

  DrawColorsToContext();

  EXPECT_EQ(SK_ColorWHITE, bitmap_.getColor(0, 0));
  EXPECT_EQ(SK_ColorBLACK, bitmap_.getColor(1, 0));
  EXPECT_EQ(0xffe2e2e2, bitmap_.getColor(2, 0));
  EXPECT_EQ(0xffd8d8d8, bitmap_.getColor(3, 0));
}

TEST_F(GraphicsContextDarkModeTest, InvertLightnessPlusContrast) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kInvertLightness;
  settings.grayscale = false;
  settings.contrast = 0.2;
  context_->SetDarkMode(settings);

  DrawColorsToContext();

  EXPECT_EQ(SK_ColorWHITE, bitmap_.getColor(0, 0));
  EXPECT_EQ(SK_ColorBLACK, bitmap_.getColor(1, 0));
  EXPECT_EQ(SK_ColorRED, bitmap_.getColor(2, 0));
  EXPECT_EQ(0xffe7e7e7, bitmap_.getColor(3, 0));
}

}  // namespace
}  // namespace blink
