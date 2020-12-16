// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_FAKE_CANVAS_RESOURCE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_FAKE_CANVAS_RESOURCE_HOST_H_

#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace cc {
class PaintCanvas;
}

namespace blink {

class FakeCanvasResourceHost : public CanvasResourceHost {
 public:
  FakeCanvasResourceHost(IntSize size) : size_(size) {}
  ~FakeCanvasResourceHost() override {}
  void NotifyGpuContextLost() override {}
  void SetNeedsCompositingUpdate() override {}
  void RestoreCanvasMatrixClipStack(cc::PaintCanvas*) const override {}
  void UpdateMemoryUsage() override {}
  CanvasResourceProvider* GetOrCreateCanvasResourceProvider(
      RasterModeHint hint) override {
    return GetOrCreateCanvasResourceProviderImpl(hint);
  }
  CanvasResourceProvider* GetOrCreateCanvasResourceProviderImpl(
      RasterModeHint hint) override {
    if (ResourceProvider())
      return ResourceProvider();

    std::unique_ptr<CanvasResourceProvider> provider;
    if (hint == RasterModeHint::kPreferGPU ||
        RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled()) {
      uint32_t shared_image_usage_flags =
          gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_SCANOUT;
      provider = CanvasResourceProvider::CreateSharedImageProvider(
          size_, kMedium_SkFilterQuality, CanvasResourceParams(),
          CanvasResourceProvider::ShouldInitialize::kCallClear,
          SharedGpuContext::ContextProviderWrapper(),
          hint == RasterModeHint::kPreferGPU ? RasterMode::kGPU
                                             : RasterMode::kCPU,
          false /*is_origin_top_left*/, shared_image_usage_flags);
    }
    if (!provider) {
      provider = CanvasResourceProvider::CreateSharedBitmapProvider(
          size_, kMedium_SkFilterQuality, CanvasResourceParams(),
          CanvasResourceProvider::ShouldInitialize::kCallClear,
          nullptr /* dispatcher_weakptr */);
    }
    if (!provider) {
      provider = CanvasResourceProvider::CreateBitmapProvider(
          size_, kMedium_SkFilterQuality, CanvasResourceParams(),
          CanvasResourceProvider::ShouldInitialize::kCallClear);
    }

    ReplaceResourceProvider(std::move(provider));

    return ResourceProvider();
  }

  SkFilterQuality FilterQuality() const override {
    return kLow_SkFilterQuality;
  }

 private:
  IntSize size_;
};

}  // namespace blink

#endif
