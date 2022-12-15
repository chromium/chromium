// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_

#include <memory>

#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "ui/gfx/hdr_metadata.h"

namespace cc {
class PaintCanvas;
}

namespace blink {
class CanvasResourceProvider;

class PLATFORM_EXPORT CanvasResourceHost {
 public:
  virtual ~CanvasResourceHost() = default;
  virtual void NotifyGpuContextLost() = 0;
  virtual void SetNeedsCompositingUpdate() = 0;
  virtual void RestoreCanvasMatrixClipStack(cc::PaintCanvas*) const = 0;
  virtual void UpdateMemoryUsage() = 0;
  virtual size_t GetMemoryUsage() const = 0;
  virtual CanvasResourceProvider* GetOrCreateCanvasResourceProvider(
      RasterModeHint hint) = 0;
  virtual CanvasResourceProvider* GetOrCreateCanvasResourceProviderImpl(
      RasterModeHint hint) = 0;

  virtual void SetFilterQuality(cc::PaintFlags::FilterQuality filter_quality);
  cc::PaintFlags::FilterQuality FilterQuality() const {
    return filter_quality_;
  }
  void SetHDRConfiguration(
      gfx::HDRMode hdr_mode,
      const absl::optional<gfx::HDRMetadata>& hdr_metadata) {
    hdr_mode_ = hdr_mode;
    hdr_metadata_ = hdr_metadata;
  }
  gfx::HDRMode GetHDRMode() const { return hdr_mode_; }
  const absl::optional<gfx::HDRMetadata>& GetHDRMetadata() const {
    return hdr_metadata_;
  }

  virtual bool LowLatencyEnabled() const { return false; }

  CanvasResourceProvider* ResourceProvider() const;

  std::unique_ptr<CanvasResourceProvider> ReplaceResourceProvider(
      std::unique_ptr<CanvasResourceProvider>);

  virtual void DiscardResourceProvider();

  virtual bool IsPrinting() const { return false; }
  virtual bool PrintedInCurrentTask() const = 0;

 private:
  void InitializeForRecording(cc::PaintCanvas* canvas);

  std::unique_ptr<CanvasResourceProvider> resource_provider_;
  cc::PaintFlags::FilterQuality filter_quality_ =
      cc::PaintFlags::FilterQuality::kLow;
  gfx::HDRMode hdr_mode_ = gfx::HDRMode::kDefault;
  absl::optional<gfx::HDRMetadata> hdr_metadata_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_HOST_H_
