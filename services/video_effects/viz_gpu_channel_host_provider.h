// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_VIZ_GPU_CHANNEL_HOST_PROVIDER_H_
#define SERVICES_VIDEO_EFFECTS_VIZ_GPU_CHANNEL_HOST_PROVIDER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "services/video_effects/gpu_channel_host_provider.h"
#include "services/viz/public/cpp/gpu/gpu.h"

namespace video_effects {

class VizGpuChannelHostProvider : public GpuChannelHostProvider,
                                  public gpu::GpuChannelLostObserver,
                                  public viz::ContextLostObserver {
 public:
  explicit VizGpuChannelHostProvider(std::unique_ptr<viz::Gpu> viz_gpu);

  // GpuChannelHostProvider:
  scoped_refptr<viz::ContextProviderCommandBuffer> GetWebGpuContextProvider()
      override;
  scoped_refptr<viz::RasterContextProvider> GetRasterInterfaceContextProvider()
      override;
  scoped_refptr<gpu::ClientSharedImageInterface> GetSharedImageInterface()
      override;
  void AddObserver(Observer& observer) override;
  void RemoveObserver(Observer& observer) override;

 protected:
  ~VizGpuChannelHostProvider() override;
  scoped_refptr<gpu::GpuChannelHost> GetGpuChannelHost() override;

 private:
  void Reset();

  // gpu::GpuChannelLostObserver:
  void OnGpuChannelLost() override;

  // viz::ContextLostObserver:
  void OnContextLost() override;

  // Called by `OnGpuChannelLost()` and `OnContextLost()`.
  void HandleContextLost();

  bool HasPermanentError();

  std::unique_ptr<viz::Gpu> viz_gpu_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_;
  scoped_refptr<viz::ContextProviderCommandBuffer> webgpu_context_provider_;
  scoped_refptr<viz::ContextProviderCommandBuffer>
      raster_interface_context_provider_;
  scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface_;

  int num_context_lost_ = 0;
  base::RepeatingClosure task_gpu_channel_lost_on_provider_thread_;

  base::ObserverList<Observer, true /*check_empty*/, false /*allow_reentrancy*/>
      observers_;

  base::WeakPtrFactory<VizGpuChannelHostProvider> weak_ptr_factory_{this};
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_VIZ_GPU_CHANNEL_HOST_PROVIDER_H_
