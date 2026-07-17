// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_IMAGE_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_IMAGE_PROVIDER_H_

#include <optional>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_image.h"
#include "cc/raster/playback_image_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace cc {
class ImageDecodeCache;
}

namespace gfx {
class ColorSpace;
}

namespace blink {
class WebGraphicsContext3DProviderWrapper;

class PLATFORM_EXPORT CanvasImageProvider : public cc::ImageProvider {
 public:
  CanvasImageProvider(cc::ImageDecodeCache* cache_n32,
                      cc::ImageDecodeCache* cache_f16,
                      const gfx::ColorSpace& target_color_space,
                      viz::SharedImageFormat canvas_format,
                      cc::PlaybackImageProvider::RasterMode raster_mode,
                      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
                          context_provider_wrapper);
  CanvasImageProvider(const CanvasImageProvider&) = delete;
  CanvasImageProvider& operator=(const CanvasImageProvider&) = delete;
  ~CanvasImageProvider() override;

  // cc::ImageProvider implementation.
  cc::ImageProvider::ScopedResult GetRasterContent(
      const cc::DrawImage&) override;

  void ReleaseLockedImages() { locked_images_.clear(); }
  void UnbindTextureBackedImages();
  void SetAnimatedImageFrameIndexes(
      scoped_refptr<const cc::AnimatedImageFrameIndexMap> indexes);

 private:
  void CanUnlockImage(ScopedResult);
  void CleanupLockedImages();
  bool IsHardwareDecodeCache() const;

  cc::PlaybackImageProvider::RasterMode raster_mode_;
  bool cleanup_task_pending_ = false;
  Vector<ScopedResult> locked_images_;
  Vector<cc::PaintImage> bound_texture_backed_images_;
  std::optional<cc::PlaybackImageProvider> playback_image_provider_n32_;
  std::optional<cc::PlaybackImageProvider> playback_image_provider_f16_;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;

  base::WeakPtrFactory<CanvasImageProvider> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_IMAGE_PROVIDER_H_
