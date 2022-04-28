// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/graphics.h"

#include <cmath>

#include "base/notreached.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/blit.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace chrome_pdf {

SkiaGraphics::SkiaGraphics(Client* client, SkSurface* surface)
    : client_(client), surface_(surface) {}

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

void SkiaGraphics::SetScale(float scale) {
  if (scale <= 0.0f) {
    NOTREACHED();
    return;
  }

  client_->UpdateScale(scale);
}

void SkiaGraphics::SetLayerTransform(float scale,
                                     const gfx::Point& origin,
                                     const gfx::Vector2d& translate) {
  if (scale <= 0.0f) {
    NOTREACHED();
    return;
  }

  // translate_with_origin = origin - scale * origin - translate
  gfx::Vector2dF translate_with_origin = origin.OffsetFromOrigin();
  translate_with_origin.Scale(1.0f - scale);
  translate_with_origin.Subtract(translate);

  // TODO(crbug.com/1263614): Pepper defers updates until `Flush()`. Determine
  // if `SkiaGraphics` should do something similar.
  client_->UpdateLayerTransform(scale, translate_with_origin);
}

}  // namespace chrome_pdf
