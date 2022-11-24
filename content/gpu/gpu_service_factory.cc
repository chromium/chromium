// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "base/bind.h"
#include "media/mojo/services/media_service_factory.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)

namespace content {

GpuServiceFactory::GpuServiceFactory(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const gpu::GPUInfo& gpu_info,
    base::WeakPtr<media::MediaGpuChannelManager> media_gpu_channel_manager,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    media::AndroidOverlayMojoFactoryCB android_overlay_factory_cb) {
#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  gpu_preferences_ = gpu_preferences;
  gpu_workarounds_ = gpu_workarounds;
  gpu_feature_info_ = gpu_feature_info;
  gpu_info_ = gpu_info;
  task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  media_gpu_channel_manager_ = std::move(media_gpu_channel_manager);
  gpu_memory_buffer_factory_ = gpu_memory_buffer_factory;
  android_overlay_factory_cb_ = std::move(android_overlay_factory_cb);
#endif
}

GpuServiceFactory::~GpuServiceFactory() {}

void GpuServiceFactory::RunMediaService(
    mojo::PendingReceiver<media::mojom::MediaService> receiver) {
#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  // This service will host audio/video decoders, and if these decoding
  // operations are blocked, user may hear audio glitch or see video freezing,
  // hence "user blocking".
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;
#if BUILDFLAG(IS_WIN)
  // Run everything on the gpu main thread, since it's required for decode swap
  // chains. See SwapChainPresenter::TryPresentToDecodeSwapChain().
  task_runner = task_runner_;
#else
  // TODO(crbug.com/786169): Check whether this needs to be single threaded.
  task_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_BLOCKING});
#endif  // BUILDFLAG(IS_WIN)

  using FactoryCallback =
      base::OnceCallback<std::unique_ptr<media::MediaService>()>;
  FactoryCallback factory =
      base::BindOnce(&media::CreateGpuMediaService, std::move(receiver),
                     gpu_preferences_, gpu_workarounds_, gpu_feature_info_,
                     gpu_info_, task_runner_, media_gpu_channel_manager_,
                     gpu_memory_buffer_factory_, android_overlay_factory_cb_);
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](FactoryCallback factory) {
            static base::NoDestructor<std::unique_ptr<media::MediaService>>
                service{std::move(factory).Run()};
          },
          std::move(factory)));
  return;
#endif  // BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
}

}  // namespace content
