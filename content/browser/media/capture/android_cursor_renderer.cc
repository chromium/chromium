// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/android_cursor_renderer.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"

// Only used by MouseCursorOverlayControllerAndroid, and is drawn and cached for
// subsequent usages.
namespace content {

namespace {
// The logic base size of the cursor (24x24 DIPs).
constexpr int kBaseCursorSize = 24;

// Knob to adjust visual size of the cursor.
// TODO(b/486019140): Remove this once we use the system cursors directly.
constexpr float kCursorVisualScale = 0.8f;
}  // namespace

// static
SkBitmap AndroidCursorRenderer::GenerateCursorImage(float scale) {
  // For now, we always return a default arrow cursor. This can be updated
  // to support different cursor types if needed, but we should just use the
  // system cursors directly once OS support is added.
  // See ViewAndroidDelegate.java -> onCursorChanged() for the full list of
  // cursors. Also see android_cursor_renderer.h for the TODO.

  gfx::Size size = GetCursorSize();
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size.width() * scale, size.height() * scale);
  bitmap.eraseColor(SK_ColorTRANSPARENT);

  SkCanvas canvas(bitmap);
  canvas.scale(scale, scale);

  // Scale the arrow visually around its tip (hotspot).
  gfx::Point hotspot = GetCursorHotspot();
  canvas.translate(hotspot.x(), hotspot.y());
  canvas.scale(kCursorVisualScale, kCursorVisualScale);
  canvas.translate(-hotspot.x(), -hotspot.y());

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kFill_Style);

  // Draw basic arrow.
  // The path coordinates are based on a 32x32 grid.
  SkPathBuilder builder;
  builder.moveTo(hotspot.x(), hotspot.y());            // Tip
  builder.lineTo(hotspot.x(), hotspot.y() + 17);       // Left edge
  builder.lineTo(hotspot.x() + 4, hotspot.y() + 13);   // Left indent
  builder.lineTo(hotspot.x() + 7, hotspot.y() + 19);   // Tail start
  builder.lineTo(hotspot.x() + 9, hotspot.y() + 18);   // Tail end
  builder.lineTo(hotspot.x() + 6, hotspot.y() + 12);   // Right indent
  builder.lineTo(hotspot.x() + 11, hotspot.y() + 12);  // Top edge
  builder.close();
  SkPath path = builder.detach();

  // Draw white border (outline)
  paint.setARGB(255, 255, 255, 255);  // White
  paint.setStyle(SkPaint::kStrokeAndFill_Style);
  paint.setStrokeWidth(2);
  canvas.drawPath(path, paint);

  // Draw black fill
  paint.setARGB(255, 0, 0, 0);  // Black
  paint.setStyle(SkPaint::kFill_Style);
  canvas.drawPath(path, paint);

  bitmap.setImmutable();
  return bitmap;
}

// static
gfx::Size AndroidCursorRenderer::GetCursorSize() {
  return gfx::Size(kBaseCursorSize, kBaseCursorSize);
}

// static
gfx::Point AndroidCursorRenderer::GetCursorHotspot() {
  return gfx::Point(2, 2);
}

}  // namespace content
