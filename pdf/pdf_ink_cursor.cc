// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_cursor.h"

#include <algorithm>

#include "base/check_op.h"
#include "pdf/pdf_ink_brush.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"

namespace chrome_pdf {

namespace {

void CheckCursorDiameterIsInRange(float diameter) {
  CHECK_GE(diameter, 6);
  CHECK_LE(diameter, 32);
}

SkColor GetCursorOutlineColor(SkColor color) {
  // Compute the lightness value as defined by HSL.
  const uint8_t max =
      std::max({SkColorGetR(color), SkColorGetG(color), SkColorGetB(color)});
  const uint8_t min =
      std::min({SkColorGetR(color), SkColorGetG(color), SkColorGetB(color)});
  const int lightness = 0.5 * (max + min);

  // Use lighter outline for darker `color`.
  constexpr SkColor kDarkOutlineColor = SkColorSetARGB(0xFF, 0x90, 0x90, 0x90);
  constexpr SkColor kLightOutlineColor = SkColorSetARGB(0xFF, 0xAA, 0xAA, 0xAA);
  return lightness > 127 ? kDarkOutlineColor : kLightOutlineColor;
}

}  // namespace

float CursorDiameterFromBrushSize(float brush_size) {
  PdfInkBrush::CheckToolSizeIsInRange(brush_size);

  constexpr float kMinSize = 4;  // Cursor become very hard to see if smaller.
  float cursor_diameter = std::max(brush_size, kMinSize);

  // Fudge factors to better match sizes used in Chrome OS Gallery.
  constexpr float kSmallAdjustment = 2;
  constexpr float kLargeAdjustment = 4;
  constexpr float kLargeSize = 10;
  float adjustment =
      cursor_diameter < kLargeSize ? kSmallAdjustment : kLargeAdjustment;
  return cursor_diameter + adjustment;
}

SkBitmap GenerateToolCursor(SkColor color, float diameter) {
  CheckCursorDiameterIsInRange(diameter);

  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32(diameter, diameter, kPremul_SkAlphaType));
  SkCanvas canvas(bitmap);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(GetCursorOutlineColor(color));
  paint.setStyle(SkPaint::kFill_Style);

  auto rect = SkRect::MakeWH(diameter, diameter);
  SkPath outline_path;
  outline_path.addOval(rect, SkPathDirection::kCW, 1);
  canvas.drawPath(outline_path, paint);

  paint.setColor(color);
  SkPath fill_path;
  rect.inset(1, 1);
  fill_path.addOval(rect, SkPathDirection::kCW, 1);
  canvas.drawPath(fill_path, paint);

  return bitmap;
}

}  // namespace chrome_pdf
