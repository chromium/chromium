// Copyright 2017 The Chromium Authors
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
  explicit FakeCanvasResourceHost(gfx::Size size) : size_(size) {}
  void NotifyGpuContextLost() override {}
  void SetNeedsCompositingUpdate() override {}
  void RestoreCanvasMatrixClipStack(cc::PaintCanvas*) const override {}
  void UpdateMemoryUsage() override {}
  bool PrintedInCurrentTask() const override { return false; }
  size_t GetMemoryUsage() const override { return 0; }
  CanvasResourceProvider* GetOrCreateCanvasResourceProvider(
      RasterModeHint hint) override {
    return GetOrCreateCanvasResourceProviderImpl(hint);
  }
  CanvasResourceProvider* GetOrCreateCanvasResourceProviderImpl(
      RasterModeHint hint) override {
    if (ResourceProvider())
      return ResourceProvider();
    const SkImageInfo resource_info =
        SkImageInfo::MakeN32Premul(size_.width(), size_.height());
    constexpr auto kFilterQuality = cc::PaintFlags::FilterQuality::kMedium;
    constexpr auto kShouldInitialize =
        CanvasResourceProvider::ShouldInitialize::kCallClear;
    std::unique_ptr<CanvasResourceProvider> provider;
    if (hint == RasterModeHint::kPreferGPU ||
        RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled()) {
      constexpr bool kIsOriginTopLeft = true;
      constexpr uint32_t kSharedImageUsageFlags =
          gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
          gpu::SHARED_IMAGE_USAGE_SCANOUT;
      provider = CanvasResourceProvider::CreateSharedImageProvider(
          resource_info, kFilterQuality, kShouldInitialize,
          SharedGpuContext::ContextProviderWrapper(),
          hint == RasterModeHint::kPreferGPU ? RasterMode::kGPU
                                             : RasterMode::kCPU,
          kIsOriginTopLeft, kSharedImageUsageFlags);
    }
    if (!provider) {
      provider = CanvasResourceProvider::CreateSharedBitmapProvider(
          resource_info, kFilterQuality, kShouldInitialize,
          nullptr /* dispatcher_weakptr */);
    }
    if (!provider) {
      provider = CanvasResourceProvider::CreateBitmapProvider(
          resource_info, kFilterQuality, kShouldInitialize);
    }

    ReplaceResourceProvider(std::move(provider));

    return ResourceProvider();
  }

 private:
  gfx::Size size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_FAKE_CANVAS_RESOURCE_HOST_H_
