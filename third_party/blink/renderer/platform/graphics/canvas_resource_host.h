// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_

#include <memory>

#include "cc/layers/texture_layer.h"
#include "third_party/blink/renderer/platform/graphics/flush_reason.h"
#include "third_party/blink/renderer/platform/graphics/opacity_mode.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace cc {
class PaintCanvas;
}  // namespace cc

namespace blink {

class CanvasResourceProvider;

// Specifies whether the provider should rasterize paint commands on the CPU
// or GPU. This is used to support software raster with GPU compositing.
enum class RasterMode {
  kGPU,
  kCPU,
};

enum class RasterModeHint {
  kPreferGPU,
  kPreferCPU,
};

class PLATFORM_EXPORT CanvasResourceHost {
 public:
  explicit CanvasResourceHost(gfx::Size size);
  virtual ~CanvasResourceHost();

  virtual void NotifyGpuContextLost() = 0;
  // TODO(crbug.com/399587138): Delete once `cc::Layer` related code is moved to
  // `CanvasRenderingContext2D`. `IsContextLost()` is only needed by
  // `IsResourceValid()`, which is needed by `PrepareTransferableResource()`,
  // which is needed by cc_layer_, which is only used by
  // CanvasRenderingContext2D.
  virtual bool IsContextLost() const = 0;
  virtual void SetNeedsCompositingUpdate() = 0;
  virtual void InitializeForRecording(cc::PaintCanvas* canvas) const = 0;
  virtual void UpdateMemoryUsage() = 0;
  virtual size_t GetMemoryUsage() const = 0;
  virtual void PageVisibilityChanged() {}
  virtual CanvasResourceProvider* GetOrCreateCanvasResourceProvider() = 0;

  // Initialize the indicated cc::Layer with the HTMLCanvasElement's CSS
  // properties. This is a no-op if `this` is not an HTMLCanvasElement.
  virtual void InitializeLayerWithCSSProperties(cc::Layer* layer) {}

  bool IsComposited() const;
  gfx::Size Size() const { return size_; }
  virtual void SetSize(gfx::Size size) { size_ = size; }

  virtual bool LowLatencyEnabled() const { return false; }

  CanvasResourceProvider* ResourceProvider() const {
    return resource_provider_.get();
  }

  void FlushRecording(FlushReason reason);

  std::unique_ptr<CanvasResourceProvider> ReplaceResourceProvider(
      std::unique_ptr<CanvasResourceProvider>);

  virtual void DiscardResourceProvider();

  virtual bool IsPageVisible() const = 0;

  virtual bool IsPrinting() const { return false; }
  virtual bool PrintedInCurrentTask() const = 0;
  virtual bool IsHibernating() const { return false; }

  bool ShouldTryToUseGpuRaster() const;
  void SetPreferred2DRasterMode(RasterModeHint);

  void AlwaysEnableRasterTimersForTesting() {
    always_enable_raster_timers_for_testing_ = true;
  }

  // Actual RasterMode used for rendering 2d primitives.
  RasterMode GetRasterMode() const;

  // Called when the CC texture layer that this instance is holding (if any)
  // should be cleared. Subclasses that can hold a CC texture layer should
  // override this method.
  virtual void ClearLayerTexture() {}

  virtual void SetTransferToGPUTextureWasInvoked() {}
  virtual bool TransferToGPUTextureWasInvoked() { return false; }

 protected:
  virtual CanvasResourceProvider* GetOrCreateCanvasResourceProviderImpl() = 0;

 private:
  std::unique_ptr<CanvasResourceProvider> resource_provider_;
  RasterModeHint preferred_2d_raster_mode_ = RasterModeHint::kPreferCPU;
  gfx::Size size_;
  bool always_enable_raster_timers_for_testing_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_
