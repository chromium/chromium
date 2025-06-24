// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_FAKE_CANVAS_RESOURCE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_FAKE_CANVAS_RESOURCE_HOST_H_

#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace cc {
class PaintCanvas;
}

namespace blink {

class FakeCanvasResourceHost : public CanvasResourceHost {
 public:
  explicit FakeCanvasResourceHost(gfx::Size size) : CanvasResourceHost(size) {}
  ~FakeCanvasResourceHost() override = default;
  void NotifyGpuContextLost() override {}
  bool IsContextLost() const override { return false; }
  void SetNeedsCompositingUpdate() override {}
  void InitializeForRecording(cc::PaintCanvas*) const override {}
  bool PrintedInCurrentTask() const override { return false; }
  bool IsPageVisible() const override { return page_visible_; }
  bool IsHibernating() const override { return is_hibernating_; }
  void SetIsHibernating(bool is_hibernating) {
    is_hibernating_ = is_hibernating;
  }

  CanvasResourceProvider* GetResourceProviderForCanvas2D() const override {
    return resource_provider_.get();
  }
  void ResetResourceProviderForCanvas2D() override {
    resource_provider_.reset();
  }

  CanvasResourceProvider* GetOrCreateCanvasResourceProviderForCanvas2D() {
    if (GetResourceProviderForCanvas2D()) {
      return GetResourceProviderForCanvas2D();
    }
    constexpr auto kShouldInitialize =
        CanvasResourceProvider::ShouldInitialize::kCallClear;
    constexpr gpu::SharedImageUsageSet kSharedImageUsageFlags =
        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ | gpu::SHARED_IMAGE_USAGE_SCANOUT;
    resource_provider_ = CanvasResourceProvider::CreateSharedImageProvider(
        Size(), GetN32FormatForCanvas(), kPremul_SkAlphaType,
        gfx::ColorSpace::CreateSRGB(), kShouldInitialize,
        SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
        kSharedImageUsageFlags, this);

    return resource_provider_.get();
  }

  void SetPageVisible(bool visible) {
    if (page_visible_ != visible) {
      page_visible_ = visible;
    }
  }

 private:
  std::unique_ptr<CanvasResourceProvider> resource_provider_;
  bool page_visible_ = true;
  bool is_hibernating_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEST_FAKE_CANVAS_RESOURCE_HOST_H_
