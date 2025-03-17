// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/platform_focus_ring.h"

#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"

namespace blink {

namespace {

cc::PaintFlags PaintFlagsForFocusRing(SkColor4f color, float width) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setColor(color);
  flags.setStrokeWidth(width);
  return flags;
}

}  // namespace

void DrawPlatformFocusRing(const SkRRect& rrect,
                           cc::PaintCanvas* canvas,
                           SkColor4f color,
                           float width) {
  canvas->drawRRect(rrect, PaintFlagsForFocusRing(color, width));
}

void DrawPlatformFocusRing(const SkPath& path,
                           cc::PaintCanvas* canvas,
                           SkColor4f color,
                           float width,
                           float corner_radius) {
  cc::PaintFlags path_flags = PaintFlagsForFocusRing(color, width);
  if (corner_radius) {
    path_flags.setPathEffect(cc::PathEffect::MakeCorner(corner_radius));
  }
  canvas->drawPath(path, path_flags);
}

}  // namespace blink
