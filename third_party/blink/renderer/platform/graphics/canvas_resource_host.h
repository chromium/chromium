// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_

#include <memory>

#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace cc {
class PaintCanvas;
}

namespace blink {
class CanvasResourceProvider;
class SharedContextRateLimiter;

class PLATFORM_EXPORT CanvasResourceHost {
 public:
  explicit CanvasResourceHost(gfx::Size size);
  virtual ~CanvasResourceHost();

  virtual void NotifyGpuContextLost() = 0;
  virtual void SetNeedsCompositingUpdate() = 0;
  virtual void RestoreCanvasMatrixClipStack(cc::PaintCanvas*) const = 0;
  virtual void UpdateMemoryUsage() = 0;
  virtual size_t GetMemoryUsage() const = 0;
  virtual void PageVisibilityChanged() {}
  virtual CanvasResourceProvider* GetOrCreateCanvasResourceProvider(
      RasterModeHint hint) = 0;
  virtual CanvasResourceProvider* GetOrCreateCanvasResourceProviderImpl(
      RasterModeHint hint) = 0;
  bool IsComposited() const;
  gfx::Size Size() const { return size_; }
  virtual void SetSize(gfx::Size size) { size_ = size; }

  virtual void SetFilterQuality(cc::PaintFlags::FilterQuality filter_quality);
  cc::PaintFlags::FilterQuality FilterQuality() const {
    return filter_quality_;
  }
  void SetHdrMetadata(const gfx::HDRMetadata& hdr_metadata) {
    hdr_metadata_ = hdr_metadata;
  }
  const gfx::HDRMetadata& GetHDRMetadata() const { return hdr_metadata_; }

  virtual bool LowLatencyEnabled() const { return false; }

  CanvasResourceProvider* ResourceProvider() const;

  std::unique_ptr<CanvasResourceProvider> ReplaceResourceProvider(
      std::unique_ptr<CanvasResourceProvider>);

  virtual void DiscardResourceProvider();

  void SetIsDisplayed(bool);
  bool IsDisplayed() { return is_displayed_; }

  virtual bool IsPageVisible() = 0;

  virtual bool IsPrinting() const { return false; }
  virtual bool PrintedInCurrentTask() const = 0;
  virtual bool IsHibernating() const { return false; }

  RasterModeHint preferred_2d_raster_mode() const {
    return preferred_2d_raster_mode_;
  }

  bool ShouldTryToUseGpuRaster() const;
  void SetPreferred2DRasterMode(RasterModeHint);

  // Temporary plumbing while relocating code from Canvas2DLayerBridge.
  SharedContextRateLimiter* RateLimiter() const;
  void CreateRateLimiter();
  unsigned IncrementFramesSinceLastCommit() {
    return ++frames_since_last_commit_;
  }
  void ResetFramesSinceLastCommit() { frames_since_last_commit_ = 0; }
  void AlwaysEnableRasterTimersForTesting() {
    always_enable_raster_timers_for_testing_ = true;
  }

  // Actual RasterMode used for rendering 2d primitives.
  RasterMode GetRasterMode() const;

 private:
  void InitializeForRecording(cc::PaintCanvas* canvas);

  bool is_displayed_ = false;
  unsigned frames_since_last_commit_ = 0;
  std::unique_ptr<SharedContextRateLimiter> rate_limiter_;
  std::unique_ptr<CanvasResourceProvider> resource_provider_;
  cc::PaintFlags::FilterQuality filter_quality_ =
      cc::PaintFlags::FilterQuality::kLow;
  gfx::HDRMetadata hdr_metadata_;
  RasterModeHint preferred_2d_raster_mode_ = RasterModeHint::kPreferCPU;
  gfx::Size size_;
  bool always_enable_raster_timers_for_testing_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_
