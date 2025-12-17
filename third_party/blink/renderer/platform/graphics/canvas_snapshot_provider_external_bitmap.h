// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_SNAPSHOT_PROVIDER_EXTERNAL_BITMAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_SNAPSHOT_PROVIDER_EXTERNAL_BITMAP_H_

#include <memory>

#include "cc/paint/skia_paint_canvas.h"
#include "cc/raster/playback_image_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_snapshot_provider.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"

namespace blink {

// Renders to a RAM-backed bitmap via an external (client-supplied) draw.
class PLATFORM_EXPORT CanvasSnapshotProviderExternalBitmap
    : public CanvasSnapshotProvider,
      public cc::ImageProvider {
 public:
  static std::unique_ptr<CanvasSnapshotProviderExternalBitmap> Create(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space);

  ~CanvasSnapshotProviderExternalBitmap() override;

  // CanvasSnapshotProvider:
  bool IsGpuContextLost() const override;
  bool IsValid() const override;
  bool IsAccelerated() const override { return false; }
  bool IsExternalBitmapProvider() const override { return true; }
  scoped_refptr<StaticBitmapImage> DoExternalDrawAndSnapshot(
      base::FunctionRef<void(MemoryManagedPaintCanvas&)> draw_callback,
      ImageOrientation orientation) override;
  viz::SharedImageFormat GetSharedImageFormat() const override {
    return format_;
  }
  gfx::ColorSpace GetColorSpace() const override { return color_space_; }
  SkAlphaType GetAlphaType() const override { return alpha_type_; }
  gfx::Size Size() const override { return size_; }

  // cc::ImageProvider:
  cc::ImageProvider::ScopedResult GetRasterContent(
      const cc::DrawImage& draw_image) override;

 private:
  CanvasSnapshotProviderExternalBitmap(gfx::Size size,
                                       viz::SharedImageFormat format,
                                       SkAlphaType alpha_type,
                                       const gfx::ColorSpace& color_space);

  std::optional<cc::PlaybackImageProvider> playback_image_provider_n32_;
  std::optional<cc::PlaybackImageProvider> playback_image_provider_f16_;

  sk_sp<SkSurface> surface_;
  std::unique_ptr<cc::SkiaPaintCanvas> skia_canvas_;

  gfx::Size size_;
  viz::SharedImageFormat format_;
  SkAlphaType alpha_type_;
  gfx::ColorSpace color_space_;
  const cc::PaintImage::Id snapshot_paint_image_id_;
  cc::PaintImage::ContentId snapshot_paint_image_content_id_ =
      cc::PaintImage::kInvalidContentId;
  uint32_t snapshot_sk_image_id_ = 0u;

  // Recording accumulating draw ops. This pointer is always valid and safe to
  // dereference.
  std::unique_ptr<MemoryManagedPaintRecorder> recorder_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_SNAPSHOT_PROVIDER_EXTERNAL_BITMAP_H_
