// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"

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
         !context_provider_wrapper->ContextProvider()->IsContextLost();
}

}  // namespace

CanvasResourceHost::CanvasResourceHost(gfx::Size size) : size_(size) {}

CanvasResourceHost::~CanvasResourceHost() = default;

CanvasResourceProvider* CanvasResourceHost::ResourceProvider() const {
  return resource_provider_.get();
}

std::unique_ptr<CanvasResourceProvider>
CanvasResourceHost::ReplaceResourceProvider(
    std::unique_ptr<CanvasResourceProvider> new_resource_provider) {
  std::unique_ptr<CanvasResourceProvider> old_resource_provider =
      std::move(resource_provider_);
  resource_provider_ = std::move(new_resource_provider);
  UpdateMemoryUsage();
  if (resource_provider_) {
    resource_provider_->SetCanvasResourceHost(this);
    resource_provider_->Canvas()->restoreToCount(1);
    InitializeForRecording(resource_provider_->Canvas());
    // Using unretained here since CanvasResourceHost owns |resource_provider_|
    // and is guaranteed to outlive it
    resource_provider_->SetRestoreClipStackCallback(base::BindRepeating(
        &CanvasResourceHost::InitializeForRecording, base::Unretained(this)));
  }
  if (old_resource_provider) {
    old_resource_provider->SetCanvasResourceHost(nullptr);
    old_resource_provider->SetRestoreClipStackCallback(
        CanvasResourceProvider::RestoreMatrixClipStackCb());
  }
  return old_resource_provider;
}

void CanvasResourceHost::DiscardResourceProvider() {
  resource_provider_ = nullptr;
  UpdateMemoryUsage();
}

void CanvasResourceHost::InitializeForRecording(cc::PaintCanvas* canvas) {
  RestoreCanvasMatrixClipStack(canvas);
}

void CanvasResourceHost::SetFilterQuality(
    cc::PaintFlags::FilterQuality filter_quality) {
  filter_quality_ = filter_quality;
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

  if (UNLIKELY(!resource_provider_)) {
    return false;
  }

  return resource_provider_->SupportsDirectCompositing() &&
         !LowLatencyEnabled();
}

bool CanvasResourceHost::IsHibernating() const {
  return false;
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

}  // namespace blink
