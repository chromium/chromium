// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_H_
#define MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_H_

#include <memory>

#include "build/build_config.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_decode_accelerator.h"

namespace gpu {
struct GpuPreferences;
}  // namespace gpu

namespace media {

class MEDIA_GPU_EXPORT GpuVideoDecodeAcceleratorFactory {
 public:
  static std::unique_ptr<VideoDecodeAccelerator> CreateVDA(
      VideoDecodeAccelerator::Client* client,
      const VideoDecodeAccelerator::Config& config,
      const gpu::GpuPreferences& gpu_preferences);
};

}  // namespace media

#endif  // MEDIA_GPU_GPU_VIDEO_DECODE_ACCELERATOR_FACTORY_H_
