// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_NON2D_SNAPSHOT_PROVIDER_BITMAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_NON2D_SNAPSHOT_PROVIDER_BITMAP_H_

#include <memory>

#include "third_party/blink/renderer/platform/graphics/canvas_snapshot_provider.h"

namespace blink {

// Renders to a RAM-backed bitmap via an external (client-supplied) draw.
class PLATFORM_EXPORT CanvasNon2DSnapshotProviderBitmap
    : public CanvasSnapshotProvider {
 public:
  static std::unique_ptr<CanvasNon2DSnapshotProviderBitmap> Create(
      const CanvasSnapshotProvider::Info& info);

  ~CanvasNon2DSnapshotProviderBitmap() override;

  // CanvasSnapshotProvider:
  bool IsGpuContextLost() const override;
  bool IsValid() const override;
  bool IsExternalBitmapProvider() const override { return true; }
  viz::SharedImageFormat GetSharedImageFormat() const override {
    return info_.format;
  }
  gfx::ColorSpace GetColorSpace() const override { return info_.color_space; }
  SkAlphaType GetAlphaType() const override { return info_.alpha_type; }
  gfx::Size Size() const override { return info_.size; }
  const CanvasSnapshotProvider::Info& Info() const { return info_; }

 private:
  explicit CanvasNon2DSnapshotProviderBitmap(
      const CanvasSnapshotProvider::Info& info);

  const CanvasSnapshotProvider::Info info_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_NON2D_SNAPSHOT_PROVIDER_BITMAP_H_
