// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_

#include <memory>

#include "cc/layers/texture_layer.h"
#include "cc/layers/texture_layer_client.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace cc {
class PaintCanvas;
}  // namespace cc

namespace blink {

class CanvasResourceProvider;
class SharedContextRateLimiter;

class PLATFORM_EXPORT CanvasResourceHost : public cc::TextureLayerClient {
 public:
  explicit CanvasResourceHost(gfx::Size size);
  ~CanvasResourceHost() override;

  // cc::TextureLayerClient implementation.
  bool PrepareTransferableResource(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* out_resource,
      viz::ReleaseCallback* out_release_callback) override;

  virtual void NotifyGpuContextLost() = 0;
  virtual void SetNeedsCompositingUpdate() = 0;
  virtual void InitializeForRecording(cc::PaintCanvas* canvas) const = 0;
  virtual void UpdateMemoryUsage() = 0;
  virtual size_t GetMemoryUsage() const = 0;
  virtual void PageVisibilityChanged() {}
  virtual CanvasResourceProvider* GetOrCreateCanvasResourceProvider(
      RasterModeHint hint) = 0;
  virtual CanvasResourceProvider* GetOrCreateCanvasResourceProviderImpl(
      RasterModeHint hint) = 0;
  bool IsComposited() const;
  bool IsResourceValid();
  gfx::Size Size() const { return size_; }
  virtual void SetSize(gfx::Size size) { size_ = size; }

  virtual void SetFilterQuality(cc::PaintFlags::FilterQuality filter_quality);
  cc::PaintFlags::FilterQuality FilterQuality() const {
    return filter_quality_;
  }
  void SetHdrMetadata(const gfx::HDRMetadata& hdr_metadata);
  const gfx::HDRMetadata& GetHDRMetadata() const { return hdr_metadata_; }

  virtual bool LowLatencyEnabled() const { return false; }

  CanvasResourceProvider* ResourceProvider() const {
    return resource_provider_.get();
  }

  void FlushRecording(FlushReason reason);

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
  void AlwaysEnableRasterTimersForTesting() {
    always_enable_raster_timers_for_testing_ = true;
  }

  // Actual RasterMode used for rendering 2d primitives.
  RasterMode GetRasterMode() const;
  void ResetLayer();
  void ClearLayerTexture();
  void SetNeedsPushProperties();
  cc::TextureLayer* GetOrCreateCcLayerIfNeeded();
  cc::TextureLayer* CcLayer() { return cc_layer_.get(); }
  void DoPaintInvalidation(const gfx::Rect& dirty_rect);
  void SetOpacityMode(OpacityMode opacity_mode);

  // Temporary, for canvas_2d_layer_bridge use.
  bool context_lost() { return context_lost_; }
  void set_context_lost(bool value) { context_lost_ = value; }

  bool shared_bitmap_gpu_channel_lost() const {
    return shared_bitmap_gpu_channel_lost_;
  }
  void set_shared_bitmap_gpu_channel_lost(bool value) {
    shared_bitmap_gpu_channel_lost_ = value;
  }

  void SetTransferToGPUTextureWasInvoked() {
    transfer_to_gpu_texture_was_invoked_ = true;
  }
  bool TransferToGPUTextureWasInvoked() {
    return transfer_to_gpu_texture_was_invoked_;
  }

 private:
  bool is_displayed_ = false;
  bool context_lost_ = false;
  bool shared_bitmap_gpu_channel_lost_ = false;
  unsigned frames_since_last_commit_ = 0;
  std::unique_ptr<SharedContextRateLimiter> rate_limiter_;
  std::unique_ptr<CanvasResourceProvider> resource_provider_;
  cc::PaintFlags::FilterQuality filter_quality_ =
      cc::PaintFlags::FilterQuality::kLow;
  gfx::HDRMetadata hdr_metadata_;
  RasterModeHint preferred_2d_raster_mode_ = RasterModeHint::kPreferCPU;
  gfx::Size size_;
  bool always_enable_raster_timers_for_testing_ = false;
  scoped_refptr<cc::TextureLayer> cc_layer_;
  OpacityMode opacity_mode_ = kNonOpaque;
  bool transfer_to_gpu_texture_was_invoked_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_
