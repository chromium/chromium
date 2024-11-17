// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_VIZ_GPU_CHANNEL_HOST_PROVIDER_H_
#define SERVICES_VIDEO_EFFECTS_VIZ_GPU_CHANNEL_HOST_PROVIDER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/video_effects/video_effects_processor_impl.h"
#include "services/viz/public/cpp/gpu/gpu.h"

namespace video_effects {

class VizGpuChannelHostProvider : public video_effects::GpuChannelHostProvider {
 public:
  explicit VizGpuChannelHostProvider(std::unique_ptr<viz::Gpu> viz_gpu);
  ~VizGpuChannelHostProvider() override;

  // GpuChannelHostProvider:
  scoped_refptr<viz::ContextProviderCommandBuffer> GetWebGpuContextProvider()
      override;
  scoped_refptr<viz::RasterContextProvider> GetRasterInterfaceContextProvider()
      override;
  scoped_refptr<gpu::ClientSharedImageInterface> GetSharedImageInterface()
      override;

 protected:
  scoped_refptr<gpu::GpuChannelHost> GetGpuChannelHost() override;

 private:
  std::unique_ptr<viz::Gpu> viz_gpu_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_;
  scoped_refptr<viz::ContextProviderCommandBuffer> webgpu_context_provider_;
  scoped_refptr<viz::ContextProviderCommandBuffer>
      raster_interface_context_provider_;
  scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface_;
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_VIZ_GPU_CHANNEL_HOST_PROVIDER_H_
