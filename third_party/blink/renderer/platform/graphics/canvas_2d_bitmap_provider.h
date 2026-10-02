// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_BITMAP_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_BITMAP_PROVIDER_H_

#include <memory>
#include <optional>

#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_record.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/canvas_memory_dump_provider.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"

class SkCanvas;
class SkSurface;

namespace gfx {
class Size;
}  // namespace gfx

namespace blink {

class CanvasImageProvider;

// Renders canvas2D ops to a Skia RAM-backed bitmap. Mailboxing is not
// supported : cannot be directly composited. For usage by (Offscreen)Canvas2D
// as a last-case resort when it is not possible to create
// Canvas2DResourceProvider.
class PLATFORM_EXPORT Canvas2DBitmapProvider final
    : public CanvasMemoryDumpClient {
 public:
  // The returned instance will have been cleared at creation.
  static std::unique_ptr<Canvas2DBitmapProvider> CreateWithClear(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      CanvasResourceProviderDelegate* delegate = nullptr);

  ~Canvas2DBitmapProvider();

  void RasterRecord(cc::PaintRecord last_recording,
                    CanvasImageProvider* image_provider);
  SkSurface* surface() const { return surface_.get(); }

 private:
  Canvas2DBitmapProvider(sk_sp<SkSurface> surface,
                         CanvasResourceProviderDelegate* delegate);

  // CanvasMemoryDumpClient implementation.
  void OnMemoryDump(base::trace_event::ProcessMemoryDump*) override;
  size_t GetSize() const override;

  void ApplyAnimatedImageFrameIndexesForId(CanvasImageProvider* image_provider,
                                           SkCanvas* canvas,
                                           uint32_t id);

  WeakPersistent<CanvasResourceProviderDelegate> delegate_;
  const sk_sp<SkSurface> surface_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_2D_BITMAP_PROVIDER_H_
