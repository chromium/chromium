// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(awalker): clean up the const/non-const reference handling in this test

#include "skia/ext/platform_canvas.h"

#include <stdint.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkPixelRef.h"

// Native drawing context is only used/supported on Windows.
#if defined(OS_WIN)

namespace skia {

namespace {

// Uses DstATop transfer mode with SK_ColorBLACK 0xff000000:
//   the destination pixel will end up with 0xff alpha
//   if it was transparent black (0x0) before the blend,
//      it will be set to opaque black (0xff000000).
//   if it has nonzero alpha before the blend,
//      it will retain its color.
void MakeOpaque(SkCanvas* canvas, int x, int y, int width, int height) {
  if (width <= 0 || height <= 0)
    return;

  SkRect rect;
  rect.setXYWH(SkIntToScalar(x), SkIntToScalar(y),
               SkIntToScalar(width), SkIntToScalar(height));
  SkPaint paint;
  paint.setColor(SK_ColorBLACK);
  paint.setBlendMode(SkBlendMode::kDstATop);
  canvas->drawRect(rect, paint);
}

bool IsOfColor(const SkBitmap& bitmap, int x, int y, uint32_t color) {
  // For masking out the alpha values.
  static uint32_t alpha_mask =
      static_cast<uint32_t>(SK_A32_MASK) << SK_A32_SHIFT;
  return (*bitmap.getAddr32(x, y) | alpha_mask) == (color | alpha_mask);
}

// Return true if the canvas is filled to canvas_color, and contains a single
// rectangle filled to rect_color. This function ignores the alpha channel,
// since Windows will sometimes clear the alpha channel when drawing, and we
// will fix that up later in cases it's necessary.
bool VerifyRect(const SkCanvas& canvas,
                uint32_t canvas_color, uint32_t rect_color,
                int x, int y, int w, int h) {
  const SkBitmap bitmap = skia::ReadPixels(const_cast<SkCanvas*>(&canvas));

  for (int cur_y = 0; cur_y < bitmap.height(); cur_y++) {
    for (int cur_x = 0; cur_x < bitmap.width(); cur_x++) {
      if (cur_x >= x && cur_x < x + w &&
          cur_y >= y && cur_y < y + h) {
        // Inside the square should be rect_color
        if (!IsOfColor(bitmap, cur_x, cur_y, rect_color))
          return false;
      } else {
        // Outside the square should be canvas_color
        if (!IsOfColor(bitmap, cur_x, cur_y, canvas_color))
          return false;
      }
    }
  }
  return true;
}

#if !defined(USE_AURA) && !defined(OS_MACOSX)
// Return true if canvas has something that passes for a rounded-corner
// rectangle. Basically, we're just checking to make sure that the pixels in the
// middle are of rect_color and pixels in the corners are of canvas_color.
bool VerifyRoundedRect(const SkCanvas& canvas,
                       uint32_t canvas_color,
                       uint32_t rect_color,
                       int x,
                       int y,
                       int w,
                       int h) {
  SkPixmap pixmap;
  ASSERT_TRUE(canvas.peekPixels(&pixmap));
  SkBitmap bitmap;
  bitmap.installPixels(pixmap);

  // Check corner points first. They should be of canvas_color.
  if (!IsOfColor(bitmap, x, y, canvas_color)) return false;
  if (!IsOfColor(bitmap, x + w, y, canvas_color)) return false;
  if (!IsOfColor(bitmap, x, y + h, canvas_color)) return false;
  if (!IsOfColor(bitmap, x + w, y, canvas_color)) return false;

  // Check middle points. They should be of rect_color.
  if (!IsOfColor(bitmap, (x + w / 2), y, rect_color)) return false;
  if (!IsOfColor(bitmap, x, (y + h / 2), rect_color)) return false;
  if (!IsOfColor(bitmap, x + w, (y + h / 2), rect_color)) return false;
  if (!IsOfColor(bitmap, (x + w / 2), y + h, rect_color)) return false;

  return true;
}
#endif

// Checks whether there is a white canvas with a black square at the given
// location in pixels (not in the canvas coordinate system).
bool VerifyBlackRect(const SkCanvas& canvas, int x, int y, int w, int h) {
  return VerifyRect(canvas, SK_ColorWHITE, SK_ColorBLACK, x, y, w, h);
}

// Check that every pixel in the canvas is a single color.
bool VerifyCanvasColor(const SkCanvas& canvas, uint32_t canvas_color) {
  return VerifyRect(canvas, canvas_color, 0, 0, 0, 0, 0);
}

void DrawNativeRect(SkCanvas& canvas, int x, int y, int w, int h) {
  HDC dc = skia::GetNativeDrawingContext(&canvas);

  RECT inner_rc;
  inner_rc.left = x;
  inner_rc.top = y;
  inner_rc.right = x + w;
  inner_rc.bottom = y + h;
  FillRect(dc, &inner_rc, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
}

// Clips the contents of the canvas to the given rectangle. This will be
// intersected with any existing clip.
void AddClip(SkCanvas& canvas, int x, int y, int w, int h) {
  SkRect rect;
  rect.setXYWH(SkIntToScalar(x), SkIntToScalar(y), SkIntToScalar(w),
               SkIntToScalar(h));
  canvas.clipRect(rect);
}

class LayerSaver {
 public:
  LayerSaver(SkCanvas& canvas, int x, int y, int w, int h)
      : canvas_(canvas),
        x_(x),
        y_(y),
        w_(w),
        h_(h) {
    SkRect bounds;
    bounds.setLTRB(SkIntToScalar(x_), SkIntToScalar(y_), SkIntToScalar(right()),
                   SkIntToScalar(bottom()));
    canvas_.saveLayer(&bounds, NULL);
    canvas.clear(SkColorSetARGB(0, 0, 0, 0));
  }

  ~LayerSaver() {
    canvas_.restore();
  }

  int x() const { return x_; }
  int y() const { return y_; }
  int w() const { return w_; }
  int h() const { return h_; }

  // Returns the EXCLUSIVE far bounds of the layer.
  int right() const { return x_ + w_; }
  int bottom() const { return y_ + h_; }

 private:
  SkCanvas& canvas_;
  int x_, y_, w_, h_;
};

// Size used for making layers in many of the below tests.
const int kLayerX = 2;
const int kLayerY = 3;
const int kLayerW = 9;
const int kLayerH = 7;

// Size used by some tests to draw a rectangle inside the layer.
const int kInnerX = 4;
const int kInnerY = 5;
const int kInnerW = 2;
const int kInnerH = 3;

}

// This just checks that our checking code is working properly, it just uses
// regular skia primitives.
TEST(PlatformCanvas, SkLayer) {
  // Create the canvas initialized to opaque white.
  std::unique_ptr<SkCanvas> canvas = CreatePlatformCanvas(16, 16, true);
  canvas->drawColor(SK_ColorWHITE);

  // Make a layer and fill it completely to make sure that the bounds are
  // correct.
  {
    LayerSaver layer(*canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    canvas->drawColor(SK_ColorBLACK);
  }
  EXPECT_TRUE(VerifyBlackRect(*canvas, kLayerX, kLayerY, kLayerW, kLayerH));
}

// Test native clipping.
TEST(PlatformCanvas, ClipRegion) {
  // Initialize a white canvas
  std::unique_ptr<SkCanvas> canvas = CreatePlatformCanvas(16, 16, true);
  canvas->drawColor(SK_ColorWHITE);
  EXPECT_TRUE(VerifyCanvasColor(*canvas, SK_ColorWHITE));

  // Test that initially the canvas has no clip region, by filling it
  // with a black rectangle.
  // Note: Don't use LayerSaver, since internally it sets a clip region.
  DrawNativeRect(*canvas, 0, 0, 16, 16);
  EXPECT_TRUE(VerifyCanvasColor(*canvas, SK_ColorBLACK));

  // Test that intersecting disjoint clip rectangles sets an empty clip region
  canvas->drawColor(SK_ColorWHITE);
  EXPECT_TRUE(VerifyCanvasColor(*canvas, SK_ColorWHITE));
  {
    LayerSaver layer(*canvas, 0, 0, 16, 16);
    AddClip(*canvas, 2, 3, 4, 5);
    AddClip(*canvas, 4, 9, 10, 10);
    DrawNativeRect(*canvas, 0, 0, 16, 16);
  }
  EXPECT_TRUE(VerifyCanvasColor(*canvas, SK_ColorWHITE));
}

// Test the layers get filled properly by native rendering.
TEST(PlatformCanvas, FillLayer) {
  // Create the canvas initialized to opaque white.
  std::unique_ptr<SkCanvas> canvas(CreatePlatformCanvas(16, 16, true));

  // Make a layer and fill it completely to make sure that the bounds are
  // correct.
  canvas->drawColor(SK_ColorWHITE);
  {
    LayerSaver layer(*canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawNativeRect(*canvas, 0, 0, 100, 100);
    MakeOpaque(canvas.get(), 0, 0, 100, 100);
  }
  EXPECT_TRUE(VerifyBlackRect(*canvas, kLayerX, kLayerY, kLayerW, kLayerH));

  // Make a layer and fill it partially to make sure the translation is correct.
  canvas->drawColor(SK_ColorWHITE);
  {
    LayerSaver layer(*canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawNativeRect(*canvas, kInnerX, kInnerY, kInnerW, kInnerH);
    MakeOpaque(canvas.get(), kInnerX, kInnerY, kInnerW, kInnerH);
  }
  EXPECT_TRUE(VerifyBlackRect(*canvas, kInnerX, kInnerY, kInnerW, kInnerH));

  // Add a clip on the layer and fill to make sure clip is correct.
  canvas->drawColor(SK_ColorWHITE);
  {
    LayerSaver layer(*canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    canvas->save();
    AddClip(*canvas, kInnerX, kInnerY, kInnerW, kInnerH);
    DrawNativeRect(*canvas, 0, 0, 100, 100);
    MakeOpaque(canvas.get(), kInnerX, kInnerY, kInnerW, kInnerH);
    canvas->restore();
  }
  EXPECT_TRUE(VerifyBlackRect(*canvas, kInnerX, kInnerY, kInnerW, kInnerH));

  // Add a clip and then make the layer to make sure the clip is correct.
  canvas->drawColor(SK_ColorWHITE);
  canvas->save();
  AddClip(*canvas, kInnerX, kInnerY, kInnerW, kInnerH);
  {
    LayerSaver layer(*canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawNativeRect(*canvas, 0, 0, 100, 100);
    MakeOpaque(canvas.get(), 0, 0, 100, 100);
  }
  canvas->restore();
  EXPECT_TRUE(VerifyBlackRect(*canvas, kInnerX, kInnerY, kInnerW, kInnerH));
}

// Test that translation + make layer works properly.
TEST(PlatformCanvas, TranslateLayer) {
  // Create the canvas initialized to opaque white.
  std::unique_ptr<SkCanvas> canvas = CreatePlatformCanvas(16, 16, true);

  // Make a layer and fill it completely to make sure that the bounds are
  // correct.
  canvas->drawColor(SK_ColorWHITE);
  canvas->save();
  canvas->translate(1, 1);
  {
    LayerSaver layer(*canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawNativeRect(*canvas, 0, 0, 100, 100);
    MakeOpaque(canvas.get(), 0, 0, 100, 100);
  }
  canvas->restore();
  EXPECT_TRUE(VerifyBlackRect(*canvas, kLayerX + 1, kLayerY + 1,
                              kLayerW, kLayerH));

  // Translate then make the layer.
  canvas->drawColor(SK_ColorWHITE);
  canvas->save();
  canvas->translate(1, 1);
  {
    LayerSaver layer(*canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    DrawNativeRect(*canvas, kInnerX, kInnerY, kInnerW, kInnerH);
    MakeOpaque(canvas.get(), kInnerX, kInnerY, kInnerW, kInnerH);
  }
  canvas->restore();
  EXPECT_TRUE(VerifyBlackRect(*canvas, kInnerX + 1, kInnerY + 1,
                              kInnerW, kInnerH));

  // Make the layer then translate.
  canvas->drawColor(SK_ColorWHITE);
  canvas->save();
  {
    LayerSaver layer(*canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    canvas->translate(1, 1);
    DrawNativeRect(*canvas, kInnerX, kInnerY, kInnerW, kInnerH);
    MakeOpaque(canvas.get(), kInnerX, kInnerY, kInnerW, kInnerH);
  }
  canvas->restore();
  EXPECT_TRUE(VerifyBlackRect(*canvas, kInnerX + 1, kInnerY + 1,
                              kInnerW, kInnerH));

  // Translate both before and after, and have a clip.
  canvas->drawColor(SK_ColorWHITE);
  canvas->save();
  canvas->translate(1, 1);
  {
    LayerSaver layer(*canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    canvas->drawColor(SK_ColorWHITE);
    canvas->translate(1, 1);
    AddClip(*canvas, kInnerX + 1, kInnerY + 1, kInnerW - 1, kInnerH - 1);
    DrawNativeRect(*canvas, 0, 0, 100, 100);
    MakeOpaque(canvas.get(), kLayerX, kLayerY, kLayerW, kLayerH);
  }
  canvas->restore();
  EXPECT_TRUE(VerifyBlackRect(*canvas, kInnerX + 3, kInnerY + 3,
                              kInnerW - 1, kInnerH - 1));

// TODO(dglazkov): Figure out why this fails on Mac (antialiased clipping?),
// modify test and remove this guard.
#if !defined(USE_AURA)
  // Translate both before and after, and have a path clip.
  canvas->drawColor(SK_ColorWHITE);
  canvas->save();
  canvas->translate(1, 1);
  {
    LayerSaver layer(*canvas, kLayerX, kLayerY, kLayerW, kLayerH);
    canvas->drawColor(SK_ColorWHITE);
    canvas->translate(1, 1);

    SkPath path;
    SkRect rect;
    rect.iset(kInnerX - 1, kInnerY - 1,
              kInnerX + kInnerW, kInnerY + kInnerH);
    const SkScalar kRadius = 2.0;
    path.addRoundRect(rect, kRadius, kRadius);
    canvas->clipPath(path);

    DrawNativeRect(*canvas, 0, 0, 100, 100);
    MakeOpaque(canvas.get(), kLayerX, kLayerY, kLayerW, kLayerH);
  }
  canvas->restore();
  EXPECT_TRUE(VerifyRoundedRect(*canvas, SK_ColorWHITE, SK_ColorBLACK,
                                kInnerX + 1, kInnerY + 1, kInnerW, kInnerH));
#endif
}

}  // namespace skia

#endif // defined(OS_WIN)
