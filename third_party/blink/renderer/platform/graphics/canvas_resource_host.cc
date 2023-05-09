// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_host.h"

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

namespace blink {

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

}  // namespace blink
