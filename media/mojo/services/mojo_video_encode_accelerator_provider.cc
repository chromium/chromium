// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_video_encode_accelerator_provider.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace {
void BindVEAProvider(
    mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
        receiver,
    media::MojoVideoEncodeAcceleratorProvider::
        CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
    gpu::GpuPreferences gpu_preferences,
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    const gpu::GPUInfo::GPUDevice& gpu_device) {
  auto vea_provider =
      std::make_unique<media::MojoVideoEncodeAcceleratorProvider>(
          std::move(create_vea_callback), gpu_preferences, gpu_workarounds,
          gpu_device);
  mojo::MakeSelfOwnedReceiver(std::move(vea_provider), std::move(receiver));
}
}  // namespace

namespace media {

// static
void MojoVideoEncodeAcceleratorProvider::Create(
    mojo::PendingReceiver<mojom::VideoEncodeAcceleratorProvider> receiver,
    CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GPUInfo::GPUDevice& gpu_device,
    scoped_refptr<base::SequencedTaskRunner> runner) {
  DCHECK(runner);
  runner->PostTask(
      FROM_HERE, base::BindOnce(BindVEAProvider, std::move(receiver),
                                std::move(create_vea_callback), gpu_preferences,
                                gpu_workarounds, gpu_device));
}

MojoVideoEncodeAcceleratorProvider::MojoVideoEncodeAcceleratorProvider(
    CreateAndInitializeVideoEncodeAcceleratorCallback create_vea_callback,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    const gpu::GPUInfo::GPUDevice& gpu_device)
    : create_vea_callback_(std::move(create_vea_callback)),
      gpu_preferences_(gpu_preferences),
      gpu_workarounds_(gpu_workarounds),
      gpu_device_(gpu_device) {}

MojoVideoEncodeAcceleratorProvider::~MojoVideoEncodeAcceleratorProvider() =
    default;

void MojoVideoEncodeAcceleratorProvider::CreateVideoEncodeAccelerator(
    mojo::PendingReceiver<mojom::VideoEncodeAccelerator> receiver) {
  auto create_service_cb = base::BindOnce(
      &MojoVideoEncodeAcceleratorService::Create, std::move(receiver),
      create_vea_callback_, gpu_preferences_, gpu_workarounds_, gpu_device_);

  if (base::FeatureList::IsEnabled(kUseTaskRunnerForMojoVEAService)) {
#if BUILDFLAG(IS_WIN)
    base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED)
#else
    base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
#endif
        ->PostTask(FROM_HERE, std::move(create_service_cb));
  } else {
    std::move(create_service_cb).Run();
  }
}

void MojoVideoEncodeAcceleratorProvider::
    GetVideoEncodeAcceleratorSupportedProfiles(
        GetVideoEncodeAcceleratorSupportedProfilesCallback callback) {
  std::move(callback).Run(
      GpuVideoEncodeAcceleratorFactory::GetSupportedProfiles(
          gpu_preferences_, gpu_workarounds_, gpu_device_));
}

}  // namespace media
