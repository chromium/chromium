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
      AccelerationHint hint) override {
    return GetOrCreateCanvasResourceProviderImpl(hint);
  }
  CanvasResourceProvider* GetOrCreateCanvasResourceProviderImpl(
      AccelerationHint hint) override {
    if (ResourceProvider())
      return ResourceProvider();

    CanvasResourceProvider::ResourceUsage usage =
        hint == kPreferAcceleration ? CanvasResourceProvider::ResourceUsage::
                                          kAcceleratedCompositedResourceUsage
                                    : CanvasResourceProvider::ResourceUsage::
                                          kSoftwareCompositedResourceUsage;

    uint8_t presentation_mode =
        CanvasResourceProvider::kDefaultPresentationMode;
    if (RuntimeEnabledFeatures::Canvas2dImageChromiumEnabled()) {
      presentation_mode |=
          CanvasResourceProvider::kAllowImageChromiumPresentationMode;
    }

    ReplaceResourceProvider(CanvasResourceProvider::Create(
        size_, usage, SharedGpuContext::ContextProviderWrapper(), 0,
        kMedium_SkFilterQuality, CanvasColorParams(), presentation_mode,
        nullptr));
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
