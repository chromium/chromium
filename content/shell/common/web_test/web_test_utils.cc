// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/web_test/web_test_utils.h"

#include "cc/paint/skia_paint_canvas.h"
#include "third_party/blink/public/platform/web_rect.h"

namespace content {
namespace web_test_utils {

// Utility function to draw a selection rect into a bitmap.
void DrawSelectionRect(const SkBitmap& bitmap, const blink::WebRect& wr) {
  // Render a red rectangle bounding selection rect
  cc::SkiaPaintCanvas canvas(bitmap);
  cc::PaintFlags flags;
  flags.setColor(0xFFFF0000);  // Fully opaque red
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);
  flags.setStrokeWidth(1.0f);
  SkIRect rect;  // Bounding rect
  rect.setXYWH(wr.x, wr.y, wr.width, wr.height);
  canvas.drawIRect(rect, flags);
}

}  // namespace web_test_utils
}  // namespace content
