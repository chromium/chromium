// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_EFFECTS_GPU_CHANNEL_HOST_PROVIDER_H_
#define SERVICES_VIDEO_EFFECTS_GPU_CHANNEL_HOST_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"

namespace gpu {
class ClientSharedImageInterface;
class GpuChannelHost;
}  // namespace gpu

namespace viz {
class ContextProviderCommandBuffer;
class RasterContextProvider;
}  // namespace viz

namespace video_effects {

// Abstract interface that is used by `VideoEffectsServiceImpl` to obtain
// instances of `gpu::GpuChannelHost`. Those are then going to be used to
// create context providers over which the communication to GPU service will
// happen.
class GpuChannelHostProvider : public base::RefCounted<GpuChannelHostProvider> {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // The GPU context was lost.
    virtual void OnContextLost(scoped_refptr<GpuChannelHostProvider>) = 0;

    // Abandon ship! The GPU context has been lost multiple times and no further
    // attempts will be made to re-establish a connection to the GPU.
    virtual void OnPermanentError(scoped_refptr<GpuChannelHostProvider>) = 0;
  };

  // Returns the context provider for WebGPU.
  virtual scoped_refptr<viz::ContextProviderCommandBuffer>
  GetWebGpuContextProvider() = 0;

  // Returns the context provider for the raster interface.
  virtual scoped_refptr<viz::RasterContextProvider>
  GetRasterInterfaceContextProvider() = 0;

  // Returns the SharedImageInterface.
  virtual scoped_refptr<gpu::ClientSharedImageInterface>
  GetSharedImageInterface() = 0;

  virtual void AddObserver(Observer& observer) = 0;
  virtual void RemoveObserver(Observer& observer) = 0;

 protected:
  virtual ~GpuChannelHostProvider() = default;

  // Return a connected `gpu::GpuChannelHost`. Implementations should expect
  // this method to be called somewhat frequently when a new Video Effects
  // Service is created.
  virtual scoped_refptr<gpu::GpuChannelHost> GetGpuChannelHost() = 0;

 private:
  friend class base::RefCounted<GpuChannelHostProvider>;
};

}  // namespace video_effects

#endif  // SERVICES_VIDEO_EFFECTS_GPU_CHANNEL_HOST_PROVIDER_H_
