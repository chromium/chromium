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
#include "media/mojo/services/media_service_factory.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)

#if BUILDFLAG(IS_WIN)
#include <d3d11_4.h>
#endif

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
  // operations are blocked, user may hear audio glitch or see video
  // freezing, hence "user blocking".
  scoped_refptr<base::SequencedTaskRunner> task_runner = task_runner_;
  // D3D9 device doesn't support multi-treaded use.
  if (gpu_info_.gl_implementation_parts.angle !=
          gl::ANGLEImplementation::kD3D9 &&
      base::FeatureList::IsEnabled(media::kDedicatedMediaServiceThread)) {
    if (base::FeatureList::IsEnabled(
            media::kUseSequencedTaskRunnerForMediaService)) {
      task_runner = base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING});
    } else {
      task_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::TaskPriority::USER_BLOCKING});
    }
#if BUILDFLAG(IS_WIN)
    // Since the D3D11Device used for decoding is shared with SkiaRenderer(ANGLE
    // or Dawn), we need multithread protection turned on to use it from another
    // thread.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<media::MediaGpuChannelManager> manager) {
              CHECK(manager);
              gpu::ContextResult result;
              auto shared_context_state =
                  manager.get()->channel_manager()->GetSharedContextState(
                      &result);
              auto device = shared_context_state->GetD3D11Device();
              CHECK(device);
              Microsoft::WRL::ComPtr<ID3D11Multithread> multi_threaded;
              auto hr = device->QueryInterface(IID_PPV_ARGS(&multi_threaded));
              CHECK(SUCCEEDED(hr));
              multi_threaded->SetMultithreadProtected(TRUE);
            },
            media_gpu_channel_manager_));
#endif
  }

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
