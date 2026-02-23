// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_NON2D_SNAPSHOT_PROVIDER_BITMAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_NON2D_SNAPSHOT_PROVIDER_BITMAP_H_

#include <memory>

#include "cc/paint/skia_paint_canvas.h"
#include "cc/raster/playback_image_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_snapshot_provider.h"
#include "third_party/blink/renderer/platform/graphics/memory_managed_paint_recorder.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"

namespace blink {

// Renders to a RAM-backed bitmap via an external (client-supplied) draw.
class PLATFORM_EXPORT CanvasNon2DSnapshotProviderBitmap
    : public CanvasSnapshotProvider {
 public:
  static std::unique_ptr<CanvasNon2DSnapshotProviderBitmap> Create(
      const CanvasSnapshotProvider::Info& info);

  ~CanvasNon2DSnapshotProviderBitmap() override;

  static scoped_refptr<StaticBitmapImage> DoExternalDrawAndSnapshot(
      const CanvasSnapshotProvider::Info& info,
      base::FunctionRef<void(cc::PaintCanvas&)> draw_callback,
      ImageOrientation orientation,
      sk_sp<SkSurface> client_provided_surface = nullptr);

  // CanvasSnapshotProvider:
  bool IsGpuContextLost() const override;
  bool IsValid() const override;
  bool IsAccelerated() const override { return false; }
  bool IsExternalBitmapProvider() const override { return true; }
  viz::SharedImageFormat GetSharedImageFormat() const override {
    return info_.format;
  }
  gfx::ColorSpace GetColorSpace() const override { return info_.color_space; }
  SkAlphaType GetAlphaType() const override { return info_.alpha_type; }
  gfx::Size Size() const override { return info_.size; }
  const CanvasSnapshotProvider::Info& Info() const { return info_; }

  static sk_sp<SkSurface> CreateSurface(
      const CanvasSnapshotProvider::Info& info);
  sk_sp<SkSurface> GetCachedSurface();

 private:
  explicit CanvasNon2DSnapshotProviderBitmap(
      const CanvasSnapshotProvider::Info& info);

  // Used for any images that clients pass to cc::PaintCanvas::DrawImage() in
  // the invocation of the `draw_callback` that clients provide to
  // `DoExternalDrawAndSnapshot()`.
  class ImageProviderImpl : public cc::ImageProvider {
   public:
    ImageProviderImpl(bool is_f16, const gfx::ColorSpace& color_space);
    ~ImageProviderImpl() override = default;

    // cc::ImageProvider:
    cc::ImageProvider::ScopedResult GetRasterContent(
        const cc::DrawImage& draw_image) override;

   private:
    bool is_f16_;
    gfx::ColorSpace color_space_;
  };

  sk_sp<SkSurface> surface_;
  const CanvasSnapshotProvider::Info info_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_NON2D_SNAPSHOT_PROVIDER_BITMAP_H_
