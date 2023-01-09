// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_video_encode_accelerator_provider.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
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
    scoped_refptr<base::SingleThreadTaskRunner> runner) {
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
  MojoVideoEncodeAcceleratorService::Create(
      std::move(receiver), create_vea_callback_, gpu_preferences_,
      gpu_workarounds_, gpu_device_);
}

void MojoVideoEncodeAcceleratorProvider::
    GetVideoEncodeAcceleratorSupportedProfiles(
        GetVideoEncodeAcceleratorSupportedProfilesCallback callback) {
  std::move(callback).Run(
      GpuVideoEncodeAcceleratorFactory::GetSupportedProfiles(
          gpu_preferences_, gpu_workarounds_, gpu_device_));
}

}  // namespace media
