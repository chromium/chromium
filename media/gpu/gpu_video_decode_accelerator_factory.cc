// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/gpu_video_decode_accelerator_factory.h"

#include <memory>

#include "build/build_config.h"
#include "gpu/config/gpu_preferences.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/media_gpu_export.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(USE_V4L2_CODEC) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH))
#include "media/gpu/v4l2/legacy/v4l2_video_decode_accelerator.h"
#include "media/gpu/v4l2/v4l2_device.h"
#endif

namespace media {

// static
MEDIA_GPU_EXPORT std::unique_ptr<VideoDecodeAccelerator>
GpuVideoDecodeAcceleratorFactory::CreateVDA(
    VideoDecodeAccelerator::Client* client,
    const VideoDecodeAccelerator::Config& config,
    const gpu::GpuPreferences& gpu_preferences) {
  if (gpu_preferences.disable_accelerated_video_decode)
    return nullptr;

#if BUILDFLAG(USE_V4L2_CODEC) && \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_ASH))

  std::unique_ptr<VideoDecodeAccelerator> vda;
  vda.reset(new V4L2VideoDecodeAccelerator(new V4L2Device()));

  if (vda->Initialize(config, client)) {
    return vda;
  }
#endif

  return nullptr;
}
}  // namespace media
