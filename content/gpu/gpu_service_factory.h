// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_SERVICE_FACTORY_H_
#define CONTENT_GPU_GPU_SERVICE_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/android_overlay_mojo_factory.h"
#include "media/mojo/buildflags.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace gpu {
class GpuMemoryBufferFactory;
}

namespace media {
class MediaGpuChannelManager;
}

namespace content {

// Helper for handling incoming RunService requests on GpuChildThread.
class GpuServiceFactory {
 public:
  GpuServiceFactory(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const gpu::GPUInfo& gpu_info,
      base::WeakPtr<media::MediaGpuChannelManager> media_gpu_channel_manager,
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
      media::AndroidOverlayMojoFactoryCB android_overlay_factory_cb);

  GpuServiceFactory(const GpuServiceFactory&) = delete;
  GpuServiceFactory& operator=(const GpuServiceFactory&) = delete;

  ~GpuServiceFactory();

  void RunMediaService(
      mojo::PendingReceiver<media::mojom::MediaService> receiver);

 private:
#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  // Task runner we were constructed on, and that |media_gpu_channel_manager_|
  // must be accessed from (the GPU main thread task runner). We expect
  // RegisterServices() to be called on this task runner as well, but the
  // implementation doesn't care.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<media::MediaGpuChannelManager> media_gpu_channel_manager_;
  media::AndroidOverlayMojoFactoryCB android_overlay_factory_cb_;
  // Indirectly owned by GpuChildThread.
  raw_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;
  gpu::GpuPreferences gpu_preferences_;
  gpu::GpuDriverBugWorkarounds gpu_workarounds_;
  gpu::GpuFeatureInfo gpu_feature_info_;
  gpu::GPUInfo gpu_info_;
#endif
};

}  // namespace content

#endif  // CONTENT_GPU_GPU_SERVICE_FACTORY_H_
