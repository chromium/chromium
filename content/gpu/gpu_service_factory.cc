// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "media/base/media_switches.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "base/functional/bind.h"
#include "media/base/media_switches.h"
#include "media/mojo/services/gpu_mojo_media_client.h"  // nogncheck
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
  scoped_refptr<base::SequencedTaskRunner> task_runner = task_runner_;
  const bool use_dedicated_media_service_thread = [&]() {
#if BUILDFLAG(IS_WIN)
    bool is_dawn_d3d_enabled = false;
    // On Windows, check if we are using the Graphite D3D backend. There
    // is rendering performance impact caused by lock contention on the
    // D3D11 API lock, and Dawn mitigate this by batching API calls under
    // the lock.
    if (media_gpu_channel_manager_) {
      if (auto shared_context_state =
              media_gpu_channel_manager_->GetSharedContextState()) {
        is_dawn_d3d_enabled = shared_context_state->IsGraphiteDawnD3D();
      }
    }
    if (!is_dawn_d3d_enabled) {
      return false;
    }
#endif

    return media::IsDedicatedMediaServiceThreadEnabled(
        gpu_info_.gl_implementation_parts.angle);
  }();

  if (use_dedicated_media_service_thread) {
    // This service will host audio/video decoders, and if these decoding
    // operations are blocked, user may hear audio glitch or see video
    // freezing, hence "user blocking".
    if (base::FeatureList::IsEnabled(
            media::kUseSequencedTaskRunnerForMediaService)) {
      task_runner = base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING});
    } else {
      task_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_BLOCKING});
    }
  }

  media::GpuMojoMediaClientTraits traits(
      gpu_preferences_, gpu_workarounds_, gpu_feature_info_, gpu_info_,
      /*gpu_task_runner=*/task_runner_, android_overlay_factory_cb_,
      media_gpu_channel_manager_);
  auto gpu_client = media::GpuMojoMediaClient::Create(traits);

  using FactoryCallback =
      base::OnceCallback<std::unique_ptr<media::MediaService>()>;
  FactoryCallback factory =
      base::BindOnce(&media::CreateGpuMediaService, std::move(receiver),
                     std::move(gpu_client));
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
