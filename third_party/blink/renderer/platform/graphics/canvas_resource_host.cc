// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"

namespace blink {

namespace {

bool CanUseGPU() {
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  return context_provider_wrapper &&
         !context_provider_wrapper->ContextProvider().IsContextLost();
}

}  // namespace

CanvasResourceHost::CanvasResourceHost(gfx::Size size) : size_(size) {}

CanvasResourceHost::~CanvasResourceHost() = default;

std::unique_ptr<CanvasResourceProvider>
CanvasResourceHost::ReplaceResourceProvider(
    std::unique_ptr<CanvasResourceProvider> new_resource_provider) {
  std::unique_ptr<CanvasResourceProvider> old_resource_provider =
      std::move(resource_provider_);
  resource_provider_ = std::move(new_resource_provider);
  UpdateMemoryUsage();
  if (resource_provider_) {
    resource_provider_->AlwaysEnableRasterTimersForTesting(
        always_enable_raster_timers_for_testing_);
  }
  if (old_resource_provider) {
    old_resource_provider->SetCanvasResourceHost(nullptr);
  }
  return old_resource_provider;
}

void CanvasResourceHost::DiscardResourceProvider() {
  resource_provider_ = nullptr;
  UpdateMemoryUsage();
}

void CanvasResourceHost::SetPreferred2DRasterMode(RasterModeHint hint) {
  // TODO(junov): move code that switches between CPU and GPU rasterization
  // to here.
  preferred_2d_raster_mode_ = hint;
}

bool CanvasResourceHost::ShouldTryToUseGpuRaster() const {
  return preferred_2d_raster_mode_ == RasterModeHint::kPreferGPU && CanUseGPU();
}

bool CanvasResourceHost::IsComposited() const {
  if (IsHibernating()) {
    return false;
  }

  if (!resource_provider_) [[unlikely]] {
    return false;
  }

  return resource_provider_->SupportsDirectCompositing() &&
         !LowLatencyEnabled();
}

RasterMode CanvasResourceHost::GetRasterMode() const {
  if (IsHibernating()) {
    return RasterMode::kCPU;
  }
  if (resource_provider_) {
    return resource_provider_->IsAccelerated() ? RasterMode::kGPU
                                               : RasterMode::kCPU;
  }

  // Whether or not to accelerate is not yet resolved, the canvas cannot be
  // accelerated if the gpu context is lost.
  return ShouldTryToUseGpuRaster() ? RasterMode::kGPU : RasterMode::kCPU;
}

void CanvasResourceHost::FlushRecording(FlushReason reason) {
  if (resource_provider_) {
    resource_provider_->FlushCanvas(reason);
  }
}

}  // namespace blink
