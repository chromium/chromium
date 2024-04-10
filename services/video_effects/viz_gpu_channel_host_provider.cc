// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/viz_gpu_channel_host_provider.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/video_effects/video_effects_service_impl.h"
#include "services/viz/public/cpp/gpu/gpu.h"

namespace video_effects {

VizGpuChannelHostProvider::VizGpuChannelHostProvider(
    std::unique_ptr<viz::Gpu> viz_gpu)
    : viz_gpu_(std::move(viz_gpu)) {
  CHECK(viz_gpu_);
}

VizGpuChannelHostProvider::~VizGpuChannelHostProvider() = default;

scoped_refptr<gpu::GpuChannelHost>
VizGpuChannelHostProvider::GetGpuChannelHost() {
  scoped_refptr<gpu::GpuChannelHost> result = viz_gpu_->GetGpuChannel();
  if (!result || result->IsLost()) {
    return viz_gpu_->EstablishGpuChannelSync();
  }
  return result;
}

}  // namespace video_effects
