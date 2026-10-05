// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_2d_bitmap_provider.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

Canvas2DBitmapProvider::Canvas2DBitmapProvider(sk_sp<SkSurface> surface)
    : surface_(std::move(surface)) {
  CHECK(surface_);
}

Canvas2DBitmapProvider::~Canvas2DBitmapProvider() = default;

std::unique_ptr<Canvas2DBitmapProvider> Canvas2DBitmapProvider::CreateWithClear(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space) {
  const auto info = SkImageInfo::Make(
      size.width(), size.height(), viz::ToClosestSkColorType(format),
      kPremul_SkAlphaType, color_space.ToSkColorSpace());
  const bool can_use_lcd_text = alpha_type == kOpaque_SkAlphaType;
  const auto props =
      skia::LegacyDisplayGlobals::ComputeSurfaceProps(can_use_lcd_text);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info, &props);
  if (!surface) {
    return nullptr;
  }
  surface->getCanvas()->clear(
      alpha_type == kOpaque_SkAlphaType ? SkColors::kBlack
                                        : SkColors::kTransparent);
  return base::WrapUnique<Canvas2DBitmapProvider>(
      new Canvas2DBitmapProvider(std::move(surface)));
}

}  // namespace blink
