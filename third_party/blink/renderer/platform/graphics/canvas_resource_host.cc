// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_context_rate_limiter.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"

namespace blink {

namespace {

constexpr unsigned kMaxCanvasAnimationBacklog = 2;

bool CanUseGPU() {
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper =
      SharedGpuContext::ContextProviderWrapper();
  return context_provider_wrapper &&
         !context_provider_wrapper->ContextProvider().IsContextLost();
}

}  // namespace

CanvasResourceHost::CanvasResourceHost(gfx::Size size) : size_(size) {}

CanvasResourceHost::~CanvasResourceHost() {
  ResetLayer();
}

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

void CanvasResourceHost::SetIsDisplayed(bool displayed) {
  is_displayed_ = displayed;
  // If the canvas is no longer being displayed, stop using the rate
  // limiter.
  if (!is_displayed_) {
    frames_since_last_commit_ = 0;
    if (rate_limiter_) {
      rate_limiter_->Reset();
      rate_limiter_.reset(nullptr);
    }
  }
}

SharedContextRateLimiter* CanvasResourceHost::RateLimiter() const {
  return rate_limiter_.get();
}

void CanvasResourceHost::CreateRateLimiter() {
  rate_limiter_ =
      std::make_unique<SharedContextRateLimiter>(kMaxCanvasAnimationBacklog);
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

void CanvasResourceHost::ResetLayer() {
  if (cc_layer_) {
    // Orphaning the layer is required to trigger the recreation of a new
    // layer in the case where destruction is caused by a canvas resize. Test:
    // virtual/gpu/fast/canvas/canvas-resize-after-paint-without-layout.html
    cc_layer_->RemoveFromParent();
    cc_layer_->ClearClient();
    cc_layer_ = nullptr;
  }
}

void CanvasResourceHost::ClearLayerTexture() {
  if (cc_layer_) {
    cc_layer_->ClearTexture();
  }
}

void CanvasResourceHost::SetNeedsPushProperties() {
  if (cc_layer_) {
    cc_layer_->SetNeedsSetTransferableResource();
  }
}

void CanvasResourceHost::DoPaintInvalidation(const gfx::Rect& dirty_rect) {
  if (cc_layer_ && IsComposited()) {
    cc_layer_->SetNeedsDisplayRect(dirty_rect);
  }
}

void CanvasResourceHost::SetOpacityMode(OpacityMode opacity_mode) {
  is_opaque_ = opacity_mode == kOpaque;
  if (cc_layer_) {
    cc_layer_->SetContentsOpaque(is_opaque_);
    cc_layer_->SetBlendBackgroundColor(!is_opaque_);
  }
}

void CanvasResourceHost::FlushRecording(FlushReason reason) {
  if (resource_provider_) {
    resource_provider_->FlushCanvas(reason);
  }
}

bool CanvasResourceHost::IsResourceValid() {
  if (IsHibernating()) {
    return true;
  }

  if (IsContextLost()) {
    return false;
  }

  if (resource_provider_ && !resource_provider_->IsValid()) {
    return false;
  }

  return !!GetOrCreateCanvasResourceProvider();
}

}  // namespace blink
