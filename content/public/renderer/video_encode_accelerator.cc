// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/video_encode_accelerator.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "content/renderer/render_thread_impl.h"
#include "media/video/gpu_video_accelerator_factories.h"

namespace content {

void CreateVideoEncodeAccelerator(
    OnCreateVideoEncodeAcceleratorCallback callback) {
  DCHECK(!callback.is_null());

  media::GpuVideoAcceleratorFactories* gpu_factories =
      RenderThreadImpl::current()->GetGpuFactories();
  if (!gpu_factories || !gpu_factories->IsGpuVideoEncodeAcceleratorEnabled()) {
    std::move(callback).Run(nullptr,
                            std::unique_ptr<media::VideoEncodeAccelerator>());
    return;
  }

  scoped_refptr<base::SequencedTaskRunner> encode_task_runner =
      gpu_factories->GetTaskRunner();
  encode_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &media::GpuVideoAcceleratorFactories::CreateVideoEncodeAccelerator,
          base::Unretained(gpu_factories)),
      base::BindOnce(std::move(callback), encode_task_runner));
}

media::VideoEncodeAccelerator::SupportedProfiles
GetSupportedVideoEncodeAcceleratorProfiles() {
  // In https://crbug.com/664652, H264 HW accelerator is enabled on Android for
  // RTC by Default. Keep HW accelerator disabled for Cast as before at present.
#if BUILDFLAG(IS_ANDROID)
  return media::VideoEncodeAccelerator::SupportedProfiles();
#else
  media::GpuVideoAcceleratorFactories* gpu_factories =
      RenderThreadImpl::current()->GetGpuFactories();
  if (!gpu_factories || !gpu_factories->IsGpuVideoEncodeAcceleratorEnabled())
    return media::VideoEncodeAccelerator::SupportedProfiles();
  return gpu_factories->GetVideoEncodeAcceleratorSupportedProfiles().value_or(
      media::VideoEncodeAccelerator::SupportedProfiles());
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace content
