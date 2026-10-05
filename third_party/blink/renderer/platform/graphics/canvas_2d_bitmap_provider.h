// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_BITMAP_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_BITMAP_PROVIDER_H_

#include <memory>

#include "components/viz/common/resources/shared_image_format.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"

class SkSurface;

namespace gfx {
class Size;
}  // namespace gfx

namespace blink {

// Renders canvas2D ops to a Skia RAM-backed bitmap. Mailboxing is not
// supported : cannot be directly composited. For usage by (Offscreen)Canvas2D
// as a last-case resort when it is not possible to create
// Canvas2DResourceProvider.
class PLATFORM_EXPORT Canvas2DBitmapProvider final {
 public:
  // The returned instance will have been cleared at creation.
  static std::unique_ptr<Canvas2DBitmapProvider> CreateWithClear(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space);

  ~Canvas2DBitmapProvider();

  SkSurface* surface() const { return surface_.get(); }

 private:
  explicit Canvas2DBitmapProvider(sk_sp<SkSurface> surface);

  const sk_sp<SkSurface> surface_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_BITMAP_PROVIDER_H_
