// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/graphics.h"

#include <cmath>

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/blit.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"

namespace chrome_pdf {

SkiaGraphics::SkiaGraphics(SkSurface* surface) : surface_(surface) {}

SkiaGraphics::~SkiaGraphics() = default;

void SkiaGraphics::PaintImage(const SkBitmap& image,
                              const gfx::Rect& src_rect) {
  SkRect skia_rect = RectToSkRect(src_rect);

  // TODO(crbug.com/1284255): Avoid inefficient `SkBitmap::asImage()`.
  surface_->getCanvas()->drawImageRect(image.asImage(), skia_rect, skia_rect,
                                       SkSamplingOptions(), nullptr,
                                       SkCanvas::kStrict_SrcRectConstraint);
}

void SkiaGraphics::Scroll(const gfx::Rect& clip, const gfx::Vector2d& amount) {
  // If we are being asked to scroll by more than the graphics' rect size, just
  // ignore the scroll command.
  if (std::abs(amount.x()) >= surface_->width() ||
      std::abs(amount.y()) >= surface_->height()) {
    return;
  }

  // TODO(crbug.com/1263614): Use `SkSurface::notifyContentWillChange()`.
  gfx::ScrollCanvas(surface_->getCanvas(), clip, amount);
}

}  // namespace chrome_pdf
